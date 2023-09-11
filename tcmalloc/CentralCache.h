#include "Common.hpp"

class CentralCache {
public:
	// �����ӿ�
	static CentralCache* GetInstance() {
		return &_Instan;
	}

	/*
	*	��ȡһ���ǿյ� Span
	*		CentralCache��ÿ��Ͱ�еĵ�Ԫ���� Span ����ÿһ��Span������Ͱ��Ӧ��С���ڴ棡
	*		��Span������Ϊ�գ���
	*	param:
	*		spanList������Ӧ�� spanlist
	*		size������Ķ����С => �����Ӧ��Ͱ
	*/
	Span* GetSpanToUse(SpanList& spanList, size_t size);

	/*
	*	��ThreadCache����һ�������Ķ��󣨣�
	*/
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	/*
	*	�ڴ�黹����һ�������Ķ����ͷŵ�span��
	*/
	void ReleaseBlockToSpan(void* start, size_t size);
private:
	SpanList _spanListSet[BUCKETSIZE];
private:
	CentralCache() {};
	CentralCache(const CentralCache&) = delete;

	static CentralCache _Instan;
};