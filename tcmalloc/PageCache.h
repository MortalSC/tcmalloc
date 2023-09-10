#pragma once 

#include "Common.hpp"

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

public:
	std::mutex _pageMtx;
private:
	PageCache(){}
	PageCache(const PageCache&) = delete;
	static PageCache _Instan;
};