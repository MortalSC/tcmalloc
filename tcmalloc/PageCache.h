#pragma once 

#include "Common.hpp"
#include "ObjectPool.h"


// 单例模式：全局只要一个 PageCache！
class PageCache {
public:
	static PageCache* GetInstance() {
		return &_Instan;
	}

	/*
	*	当CentralCache中对应的span桶中没有可用span（可用空间）时，PageCache分配一个新的！
	*	param:
	*		npage：分割的页数
	*	作用：给CentralCache分配一个页的span
	*/
	Span* GetNewPage(size_t npage);

	/*
	*	查找归还内存块的映射页级Span
	*/
	Span* MapObjectToSpan(void* ptr);

	/*
	*	对归还的Span进行页块整理
	*/
	void ReleaseSpanToPageCache(Span* span);
private:
	SpanList _spanListPage[PAGENUMS];

	// 构建一个页号和Span结构的映射 => 用于内存回收的页合并！
	std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	//// 构建一个页号和内存大小的映射 => 便于内存释放：优化掉之前设计的释放方式（需要给定对象大小）
	//std::unordered_map<PAGE_ID, size_t> _idSizeMap;


	// 使用定长内存池中的自定义内存申请释放替代原生new/malloc
	ObjectPool<Span> spanPool;


public:
	std::mutex _pageMtx;
private:
	PageCache(){}
	PageCache(const PageCache&) = delete;

	static PageCache _Instan;
};