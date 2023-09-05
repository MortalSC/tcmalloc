#include "CentralCache.h"


CentralCache CentralCache::_sInst;

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t size) {
	// 计算要申请的内存块在哪个桶
	size_t index = SizeClass::Index(size);
	// 加桶锁限制
	_spanLists[index]._mtx.lock();

	Span* span = GetOneSpan(_spanLists[index], size);

	_spanLists[index]._mtx.unlock();
	return 5;
}


Span* CentralCache::GetOneSpan(SpanList& list, size_t size) {
	return nullptr;
}
