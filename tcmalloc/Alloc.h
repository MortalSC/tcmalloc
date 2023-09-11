#include "Common.hpp"
#include "ThreadCache.h"
#include "PageCache.h"
/*
*	提供两个接口，使得每个线程可以用来申请释放空间，而不是在内部实现再加以封装
*/

static void* Alloc(size_t size) {

	if(size <= MAXBTYES){					// 小于 256kb 的内存申请
		if (TLS_ptr == nullptr) {

			//TLS_ptr = new ThreadCache;
			static ObjectPool<ThreadCache> tcPool;		// 替换原生 new
			TLS_ptr = tcPool.New();

		}
		return TLS_ptr->Allocate(size);
	}
	else {									// 大于 256kb 时！
		// 256kb / 8kb = 32 page，可以直接找 PageCache 申请！
		// 约束：128 page * 8kb = 1024kb，大于1024kb超出PageCache管理的最大内存页
		// 故两种情形！
		// 情形一：256kb <= size <= 1024kb ，可以去PageCache进行内存申请！
		// 情形二：size > 1024kb，去堆申请！
		
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kpage = alignSize >> PAGE_SHIFT;
		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->GetNewPage(kpage);
		span->_objSize = size;
		PageCache::GetInstance()->_pageMtx.unlock();                          
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		return ptr;

		//if (size <= 1024 * 1024) {
		//	// 情形一：256kb <= size <= 1024kb
		//}
		//else {
		//	// 情形二：size > 1024kb
		//}
	}
}

//static void Dealloc(void* free_ptr, size_t size) 
/*
	优化方式一：构建内存页号和对象大小的映射（PageCache.h ：36行）
	优化方式二：在Span中直接添加一个量记录大小
*/
static void Dealloc(void* free_ptr)		// 改进成无需传递大小
{
	assert(free_ptr);
	Span* span = PageCache::GetInstance()->MapObjectToSpan(free_ptr);
	size_t size = span->_objSize;
	if (size > MAXBTYES) {
		Span* span = PageCache::GetInstance()->MapObjectToSpan(free_ptr);
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}else{
		TLS_ptr->Deallocate(free_ptr, size);
	}

}