#include "Common.hpp"


class CentralCache {
public:
	// 单例接口
	static CentralCache* GetInstance() {
		return &_Instan;
	}

	/*
	*	获取一个非空的 Span
	*		CentralCache下每个桶中的单元就是 Span 链表，每一个Span挂载着桶对应的小块内存！
	*		（Span结点可能为空！）
	*	param:
	*		spanList：所对应的 spanlist
	*		size：申请的对象大小 => 计算对应的桶
	*/
	Span* GetSpanToUse(SpanList& spanList, size_t size);

	/*
	*	给ThreadCache分配一定数量的对象（）
	*/
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	/*
	*	内存归还：将一定数量的对象释放到span中
	*/
	void ReleaseBlockToSpan(void* start, size_t size);
private:
	SpanList _spanListSet[BUCKETSIZE];
private:
	CentralCache() {};
	CentralCache(const CentralCache&) = delete;

	static CentralCache _Instan;
};