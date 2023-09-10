#include "Common.hpp"
#include "ThreadCache.h"
/*
*	提供两个接口，使得每个线程可以用来申请释放空间，而不是在内部实现再加以封装
*/

static void* Alloc(size_t size) {
	if (TLS_ptr == nullptr) {
		TLS_ptr = new ThreadCache;
	}
	return TLS_ptr->Allocate(size);
}

static void Dealloc(void* free_ptr, size_t size) {
	assert(free_ptr);
	TLS_ptr->Deallocate(free_ptr, size);
}