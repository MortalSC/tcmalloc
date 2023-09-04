#ifndef _THREAD_CACHE_H_
#define _THREAD_CACHE_H_

#include "Common.h"



class ThreadCache {
public:
	// 内存管理：申请和释放空间
	/*
	*	内存分配：
	*	param:
	*		需要的内存大小
	*/
	void* Allocate(size_t size);

	/*
	*	内存回收：
	*	param:
	*		被释放的地址
	*		释放的大小
	*/
	void Deallocate(void* free_ptr, size_t size);

private:
	/* 自由链表集合 */
	FreeList _freeListSet[];
};


#endif 