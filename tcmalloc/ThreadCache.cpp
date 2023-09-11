#include "ThreadCache.h"

/*
*	�ڴ����룺ͨ����ϣ�㷨ȥ�ض����ȵġ��ڴ�Ρ����������ȡ�ڴ��
*	�������̣�Ĭ���̻߳��棩��
*		1. ���ݶ����ʵ�ʴ�С => �����С
*		2. ͨ����ϣӳ�� => �ҵ���ȡ�ڴ���Ͱ
*		3. �ж�Ͱ���Ƿ��п����ڴ棺���ޣ�����CentralCache��ȡ��
*	�����ڴ������Լ����
*/
void* ThreadCache::Allocate(size_t size) {

	// ���������ڴ�����
	assert(size <= MAXBTYES);

	// 1. ���� size ��С����С�洢�ڴ�Σ����洢����ʱ���ڴ�ط�����ڴ��С��
	size_t alignSize = SizeClass::RoundUp(size);

	// 2. ��ȡ��Ӧ��ӳ��Ͱ���
	size_t index = SizeClass::Index(size);
	// ���Ի�ȡ������
	std::cout << "size : " << size << " ; alignSize : " << alignSize << " ; index : " << index << std::endl;

	// 3. ��ȡ�ڴ�飡
	// �鿴�̶߳�Ӧ�����������Ƿ��п����ڴ�
	if (!_freeListSet[index].Empty()) {
		// �п����ڴ棺����ͷɾ�� => �����ڴ�
		return _freeListSet[index].Pop();
	}
	else {
		// �޿����ڴ棺��CentralCache��ȡ
		return FatchFromCentralCache(index, size);
	}
	


	//return nullptr;

}

/*
*	�ڴ��ͷţ�ͨ����ϣ�㷨���ڴ��黹��ӳ�䵥Ԫ��
*/
void ThreadCache::Deallocate(void* free_ptr, size_t size) {
	assert(free_ptr);
	assert(size <= MAXBTYES);

	// �ڴ�黹���ǰ��ڴ����뵽�ض���Ͱ��
	size_t index = SizeClass::Index(size);			// ��ȡ��Ͱ������
	_freeListSet[index].Push(free_ptr);				// �������ڴ�����

	// �ж����������ȣ������Ͳ��ֹ黹��CentralCache
	if (_freeListSet[index].GetSize() >= _freeListSet[index].MaxSize()) {
		ReleaseBlockToCentralCache(_freeListSet[index], size);
	}

}

/*
*	�ڴ����� => ����ϣӳ�䵽�����������޿����ڴ�飡�����������ڴ棡
*	param:
*		index����ӦͰ�����
*		size��ʵ�ʶ����С��������ڴ棩
*/
void* ThreadCache::FatchFromCentralCache(size_t index, size_t size) {

	// 1. ʹ����������������ÿ�ε��ڴ������������ֹһ���Է�����࣬��ʵ�������ʵ͵��ڴ��˷�
	size_t limitMaxSize = min(_freeListSet[index].MaxSize(), SizeClass::LimitAllocNum(size));
	
	// 2. �жϻ�ȡ���Լ��ֵ�Ƿ������������������Լ����ͬ������ͬ��������������������������������Լ��ֵ
	if (limitMaxSize == _freeListSet[index].MaxSize()) {
		_freeListSet[index].MaxSize()++;
	}

	// 3. ��ȡ�ڴ�
	void* start = nullptr;
	void* end = nullptr;
	// ��CentralCache�л�ȡ�ڴ���ܶ��󣨼�ʵ�ʻ�ȡ�ĸ�����?��
	// ��������ʼ��ַ����ֹ��ַ����������ȡ���ڴ����Լ����ʵ�ʶ���Ĵ�С������CentrealCache���ҵ�ӳ���Ͱ�����ڻ�ȡ�ڴ棩
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, limitMaxSize, size);
	// start��end ������ͣ����ã���������¼��CentralCache���������ڴ�����ʼ����ֹ��ĵ�ַ��

	assert(actualNum > 0);

	// 4. �ڴ����� ThreadCache ������������
	if (actualNum == 1) {
		// ֻ��ȡ��һ���ڴ棡
		assert(start == end);
		return start;
	}
	else {
		// ��Χ����
		_freeListSet[index].PushRange(GetNextObj(start), end, actualNum - 1);
		return start;
	}

	//return nullptr;
}

/*
*	�ڴ��ͷ� => ����ϣӳ�䵽��������������������¹黹�ڴ棡
*/
void ThreadCache::ReleaseBlockToCentralCache(FreeList& list, size_t size) {
	void* start = nullptr;
	void* end = nullptr;		// ���ڼ�¼��ʾ��β

	list.PopRange(start, end, list.MaxSize());	// ��Χ�Դ� freeList �м����ڴ��
	// start��end ������Ͳ��������ã�
	
	CentralCache::GetInstance()->ReleaseBlockToSpan(start, size);
}