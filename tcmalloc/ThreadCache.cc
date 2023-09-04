#include "ThreadCache.h"


void* ThreadCache::Allocate(size_t size) {
	/* 申请的限制：不超过 256KB */
	assert(size <= MAX_BYTES);
}

void ThreadCache::Deallocate(void* free_ptr, size_t size) {

}