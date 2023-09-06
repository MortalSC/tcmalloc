#ifndef _THREAD_CACHE_H_
#define _THREAD_CACHE_H_

#include "Common.h"



class ThreadCache {
public:
	//ThreadCache() {}
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

	/*
	*	内存块申请（向下层）
	*/
	void* FetchFromContralCache(size_t index, size_t size);

	/*
	*	处理归还内存中出现的内存过多，取出一部分向下递交！
	*/
	void ListTooLong(FreeList& list, size_t size);

private:
	/* 自由链表集合 */
	FreeList _freeListSet[MAXBLUCKET];
};


/* 创建 TLS（线程局部存储） */
// 使得每一个线程都有自己的 pTLSThreadCache，
// 尽管是全局声明的同名变量，但是实际多线程使用时都会有自己的维护
// 使用该方式：实现每个线程获取的自己
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;


#endif 