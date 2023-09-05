#ifndef _CENTRAL_CACHE_H_
#define _CENTRAL_CACHE_H_

#include "Common.h"

// 使用单例！
class CentralCache {
public:
	static CentralCache* GetInstance() {
		return &_sInst;
	}

	// 获取一个非空的span
	Span* GetOneSpan(SpanList& list, size_t size);

	// 从中心缓存获取一定数量的内存对象给threadcache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t size);



private:
	SpanList _spanLists[MAXBLUCKET];
private:
	CentralCache(){}
	CentralCache(const CentralCache& ctc) = delete;

	static CentralCache _sInst;
};


#endif