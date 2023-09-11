#pragma once

#include <iostream>
#include <cassert>
#include <algorithm>
#include <unordered_map>

#include <thread>
#include <mutex>

//#include "ObjectPool.h"

/* ThreadCache ��ϣӳ��Ͱ�ĸ��� */
static const size_t BUCKETSIZE = 208;

/* ThreadCache �µ�����ڴ����� */
static const size_t MAXBTYES = 256 * 1024;

/* PageCache �����ҳ�� */
static const size_t PAGENUMS = 129;

/* λ�ƻ���������ĿĬ���� 8 kb Ϊһҳ��׼��2^13 = 8 * 1024 */
static const size_t PAGE_SHIFT = 13;


#ifdef _WIN32
	#include <windows.h>
#else
// ...
#endif

#ifdef _WIN64
	typedef unsigned long long PAGE_ID;
#elif _WIN32
	typedef size_t PAGE_ID;
#else
// linux
	typedef unsigned long long PAGE_ID;
#endif

// ֱ��ȥ���ϰ�ҳ����ռ�
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	// kpage << 13����ʾ���� 8 kb * ҳ�� ���ڴ棨���ҳ�ڴ棩
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux��brk mmap��
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap��
#endif
}

/* ����������ʵ�ַ���ָ���ڴ��ĵ�ַ */
static void*& GetNextObj(void* obj) {
	return *(void**)obj;
}

/* �������� */
class FreeList {
public:
	/*
	*	�ڴ��Ĳ��룺���տ����ڴ�飨�ڴ���գ�
	*/
	void Push(void* obj) {
		assert(obj);
		// ��ȡ _freeList ָ��ĵ�һ�����ĵ�ַ��
		GetNextObj(obj) = _freeList;
		// _freeList��ָ�����ͷ��
		_freeList = obj;
		_size += 1;
	}

	/*
	*	֧�ַ�Χ�Բ���
	*	param��
	*		start����ʼ
	*		end����ֹ
	*		num������ĸ���
	*/
	void PushRange(void* start, void* end, size_t num) {
		GetNextObj(end) = _freeList;
		_freeList = start;
		_size += num;
	}

	/*
	*	�ڴ��ĵ�������������ڴ�飨�ռ���䣩�������ڴ�顿
	*/
	void* Pop() {
		// ָ�������еĵ�һ�����
		void* obj = _freeList;
		// ʵ��ָ��ڶ������
		_freeList = GetNextObj(obj);
		_size -= 1;

		return obj;
	}
	/*
	*	֧�ַ�Χ��ɾ��
	*	param��
	*		start����ʼ
	*		end����ֹ
	*		num��ɾ���ĸ���
	*/
	void PopRange(void*& start, void*& end, size_t num)
	{
		assert(num <= _size);
		start = _freeList;
		end = start;

		for (size_t i = 0; i < num - 1; ++i)
		{
			end = GetNextObj(end);
		}

		_freeList = GetNextObj(end);
		GetNextObj(end) = nullptr;
		_size -= num;
	}

	/*
	*	�ж������Ƿ�Ϊ�գ�����ʾ���Ƿ��п����ڴ棡
	*/
	bool Empty() {
		return _freeList == nullptr;
	}

	/*
	*	���������������������������
	*/
	size_t& MaxSize() {
		return _maxSize;
	}

	/*
	*	��ȡ��ǰ������
	*/
	size_t GetSize() {
		return _size;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
	size_t _size = 0;				// ���ڼ�¼�����е��ڴ�������
};


/* �����ࣺ�����˹�ϣӳ���㷨����С�ڴ�����㷨 */
class SizeClass {
public:
	/* �����ȡ���������������Ķ������ */
	/*
	*	�������ޣ�8 byte => ԭ�������ܴ洢��һ���ڴ浥Ԫ�ĵ�ַ���γ�����������й���
	*	�������
	*	������������10%���ҵ�����Ƭ�˷�
	*	[1,128]						8byte����						freelist[0,16)
	*	[128+1,1024]				16byte����						freelist[16,72)
	*	[1024+1,8*1024]				128byte����						freelist[72,128)
	*	[8*1024+1,64*1024]			1024byte����					freelist[128,184)
	*	[64*1024+1,256*1024]		8*1024byte����					freelist[184,208)
	*
	*	����ؼ����ж�ָ���Ķ���λ��С���ĸ����䣬
	*			  ��ָ�������ڣ�ʹ���ض���Ԫ�顰ƴ�ӡ��ķ�ʽ��ʵ������С��Ԫ�����������ڴ��С�洢�����Ķ���
	*/
	static inline size_t RoundUp(size_t size) {
		if (size <= 128) {
			return _RoundUp(size, 8);
		}
		else if (size <= 1024) {
			return _RoundUp(size, 16);
		}
		else if (size <= 1024 * 8) {
			return _RoundUp(size, 128);
		}
		else if (size <= 1024 * 64) {
			return _RoundUp(size, 1024);
		}
		else if (size <= 1024 * 256) {
			return _RoundUp(size, 1024 * 8);
		}
		else {
			/*std::cout << "������޶Ȳ��ó����� ThreadCache �У�" << std::endl;
			return -1;*/
			// ������ڴ������һ��256 <= size <= 1024kb
			// ���ڴ���Ȼ��PageCache�����뷶Χ�ڣ����������ڴ�ؾ���ȥ�ڴ�����룡��
			return _RoundUp(size, 1 << PAGE_SHIFT);
		}
	}

