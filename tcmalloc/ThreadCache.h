#pragma once

#include "Common.hpp"
#include "CentralCache.h"

/* ThreadCache 对象 */
class ThreadCache {
public:
	/*
	*	内存申请：通过哈希算法去特定长度的“内存段”自由链表获取内存块
	*/
	void* Allocate(size_t size);

	/*
	*	内存释放：通过哈希算法将内存块归还到映射单元中
	*/
	void Deallocate(void* free_ptr, size_t size);

	/*
	*	内存申请 => 若哈希映射到的自由链表，无可用内存块！则向下申请内存！
	*/
	void* FatchFromCentralCache(size_t index, size_t size);

	/*
	*	内存释放 => 若哈希映射到的自由链表过长，则向下归还内存！
	*/
	void ReleaseBlockToCentralCache(FreeList& list, size_t size);
private:
	// 映射哈希桶【元素：自由链表】
	FreeList _freeListSet[BUCKETSIZE];
};


// 定义线程局域变量（每一个线程都会具有！且互不干扰！）
static thread_local ThreadCache* TLS_ptr = nullptr;