#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_Instan;

/*
*   ���ؾ����������span�ڴ����
*/
Span* CentralCache::GetSpanToUse(SpanList& spanList, size_t size)
{
    // 1. �ҵ�һ���ǿյ� Span���пɷ�����ڴ棩�������ǲ���
    Span* iter = spanList.Begin();
    while(iter != spanList.End()) {
        if (iter->_freeList != nullptr) {
            // �����ǿգ����пɷ�����Դ
            return iter;
        }
        else {
            iter = iter->_next;
        }
    }

    // ����λ�ã�����CentralCache��Ӧ��Ͱ��Ҳû���ڴ���Դ����Ҫ��PageCache��ȡ��
    // ע�⣺��ǰͰ�Ǳ����������ģ�
    // ����������Ϊ��ǰ�߳���PageCache����Ĺ����У����ܴ��������̹߳黹��ӦͰ��Դ�������
    spanList._mtx.unlock();

    // 2. ��PageCache�����ڴ棡
    //std::cout << "ִ�е��˴�" << std::endl;

    // 2.1 ����ҳ�Ĳ������У�ҳ��ȫ�ּ�����
    PageCache::GetInstance()->_pageMtx.lock();
    Span* span = PageCache::GetInstance()->GetNewPage(SizeClass::GetToPageNum(size));
    span->isUse = true;
    // ע����PageCache��ȡ��span�ǡ�ҳ������û�й滮���ض��ġ�ThreadCache���ڴ�鵥Ԫ��
    PageCache::GetInstance()->_pageMtx.unlock();

    // 3. �Ի�ȡ���� span ������Ƭ
    // �Ի�ȡspan�����з֣�����Ҫ��������Ϊ��������̷߳��ʲ������span

    // 3.1 ����span�Ĵ���ڴ����ʼ��ַ�ʹ���ڴ�Ĵ�С(�ֽ���)
    char* start = (char*)(span->_pageID << PAGE_SHIFT);     // ͨ�� span ��ID�������ʼ��ַ
    size_t bytes = span->_n << PAGE_SHIFT;                  // ͨ�� ҳ�� ���������Ĵ�С
    char* end = start + bytes;                              // ��ȡ����ֹ��ַ

    // 3.2 ִ����Ƭ
    span->_freeList = start;
    start += size;                                          // ���ֽڣ�ʹ��ͷ���ض��Ĵ�С��Ϊͷ����¼��һ�����ĵ�ַ��
    void* tail = span->_freeList;                           // �α�ָ�룺ִ����Ƭ����
    int i = 1;
    while (start < end) {
        i++;
        GetNextObj(tail) = start;
        tail = GetNextObj(tail);  // �ȼ��ڣ�tail = start����ָ��������һ����λ��
        start += size;
    }

    // �к� span �Ժ���Ҫ�� span �ҵ�Ͱ����ȥ��ʱ���ټ���
    spanList._mtx.lock();
    spanList.PushFront(span);

    return span;
}

/*
*   ��������ThreadCache���ڴ����󣡰�������ڴ棡
*   ע�⣺CentralCacheΪ�˱�����߳��µ��ڴ氲ȫ���⣺ʹ������Ͱ�����Ա����ʵ�Ͱ���м���������
*   ���裺
*       1. ͨ����ϣӳ�� => �ҵ��ڴ���Ӧ��Ͱ
*       2. �ڶ�Ӧ��Ͱ�л�ȡ��Դ���ٽ���Դ��
*   ������batchNum����Ҫ��ȡ���ڴ浥Ԫ����
*/
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
    // 1. �ҵ���Ӧ���ڴ��Դ
    size_t index = SizeClass::Index(size);

    // 2. ����������Դ����
    _spanListSet[index]._mtx.lock();

    // 3. ��ȡһ�� Span ������л�ȡ�ڴ��
    Span* span = GetSpanToUse(_spanListSet[index], size);
    assert(span);               // ��ֹ���ص� Span ������
    assert(span->_freeList);    // ��ֹ���ص� Span ������

    start = span->_freeList;    // ָ����ʼ��
    end = start;
    size_t i = 0;               
    size_t actualNum = 1;       // ʵ�ʷ���ĸ�����Span�еĸ������ܲ�����
    while (i < batchNum && GetNextObj(end) != nullptr) {
        // i < batchNum�����Ʒ�����������
        // GetNextObj(end) != nullptr����ֹ������������������������ɵ��ڴ�й¶��
        // �˴����������������
        //      ��1���Ѿ����������������ڴ����󣬵���Ϊ���㸻�������Ե��ڴ�����
        //      ��2�������������������ڴ�����
        // ��ֹ������������ĳ��֣�����actualNum �����ã�
        end = GetNextObj(end);      // �ȼ��ڣ������� node = node->_next;
        i++;
        actualNum++;
    }

    span->_freeList = GetNextObj(end);
    GetNextObj(end) = nullptr;
    span->_useCount += actualNum;   
    // _useCount��
    // ���ã���¼��span�б������ȥ���ڴ��������к�С���ڴ棬�������thread cache�ļ�����

    // 4. ����
    _spanListSet[index]._mtx.unlock();

    return actualNum;
}

void CentralCache::ReleaseBlockToSpan(void* start, size_t size)
{
    // 1. ���Ҷ�Ӧ��Ͱ
    size_t index = SizeClass::Index(size);

    // 2. �����黹
    _spanListSet[index]._mtx.lock();

    while (start) {
        // 2.1 ��¼��ǰ�ڴ�������ص���һ���ڴ��ĵ�ַ��
        void* next = GetNextObj(start);

        // 2.2 �ҵ��黹�ڴ�������ҳSpan
        Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

        GetNextObj(start) = span->_freeList;
        span->_freeList = start;
        span->_useCount--;                      // ����ʹ�õǼ���

        // 2.3 ע���ж��Ƿ�ȫ���黹����ȫ���黹���Խ���ҳ�ϲ�
        if (span->_useCount == 0) {
            _spanListSet[index].Erase(span);    // ���뵽�ˣ�˵��span����ҳ�Ѿ����룡�ʣ��׳��黹��PageCahce
            // ��Դ���루������ٽ�����
            span->_freeList = nullptr;
            span->_prev = nullptr;
            span->_next = nullptr;

            // span �Ѿ������ٽ�����Դ�ˣ�
            _spanListSet[index]._mtx.unlock();
            PageCache::GetInstance()->_pageMtx.lock();
            PageCache::GetInstance()->ReleaseSpanToPageCache(span);     // ����PageCache����ҳ����
            PageCache::GetInstance()->_pageMtx.unlock();
        }

        start = next;
    }

    _spanListSet[index]._mtx.unlock();

}
