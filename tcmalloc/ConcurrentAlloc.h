#pragma once

#include "Common.h"
#include "ThreadCache.h"

/*
*	提供两个接口，使得每个线程可以用来申请释放空间，而不是在内部实现再加以封装
*/

static void* ConcurrentAlloc(size_t size) {
	if (pTLSThreadCache == nullptr) {
		pTLSThreadCache = new ThreadCache;
	}

	//std::cout << std::this_thread::get_id() << " : " << pTLSThreadCache << std::endl;
	return pTLSThreadCache->Allocate(size);
}

static void ConcurrentFree(void* ptr, size_t size) {
	assert(pTLSThreadCache);
	pTLSThreadCache->Deallocate(ptr, size);
}