	/*
	*	ӳ���Ͱ�ı�ż��㷽������ָ�������л�ȡ��ƴ�Ӵ洢�������С�ڴ����� + ǰ������䳤��
	*	�磺
	*		15		=> 1��8+8��
	*		129		=> 1 (1 + 15)
	*	���� _Index �ĵڶ������������ִ��ݲ��ԣ�
	*		// alignSize���ڴ�鵥Ԫ��С��8
	*		// alignSize���ڴ�鵥Ԫ��Ӧ������λ��λ����8 => 2^3 => alignSize��3�����ø���ʽ��
	*/
	static inline size_t Index(size_t size) {
		assert(size <= MAXBTYES);
		static int arr[4] = { 16,56,56,56 };
		if (size <= 128) {
			return _Index(size, 3);																// 8 => 2 ^ 3 => alignSize��3
		}
		else if (size <= 1024) {
			return _Index(size - 128, 4) + arr[0];												// 16 = > 2 ^ 4 = > alignSize��4
		}
		else if (size <= 1024 * 8) {
			return _Index(size - 1024, 7) + arr[0] + arr[1];									// 128 = > 2 ^ 7 = > alignSize��7
		}
		else if (size <= 1024 * 64) {
			return _Index(size - 1024 * 8, 10) + arr[0] + arr[1] + arr[2];					// 1024 = > 2 ^ 10 = > alignSize��10
		}
		else if (size <= 1024 * 256) {
			return _Index(size - 1024 * 64, 13) + arr[0] + arr[1] + arr[2] + arr[3];		// 8 * 1024 = > 2 ^ 13 = > alignSize��13
		}
		else {
			std::cout << "������޶Ȳ��ó����� ThreadCache �У�" << std::endl;
			return -1;
		}
	}

	/*
	*	�ڴ�������ĵ��Ρ��䷢������
	*	�������
	*		[2, 512]��һ�������ƶ����ٸ������(������)����ֵ
	* 		С����һ���������޸�
	* 		С����һ���������޵�
	*	ע���� 256 kb ��Լ��
	*/
	static size_t LimitAllocNum(size_t size) {
		assert(size);
		size_t num = MAXBTYES / size;			// ʹ��Լ������ �ж� ��ָ����С�Ķ���ķ�����
		if (num < 2) {
			num = 2;							// ����Լ��������Ҫ�����һ����λ�ĸ���ռ�
		}
		if (num > 512)							// ����Լ��������С����Ҫһ�ι�����䣺�� 8 bytes ��256 * 1024 / 8 = 32768��
			num = 512;							// ������޿�����С ��256 * 1024 / 512 = 512��
		return num;
	}

