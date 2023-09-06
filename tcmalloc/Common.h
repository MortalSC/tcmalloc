#pragma once 

#include <iostream>
#include <cassert>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <windows.h>

//#ifdef _WIN32
//
//#else
//
//#endif // _WIN32



/* ThreadCache 的最大限制 */
static const size_t MAX_BYTES = 256 * 1024;
/* 哈希映射桶的个数 */
static const size_t MAXBLUCKET = 208;
/*  */
static const size_t NPAGES = 129;
/*  */
static const size_t PAGE_SHIFT = 13;

#ifdef _WIN32
typedef size_t PAGE_ID;
#elif _WIN64
typedef unsigned long long PAGE_ID;
#endif

/*
*	直接在堆上申请内存
*/
inline static void* SystemAlloc(size_t kpage) {
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else

#endif
	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}

/*
*	作用：获取给定对象的地址！
*/
static void*& GetNextObj(void* obj) {
	return *(void**)obj;
}

/*
*	自由链表：用于管理内存块
*/
class FreeList {
public:
	/*
	*	指定方式：头插法插入内存块单元
	*	param:
	*		被管理的内存块对象
	*/
	void Push(void* obj) {
		assert(obj);
		//*(void**)obj = _freeList;				// 对二级指针进行解引用即可获取指针在对应环境OS下的大小！
		GetNextObj(obj) = _freeList;
		_freeList = obj;
		++_size;
	}
	/*
	*	支持范围（串）插入
	*/
	void PushRange(void* start, void* end, size_t n) {
		GetNextObj(end) = _freeList;
		_freeList = start;
		_size += n;
	}

	/*
	*	指定方式：头删法
	*/
	void* Pop() {
		void* obj = _freeList;
		_freeList = GetNextObj(obj);
		--_size;
		return _freeList;
	}

	/*
	*	作用：范围式删除！
	*/
	void PopRange(void*& start, void*& end, size_t n) {
		assert(n >= _size);
		start = _freeList;
		for (size_t i = 0; i < n - 1; i++) {
			end = GetNextObj(end);
		}
		_freeList = GetNextObj(end);
		GetNextObj(end) = nullptr;
		_size -= n;
	}

	/*
	*	判断自由链表是否为空！
	*/
	bool Empty() {
		return _freeList == nullptr;
	}

	size_t& MaxSize() {
		return _maxSize;
	}

	size_t GetSize() { return _size; }

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;					// 用于限制向centralcache申请时，慢申请防止分配过多而浪费
	size_t _size;							// 记录链表载有的结点个数
};


/* 计算获取关于自由链表对象的对齐规则 */
/*
*	对齐下限：8 byte => 原因：至少能存储下一个内存单元的地址，形成自由链表进行管理！
*	对齐规则：
*	整体控制在最多10%左右的内碎片浪费
*	[1,128]						8byte对齐						freelist[0,16)
*	[128+1,1024]				16byte对齐						freelist[16,72)
*	[1024+1,8*1024]				128byte对齐						freelist[72,128)
*	[8*1024+1,64*1024]			1024byte对齐					freelist[128,184)
*	[64*1024+1,256*1024]		8*1024byte对齐					freelist[184,208)
* 
*	分配关键：判断指定的对象单位大小在哪个区间，
			  在指定区间内，使用特定单元块“拼接”的方式，实现用最小单元的整数倍的内存大小存储给定的对象！
*	
*	桶分配计算：size 除以 对齐标准单元 = 商 ······ 余数
*	商：即该区间内桶的下标【 总体的下标 = 上一个区间长度 + 商 】
*	余数：即存储该对象所浪费的内存内碎片大小
*	
*/
class SizeClass {
public:
	/* 
	*	向上对齐 ，返回分配的空间大小
	*/
	static inline size_t RoundUp(size_t size) {
		if (size <= 128) {
			// 区间：[1,128]	
			return _RoundUp(size, 8);
		}
		else if (size <= 1024) {
			// 区间：[128+1,1024]	
			return _RoundUp(size, 16);
		}
		else if (size <= 8*1024) {
			// 区间：[1024+1,8*1024]	
			return _RoundUp(size, 128);
		}
		else if (size <= 64*1024) {
			// 区间：[8*1024+1,64*1024]	
			return _RoundUp(size, 1024);
		}
		else if (size <= 256*1024) {
			// 区间：[64*1024+1,256*1024]	
			return _RoundUp(size, 8*1024);
		}
		else{
			// 有问题！理论上不可能到此！	
			assert(false);
			return -1;
		}
	}

