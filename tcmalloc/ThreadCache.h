#pragma once

#include "Common.hpp"
#include "CentralCache.h"

/* ThreadCache ���� */
class ThreadCache {
public:
	/*
	*	�ڴ����룺ͨ����ϣ�㷨ȥ�ض����ȵġ��ڴ�Ρ����������ȡ�ڴ��
	*/
	void* Allocate(size_t size);

	/*
	*	�ڴ��ͷţ�ͨ����ϣ�㷨���ڴ��黹��ӳ�䵥Ԫ��
	*/
	void Deallocate(void* free_ptr, size_t size);

	/*
	*	�ڴ����� => ����ϣӳ�䵽�����������޿����ڴ�飡�����������ڴ棡
	*/
	void* FatchFromCentralCache(size_t index, size_t size);

	/*
	*	�ڴ��ͷ� => ����ϣӳ�䵽��������������������¹黹�ڴ棡
	*/
	void ReleaseBlockToCentralCache(FreeList& list, size_t size);
private:
	// ӳ���ϣͰ��Ԫ�أ���������
	FreeList _freeListSet[BUCKETSIZE];
};


// �����߳̾��������ÿһ���̶߳�����У��һ������ţ���
static thread_local ThreadCache* TLS_ptr = nullptr;