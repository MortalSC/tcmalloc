#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t size) {
	// 计算要申请的内存块在哪个桶
	size_t index = SizeClass::Index(size);
	// 加桶锁限制
	_spanLists[index]._mtx.lock();

	// 从 span 中获取actualNum个内存块！
	// 如果不够！有几个拿几个！
	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_next); 
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1;
	while( i < n -1 && GetNextObj(end) != nullptr ){
		end = GetNextObj(end);
		++i;
		++actualNum;

	}
	span->_freeList = GetNextObj(end);
	GetNextObj(end) = nullptr;
	span->_useCount += actualNum;

	_spanLists[index]._mtx.unlock();
	return 5;
}

// 遍历特定桶中的所有span，顺序找到一个含有可分配内存的对象返回
Span* CentralCache::GetOneSpan(SpanList& list, size_t size) {

	Span* iter = list.Begin();
	while (iter != list.End()) {
		if (iter->_freeList != nullptr) {
			return iter;
		}
		else {
			iter = iter->_next;
		}
	}

	// 走到此处，说明特定的桶中没有可用资源！需要向下层 page cache 申请

	// 注意：此处推荐解锁！
	// 如果 x1，x2 线程先后访问此处，x1 来了没有资源可用，则向下申请
	// 由于 x1 是持有锁的！后续来的 x2 ，无论是否有资源都无法向后走！！！
	// 【有资源的情况就是：恰好x1向下申请到x2到来期间，有资源归还】
	// 因此，推荐把 x1 的所释放掉，让后续的可以执行
	list._mtx.unlock();

	// 向下申请中只允许一个线程同时去执行！故加锁！
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->isUse = true;
	PageCache::GetInstance()->_pageMtx.unlock();

	// 以下是已经获取到了 span 并进行切分处理，无需锁【无资源竞争：其他线程无法访问到（没有挂载到 list 中）】
	// 计算span的大块内存的起始地址和大块内存的大小（字节）
	char* start = (char*)(span->_page_id << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;

	char* end = start + bytes;

	// 把大块内存切成自由链表中的小内存链接起来
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	while (start < end) {
		GetNextObj(tail) = start;
		tail = GetNextObj(tail);
		start += size;
	}

	// 头插入 central cache 特定的桶集合中
	// 插入过程（桶的修改）可能存在有其他线程的同时操作（归还资源），必须加锁
	list._mtx.lock();
	list.PushFront(span);


	return span;
}


void CentralCache::ReleaseListToSpans(void* start, size_t size) {
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();

	//while (start != nullptr) {
	//	void* next = GetNextObj(start);
	//	Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
	//	GetNextObj(start) = span->_freeList;
	//	span->_freeList = start;
	//	span->_useCount--;
	//	if (span->_useCount == 0) {
	//		// 说明该 span 的所有小内存对象都归还回来了！ => 向下归还给 page cache（可以尝试前后页的合并）

	//		_spanLists[index].Erase(span);
	//		span->_freeList = nullptr;
	//		span->_next = nullptr;
	//		span->_prev = nullptr;

	//		_spanLists[index]._mtx.unlock();

	//		PageCache::GetInstance()->_pageMtx.lock();
	//		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
	//		PageCache::GetInstance()->_pageMtx.unlock();

	//		_spanLists[index]._mtx.lock();

	//	}
	//	start = next;
	//}

	_spanLists[index]._mtx.unlock();
}

