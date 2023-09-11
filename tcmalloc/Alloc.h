#include "Common.hpp"
#include "ThreadCache.h"
#include "PageCache.h"
/*
*	�ṩ�����ӿڣ�ʹ��ÿ���߳̿������������ͷſռ䣬���������ڲ�ʵ���ټ��Է�װ
*/

static void* Alloc(size_t size) {

	if(size <= MAXBTYES){					// С�� 256kb ���ڴ�����
		if (TLS_ptr == nullptr) {

			//TLS_ptr = new ThreadCache;
			static ObjectPool<ThreadCache> tcPool;		// �滻ԭ�� new
			TLS_ptr = tcPool.New();

		}
		return TLS_ptr->Allocate(size);
	}
	else {									// ���� 256kb ʱ��
		// 256kb / 8kb = 32 page������ֱ���� PageCache ���룡
		// Լ����128 page * 8kb = 1024kb������1024kb����PageCache���������ڴ�ҳ
		// ���������Σ�
		// ����һ��256kb <= size <= 1024kb ������ȥPageCache�����ڴ����룡
		// ���ζ���size > 1024kb��ȥ�����룡
		
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kpage = alignSize >> PAGE_SHIFT;
		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->GetNewPage(kpage);
		span->_objSize = size;
		PageCache::GetInstance()->_pageMtx.unlock();                          
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		return ptr;

		//if (size <= 1024 * 1024) {
		//	// ����һ��256kb <= size <= 1024kb
		//}
		//else {
		//	// ���ζ���size > 1024kb
		//}
	}
}

//static void Dealloc(void* free_ptr, size_t size) 
/*
	�Ż���ʽһ�������ڴ�ҳ�źͶ����С��ӳ�䣨PageCache.h ��36�У�
	�Ż���ʽ������Span��ֱ�����һ������¼��С
*/
static void Dealloc(void* free_ptr)		// �Ľ������贫�ݴ�С
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