#pragma once 

#include "Common.hpp"
#include "ObjectPool.h"


// ����ģʽ��ȫ��ֻҪһ�� PageCache��
class PageCache {
public:
	static PageCache* GetInstance() {
		return &_Instan;
	}

	/*
	*	��CentralCache�ж�Ӧ��spanͰ��û�п���span�����ÿռ䣩ʱ��PageCache����һ���µģ�
	*	param:
	*		npage���ָ��ҳ��
	*	���ã���CentralCache����һ��ҳ��span
	*/
	Span* GetNewPage(size_t npage);

	/*
	*	���ҹ黹�ڴ���ӳ��ҳ��Span
	*/
	Span* MapObjectToSpan(void* ptr);

	/*
	*	�Թ黹��Span����ҳ������
	*/
	void ReleaseSpanToPageCache(Span* span);
private:
	SpanList _spanListPage[PAGENUMS];

	// ����һ��ҳ�ź�Span�ṹ��ӳ�� => �����ڴ���յ�ҳ�ϲ���
	std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	//// ����һ��ҳ�ź��ڴ��С��ӳ�� => �����ڴ��ͷţ��Ż���֮ǰ��Ƶ��ͷŷ�ʽ����Ҫ���������С��
	//std::unordered_map<PAGE_ID, size_t> _idSizeMap;


	// ʹ�ö����ڴ���е��Զ����ڴ������ͷ����ԭ��new/malloc
	ObjectPool<Span> spanPool;


public:
	std::mutex _pageMtx;
private:
	PageCache(){}
	PageCache(const PageCache&) = delete;

	static PageCache _Instan;
};