	/*
	*	��PageCacheҳ���з��������
	*	���ã�������һ����ϵͳ��ȡ����ҳ��
	*	�������� 8byte
	* 	...
	* 	�������� 256KB
	*/
	static size_t GetToPageNum(size_t size)
	{
		size_t spanNum = LimitAllocNum(size);	// ��������ʵ�ʴ�С��Ӧ�ķ���Լ��������ȷ��ye�����㹻�ķ���ռ䣩
		size_t nPage = spanNum * size;			// ����Ԥ�Ʒ���Ĵ�С

		nPage >>= PAGE_SHIFT;					// �ռ��С / 8kb �жϷ�����ڴ���Ҫ��ҳ��
		if (nPage == 0)
			nPage = 1;							// ҳ��ʼ��λΪ 1����128��ҳ��λ��
		return nPage;
	}

private:
	static inline size_t _RoundUp(size_t size, size_t alignSize) {
		// �㷨˼·��
		// �����������¶����ԭ��ֻҪ����һ�������׼ֵ�����ɱ�֤��ȡ���洢 size bytes������ڴ�������
		// �����ڴ����� * �ڴ���С = ��С�ķ���ռ��С
		// ���ڸ�Ƹ�����У�λ����Ч�ʸ��ڷ������㣡
		// �� 152����Ȼ���� 152 ���ԣ���Ҫ�ľ��� 10 ���ڴ�鵥Ԫ����СΪ16����
		// 152��1001 1000�������ƣ�
		// ��Ԫ��СΪ 16 Ϊ���� 0001 0000
		// ����������С�Ķ����ڴ�δ�С���������ʹ��1001 1000��152�� => 1010 0000��160��
		return (size + alignSize - 1) & ~(alignSize - 1);
		// size + alignSize �� -1��������ʹ֮���ᡰ��λ����
		// 152 + 16 => 1010 1000
		// 16 - 1	=> 0000 1111
		// ~(alignSize - 1) => 1111 0000
		// 1010 1000 & 1111 0000 => 1010 0000 => 160��ʮ���ƣ�
	}

	static inline size_t _Index(size_t size, size_t alignSize) {
		//return (size + alignSize - 1) / alignSize - 1;			// alignSize���ڴ�鵥Ԫ��С��8
		return ( (size +  (1 << alignSize) - 1) >> alignSize) - 1;	// alignSize���ڴ�鵥Ԫ��Ӧ������λ��λ����8 => 2^3 => alignSize��3
	}

};


struct Span {
	// ά��SpanList����ϵ
	Span* _prev = nullptr;
	Span* _next = nullptr;

	// ά����������
	void* _freeList = nullptr;		

	size_t _useCount = 0;

	PAGE_ID _pageID = 0;			// PageCache�¶�Ӧ��ҳ�ţ�

	size_t _n = 0;					// ��¼PageCache��ҳ��[1~128]

	bool isUse = false;				// ��ʾ�Ƿ���ʹ���С�ҳ����

	size_t _objSize = 0;			// ��¼��Ƭ�Ķ���Ĵ�С�������Ż��ڴ��ͷ�ʱ��ָ����С������ȥ����
};

class SpanList {
public:
	// ����
	SpanList() {
		_head = new Span;							
		//_head = spanPool.New();						// �滻ԭ�� new 

		// ˫��������ʼΪ��
		_head->_prev = _head;
		_head->_next = _head;
	}

	/*
	*	��SpanList�в���SpanԪ�أ�����������ThreadCache�Ĺ黹 / �� PageCache ���뵽���ڴ�
	*	param:
	*		aimPos��˫��������׽��
	*		newSpan���½��
	*/
	void Insert(Span* aimPos, Span* newSpan) {
		assert(aimPos);
		assert(newSpan);

		Span* prev = aimPos->_prev;
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = aimPos;
		aimPos->_prev = newSpan;
	}

	/*
	*	ɾ�� SpanList �е�ָ�� Span Ԫ�أ������ǹ黹�� PageCache / �� ThreadCache �����ڴ�
	*/
	void Erase(Span* span) {
		assert(span);
		assert(span != _head);		// ɾ���Ĳ����ڱ����
		Span* prev = span->_prev;
		Span* next = span->_next;
		prev->_next = next;
		next->_prev = prev;
	}

	/*
	*	��ȡ��һ�����
	*/
	Span* Begin() {
		return _head->_next;
	}

	/*
	*	��ȡβ������һ��λ��
	*/
	Span* End() {
		return _head;
	}

	/*
	*	ͷ�巨��
	*/
	void PushFront(Span* span) {
		Insert(Begin(), span);
	}

	/*
	*	ͷɾ��
	*/
	Span* PopFront() {
		Span* front = _head->_next;
		Erase(front);
		return front;
	}

	/*
	*	�ж��Ƿ�Ϊ�գ�
	*	�ж�PageCacheͰ��ҳ��Ӧ�������Ƿ�Ϊ�գ�
	*/
	bool Empty() {
		// ˫������
		return _head->_next == _head;
	}




private:
	Span* _head;
	//ObjectPool<Span> spanPool;
public:
	std::mutex _mtx;
};