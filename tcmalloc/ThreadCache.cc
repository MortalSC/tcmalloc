#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::Allocate(size_t size) {
	/* 申请的限制：不超过 256KB */
	assert(size <= MAX_BYTES);
	/* 获取对齐后所分配的空间大小 */
	size_t alignSize = SizeClass::RoundUp(size);
	/* 计算对应的桶 */
	size_t index = SizeClass::Index(size);

	/* 
	*	获取内存资源的策略：符合 ThreadCache 限制时，优先通过哈希映射查看是否存在可用内存块
	*/
	if (!_freeListSet[index].Empty()) {
		// 非空：有可用内存块！
		return _freeListSet[index].Pop();
	}
	else {
		// 空：无可用内存块！
		// 需要主动申请（向 contral cache 获取！）
		return FetchFromContralCache(index, alignSize);
	}
}

void ThreadCache::Deallocate(void* free_ptr, size_t size) {
	assert(free_ptr);
	assert(size <= MAX_BYTES);
	size_t index = SizeClass::Index(size);
	_freeListSet[index].Push(free_ptr);
}

void* ThreadCache::FetchFromContralCache(size_t index, size_t size) {
	// 慢开始策略：第一次多分配（少量）
	size_t batchSize = std::min(_freeListSet[index].MaxSize(), SizeClass::NumMoveSize(size));

	//慢开始反馈调节算法
	// 1、最开始不会一次向central cache一次批量要太多，因为要太多了可能用不完
	// 2、如果你不要这个size大小内存需求，那么batchNum就会不断增长，直到上限
	// 3、size越大，一次向central cache要的batchNum就越小
	// 4、size越小，一次向central cache要的batchNum就越大
	if (batchSize == _freeListSet[index].MaxSize()) {
		_freeListSet[index].MaxSize() += 1;
	}

	void* start = nullptr;
	void* end = nullptr;
	size_t actualSize = CentralCache::GetInstance()->FetchRangeObj(start, end, batchSize, size);
	assert(actualSize > 1);
	if (actualSize == 1) {
		assert(start == end);
		//return start;
	}
	else {
		_freeListSet[index].PushRange(GetNextObj(start), end);
		//return start;
	}

	return start;
}