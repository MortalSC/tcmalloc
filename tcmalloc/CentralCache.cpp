#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_Instan;

/*
*   返回具体参与分配的span内存对象
*/
Span* CentralCache::GetSpanToUse(SpanList& spanList, size_t size)
{
    // 1. 找到一个非空的 Span（有可分配的内存）：遍历是查找
    Span* iter = spanList.Begin();
    while(iter != spanList.End()) {
        if (iter->_freeList != nullptr) {
            // 遇到非空：即有可分配资源
            return iter;
        }
        else {
            iter = iter->_next;
        }
    }

    // 到此位置，即：CentralCache对应的桶中也没有内存资源！需要向PageCache获取！
    // 注意：当前桶是被加锁保护的！
    // 必须解除！因为当前线程向PageCache申请的过程中，可能存在其他线程归还对应桶资源的情况！
    spanList._mtx.unlock();

    // 2. 向PageCache申请内存！
    //std::cout << "执行到此处" << std::endl;

    // 2.1 对于页的操作进行（页）全局加锁！
    PageCache::GetInstance()->_pageMtx.lock();
    Span* span = PageCache::GetInstance()->GetNewPage(SizeClass::GetToPageNum(size));
    span->isUse = true;
    span->_objSize = size;              // 优化内存释放时的内存大小参数传递（去除）
    // 注：从PageCache获取的span是“页级”（没有规划出特定的【ThreadCache】内存块单元）
    PageCache::GetInstance()->_pageMtx.unlock();

    // 3. 对获取到的 span 进行切片
    // 对获取span进行切分，不需要加锁，因为这会其他线程访问不到这个span

    // 3.1 计算span的大块内存的起始地址和大块内存的大小(字节数)
    char* start = (char*)(span->_pageID << PAGE_SHIFT);     // 通过 span 的ID换算出起始地址
    size_t bytes = span->_n << PAGE_SHIFT;                  // 通过 页号 来换算出块的大小
    char* end = start + bytes;                              // 获取到终止地址

    // 3.2 执行切片
    span->_freeList = start;
    start += size;                                          // 按字节，使用头部特定的大小作为头（记录第一个结点的地址）
    void* tail = span->_freeList;                           // 游标指针：执行切片插入
    int i = 1;
    while (start < end) {
        i++;
        GetNextObj(tail) = start;
        tail = GetNextObj(tail);  // 等价于：tail = start，即指针向后便宜一个单位！
        start += size;
    }

    // 切好 span 以后，需要把 span 挂到桶里面去的时候，再加锁
    spanList._mtx.lock();
    spanList.PushFront(span);

    return span;
}

/*
*   接收来自ThreadCache的内存请求！按需分配内存！
*   注意：CentralCache为了避免多线程下的内存安全问题：使用锁（桶锁：对被访问的桶进行加锁保护）
*   步骤：
*       1. 通过哈希映射 => 找到内存块对应的桶
*       2. 在对应的桶中获取资源（临界资源）
*   参数：batchNum：即要获取的内存单元个数
*/
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
    // 1. 找到对应的内存块源
    size_t index = SizeClass::Index(size);

    // 2. 加锁保护资源访问
    _spanListSet[index]._mtx.lock();

    // 3. 获取一个 Span 对象从中获取内存块
    Span* span = GetSpanToUse(_spanListSet[index], size);
    assert(span);               // 防止返回的 Span 有问题
    assert(span->_freeList);    // 防止返回的 Span 有问题

    start = span->_freeList;    // 指向起始点
    end = start;
    size_t i = 0;               
    size_t actualNum = 1;       // 实际分配的个数！Span中的个数可能不够！
    while (i < batchNum && GetNextObj(end) != nullptr) {
        // i < batchNum：控制分配的最大数量
        // GetNextObj(end) != nullptr：防止分配数量不够，继续分配造成的内存泄露！
        // 此处可能有两种情况！
        //      （1）已经满足了申请对象的内存需求，但是为满足富余分配策略的内存需求
        //      （2）不满足了申请对象的内存需求
        // 防止以上两种问题的出现，即：actualNum 的作用！
        end = GetNextObj(end);      // 等价于：链表中 node = node->_next;
        i++;
        actualNum++;
    }

    span->_freeList = GetNextObj(end);
    GetNextObj(end) = nullptr;
    span->_useCount += actualNum;   
    // _useCount：
    // 作用：记录了span中被分配出去的内存块个数（切好小块内存，被分配给thread cache的计数）

    // 4. 解锁
    _spanListSet[index]._mtx.unlock();

    return actualNum;
}

void CentralCache::ReleaseBlockToSpan(void* start, size_t size)
{
    // 1. 查找对应的桶
    size_t index = SizeClass::Index(size);

    // 2. 加锁归还
    _spanListSet[index]._mtx.lock();

    while (start) {
        // 2.1 记录当前内存块所记载的下一块内存块的地址！
        void* next = GetNextObj(start);

        // 2.2 找到归还内存块的所属页Span
        Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

        GetNextObj(start) = span->_freeList;
        span->_freeList = start;
        span->_useCount--;                      // 更新使用登记数

        // 2.3 注意判断是否全部归还，若全部归还可以进行页合并
        if (span->_useCount == 0) {
            _spanListSet[index].Erase(span);    // 进入到此，说明span的整页已经集齐！故：抛出归还给PageCahce
            // 资源分离（分理出临界区）
            span->_freeList = nullptr;
            span->_prev = nullptr;
            span->_next = nullptr;

            // span 已经不是临界区资源了！
            _spanListSet[index]._mtx.unlock();
            PageCache::GetInstance()->_pageMtx.lock();
            PageCache::GetInstance()->ReleaseSpanToPageCache(span);     // 交给PageCache进行页整理！
            PageCache::GetInstance()->_pageMtx.unlock();
        }

        start = next;
    }

    _spanListSet[index]._mtx.unlock();

}
