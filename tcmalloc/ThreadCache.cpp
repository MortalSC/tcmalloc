#include "ThreadCache.h"

/*
 *	内存申请：通过哈希算法去特定长度的“内存段”自由链表获取内存块
 *	申请流程（默认线程缓存）：
 *		1. 根据对象的实际大小 => 对齐大小
 *		2. 通过哈希映射 => 找到获取内存块的桶
 *		3. 判断桶中是否有可用内存：若无：则向CentralCache获取！
 *	对于内存的申请约束：
 */
void *ThreadCache::Allocate(size_t size)
{

	// 申请的最大内存限制
	// assert(size <= MAXBTYES);

	if (size <= MAXBTYES)
	{
		// 1. 计算 size 大小的最小存储内存段（即存储对象时，内存池分配的内存大小）
		size_t alignSize = SizeClass::RoundUp(size);

		// 2. 获取对应的映射桶编号
		size_t index = SizeClass::Index(size);
		// 测试获取的数据
		std::cout << "size : " << size << " ; alignSize : " << alignSize << " ; index : " << index << std::endl;

		// 3. 获取内存块！
		// 查看线程对应的自由链表是否有可用内存
		if (!_freeListSet[index].Empty())
		{
			// 有可用内存：链表头删法 => 分配内存
			return _freeListSet[index].Pop();
		}
		else
		{
			// 无可用内存：向CentralCache获取
			FatchFromCentralCache(index, size);
		}
	}

	return nullptr;
}

/*
 *	内存释放：通过哈希算法将内存块归还到映射单元中
 */
void ThreadCache::Deallocate(void *free_ptr, size_t size)
{
	assert(free_ptr);
	assert(size <= MAXBTYES);

	// 内存归还就是把内存块插入到特定的桶中
	size_t index = SizeClass::Index(size); // 获取到桶的索引
	_freeListSet[index].Push(free_ptr);	   // 将空闲内存块插入
}

/*
 *	内存申请 => 若哈希映射到的自由链表，无可用内存块！则向下申请内存！
 *	param:
 *		index：对应桶的序号
 *		size：实际对象大小（需求的内存）
 */
void *ThreadCache::FatchFromCentralCache(size_t index, size_t size)
{

// 1. 使用慢增长策略限制每次的内存分配数量，防止一次性分配过多，但实际利用率低的内存浪费
#ifdef _WIN32
	size_t limitMaxSize = min(_freeListSet[index].MaxSize(), SizeClass::LimitAllocNum(size));
#else
	size_t limitMaxSize = std::min(_freeListSet[index].MaxSize(), SizeClass::LimitAllocNum(size));

#endif
	// 2. 判断获取大的约束值是否和自由链表的最大增长约束相同，若相同，则慢增长：提升自由链表的最大增长约束值
	if (limitMaxSize == _freeListSet[index].MaxSize())
	{
		_freeListSet[index].MaxSize()++;
	}

	// 3. 获取内存
	void *start = nullptr;
	void *end = nullptr;
	// 从CentralCache中获取内存才能对象（及实际获取的个数：?）
	// 参数：起始地址、终止地址、慢增长获取的内存块数约束、实际对象的大小（用于CentrealCache层找到映射的桶：便于获取内存）
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, limitMaxSize, size);
	// start、end 是输出型（引用）参数，记录了CentralCache分配来的内存块的起始和终止块的地址！

	assert(actualNum > 0);

	// 4. 内存载入 ThreadCache 的自由链表中
	if (actualNum == 1)
	{
		// 只获取到一块内存！
		assert(start == end);
		return start;
	}
	else
	{
		// 范围插入
		_freeListSet[index].PushRange(GetNextObj(start), end, actualNum - 1);
		return start;
	}

	// return nullptr;
}

/*
 *	内存释放 => 若哈希映射到的自由链表过长，则向下归还内存！
 */
void ThreadCache::ListTooLong(FreeList &list, size_t size)
{
}