	/*
	*	计算返回：空间对应的映射桶【计算映射的哪一个自由链表桶】
	*	定位思路：bytes 对应每个区间中映射桶 + 前一个区间桶的个数 = 总体中桶的映射关系 
	*/
	// 
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		// 每个区间有多少个链
		static int group_array[4] = { 16, 56, 56, 56 };
		if (bytes <= 128) {
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024) {
			return _Index(bytes - 128, 4) + group_array[0];
		}
		else if (bytes <= 8 * 1024) {
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (bytes <= 64 * 1024) {
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1]
				+ group_array[0];
		}
		else if (bytes <= 256 * 1024) {
			return _Index(bytes - 64 * 1024, 13) + group_array[3] +
				group_array[2] + group_array[1] + group_array[0];
		}
		else {
			assert(false);
		}
		return -1;
	}

	// 一次从中心缓存获取多少个
	static size_t NumMoveSize(size_t size)
	{
		//if (size == 0)
		//	return 0;
		assert(size);
		// [2, 512]，一次批量移动多少个对象的(慢启动)上限值
		// 小对象一次批量上限高
		// 大对象一次批量上限低
		int num = MAX_BYTES / size;		// 256 * 1024 / 8 = 32768
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}

	/*
	*	
	*/
	static size_t NumMovePage(size_t size) {
		size_t num = NumMoveSize(size);
		size_t npage = num * size;
		npage >>= PAGE_SHIFT;
		if (npage == 0) npage = 1;
		return npage;
	}

private:
	/*
	*	作用：返回分配的空间大小
	*/
#if 0
	size_t _RoundUp(size_t size, int align_standard) {
		// 1. 获取对齐标准
		size_t alignSize;									// 计算对齐后的空间占用大小
		if (size % align_standard != 0) {
			alignSize = (size / align_standard + 1) * (align_standard);
		}
		else {
			alignSize = align_standard;
		}
	}
#endif
	/*
		param:
			bytes：申请的目标空间大小
			align：对齐标准单元 [ 8、16、··· ]
	*/
	static inline size_t _RoundUp(size_t bytes, size_t align) {
		return ((bytes + align - 1) & ~(align - 1));
	}

	/*
	*	作用：计算映射的哪一个自由链表桶
	*	param:
	*		bytes：申请的目标空间大小
	*		align_shift：对齐标准单元 [ 8、16、··· ]的二进制 1 的位置
				8	=> 3
	*			16	=> 4
	*			128 => 7
	*			···
	*/
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
};


/*
*	管理多个连续页的大块内存跨度结构
*/
struct Span { 
	
	size_t _page_id;		// 大块内存的起始页号
	size_t _n;				// 某块span页中的剩余单位数量

	Span* _next;			// 双向自由链表的指针
	Span* _prev;			// 双向自由链表的指针

	size_t _useCount;		// 计数：统计切好并分配个threadcache的个数
	void* _freeList = nullptr;		// 切好的小内存的自由链表

	bool isUse = false;
};

// 含哨兵头结点的双向循环链表
class SpanList {
public:
	SpanList() {
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	void Insert(Span* pos, Span* newSpan) {
		assert(pos);
		assert(newSpan);
		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
	}

	void Erase(Span* pos) {
		assert(pos);
		assert(pos != _head);
		Span* prev = pos->_prev;
		Span* next = pos->_next;
		prev->_next = next;
		next->_prev = prev;
	}

	Span* Begin() {
		return _head->_next;
	}

	Span* End() {
		return _head;
	}

	void PushFront(Span* span) {
		Insert(Begin(), span);
	}

	Span* PopFront() {
		Span* front = _head->_next;
		Erase(front);
		return front;
	}

	bool Empty() {
		return _head->_next == _head;
	}
private:
	Span* _head;		// 哨兵头结点
public:
	std::mutex _mtx;	// 桶锁
};