#include "PageCache.h"

PageCache PageCache::_Instan;

/*
*	���ã��ӿ���ҳ�л�ȡ�ڴ�
*/
Span* PageCache::GetNewPage(size_t npage)
{
	assert(npage > 0 && npage < PAGENUMS);

	// 1. �ж�ָ����ҳ���Ƿ��п����ڴ棡
	// ��û�У���˳��鿴��һҳֱ�������ǿ�ҳ / ���һҳ���޿����ڴ棬������룩
	if (!_spanListPage[npage].Empty()) {
		return _spanListPage->PopFront();
	}

	// 2. ˳��鿴��һҳֱ�������ǿ�ҳ / ���һҳ���޿����ڴ棬������룩
	for (size_t i = npage + 1; i < PAGENUMS; i++) {
		if (!_spanListPage[i].Empty()) {
			// �����ǿ�ҳ��
			
			// ҳ�ϵ��ڴ�������򣺸�ҳ�ֳ������֣�
			// ��һ���֣��������ʱ�����ҳ��С + ʣ���С����ǰ���ڵ�PageCache�е��ض�ҳ�ϣ�
			Span* prevSpan = _spanListPage[i].PopFront();		// ��ȡ��ҳ���ڴ��
			Span* kSpan = new Span;

			// ��prevSpan��ͷ����һ�� npage ҳ����
			// npage ҳ npage ����
			// prevSpan�ٹҵ���Ӧӳ���λ��

			// ��ȡ��ҳ ID
			kSpan->_pageID = prevSpan->_pageID;					// ������ͬһҳ�У��ָ����� span ��Ҫ��¼ҳid
			kSpan->_n = npage;									// ��¼ҳ��

			prevSpan->_pageID += npage;							// ���ҳid�����ϵ�����������128ҳ��ҳid����Сֵ��
			prevSpan->_n -= npage;

			_spanListPage[prevSpan->_n].PushFront(prevSpan);	// ��ҳ����һ���֣���һ��ʼ�Ŀ�ҳ���в��䡾��ͼ��������⡿��

			// �洢 prevSpan ����λҳ�Ÿ� prevSpan ӳ�䣬���� page cache �����ڴ�ʱ
			// ���еĺϲ����ң�����¼�˴��ҳ�ķ���ȥ�򣨱��ڻ��պϲ���
			_idSpanMap[prevSpan->_pageID] = prevSpan;
			_idSpanMap[prevSpan->_pageID + prevSpan->_n - 1] = prevSpan;


			// ����id��span��ӳ�䣬����central cache����С���ڴ�ʱ�����Ҷ�Ӧ��span
			for (PAGE_ID i = 0; i < kSpan->_n; ++i)
			{
				_idSpanMap[kSpan->_pageID + i] = kSpan;
			}

			return kSpan;
		}
	}

	// 3. ����λ�ã�����ֱ�����һҳҳû�п��ÿռ䣬��Ҫ����ͬ�����µĴ�ҳ��ȥ�Ҷ�Ҫһ��128ҳ��span
	Span* bigSpan = new Span;
	void* ptr = SystemAlloc(PAGENUMS - 1);
	bigSpan->_pageID = (PAGE_ID)ptr >> PAGE_SHIFT;		// ���� Span ��ҳ ID ��
	// ���㷽����ͨ����ַ��ֵ�� / ҳ��С������λ��8kb����ȡ�����ҳ�ţ�
	bigSpan->_n = PAGENUMS - 1;							// �»�ȡ��ҳ��һ���ڵ� 128 ҳ

	_spanListPage[bigSpan->_n].PushFront(bigSpan);		// ����ҳ����ӳ�䡰���ݡ�Ͱ��

	return GetNewPage(npage);
}

Span* PageCache::MapObjectToSpan(void* ptr)
{
	PAGE_ID id = ((PAGE_ID)ptr >> PAGE_SHIFT);	// ��ȡҳid
	auto ret = _idSpanMap.find(id);				
	// ͨ��ӳ���ϵ�ҵ���Ӧ��Span
	if (ret != _idSpanMap.end()) {
		// �ҵ���ӳ�����
		return ret->second;						// ����ӳ���ҵ��� Span
	}
	else {
		//û���ҵ��������ϲ����ܳ���
		assert(false);
		return nullptr;
	}

	return nullptr;
}

/*
*	ҳ����˼·��
*		ͨ��ǰ�����ѣ�����ҳid
*/

void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// ��ǰ�ϲ���
	while (1) {
		PAGE_ID prevID = span->_pageID - 1;		// ��ǰ����ҳID
		//��ҳID���Ǹ��ݵ�ַ���趨��ҳ�̶���̶���С�������á�
		// -1��������ǰ�ƶ�һ��ҳ�̶����С��Ԫ

		// ��ӳ���в����Ƿ���ڣ�
		auto ret = _idSpanMap.find(prevID);
		if (ret == _idSpanMap.end()) break;		// û�ҵ���˵��ǰ���ڴ�

		// �ߵ��˴���˵����������ҳ����ϱ�ʹ��״̬ȷ���Ƿ�ɺϲ�
		Span* prevSpan = ret->second;			// ��ȡ�� Span
		if (prevSpan->isUse == true) break;
		// ˵��ǰ����ڴ���ڣ������ڱ�ʹ��״̬ => ���ܺϲ���

		// ע�⣺�жϺϲ���ҳ��С������128ҳ�Ͳ��ܺϲ���
		if (prevSpan->_n + span->_n > PAGENUMS) break;

		// ���ˣ�����ʵ��ҳ�ϲ���
		span->_pageID = prevSpan->_pageID;		// ����ҳID
		span->_n += prevSpan->_n;				// ����ҳ��С

		_spanListPage[prevSpan->_n].Erase(prevSpan);
		delete prevSpan;

	}

	// ���ϲ�
	while (1)
	{
		PAGE_ID nextId = span->_pageID + span->_n;
		auto ret = _idSpanMap.find(nextId);
		if (ret == _idSpanMap.end())
		{
			break;
		}

		Span* nextSpan = ret->second;
		if (nextSpan->isUse == true)
		{
			break;
		}

		if (nextSpan->_n + span->_n > PAGENUMS - 1)
		{
			break;
		}

		span->_n += nextSpan->_n;

		_spanListPage[nextSpan->_n].Erase(nextSpan);
		delete nextSpan;
	}


	_spanListPage[span->_n].PushFront(span);
	span->isUse = false;
	_idSpanMap[span->_pageID] = span;
	_idSpanMap[span->_pageID + span->_n - 1] = span;
}
