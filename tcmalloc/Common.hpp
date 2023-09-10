#pragma once

#include <iostream>
#include <cassert>
#include <algorithm>
#include <unordered_map>

#include <thread>
#include <mutex>


/* ThreadCache 哈希映射桶的个数 */
static const size_t BUCKETSIZE = 208;

/* ThreadCache 下的最大内存限制 */
static const size_t MAXBTYES = 256 * 1024;

/* PageCache 的最大页数 */
static const size_t PAGENUMS = 129;

/* 位移基数：本项目默认以 8 kb 为一页标准：2^13 = 8 * 1024 */
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

// 直接去堆上按页申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	// kpage << 13：表示申请 8 kb * 页号 的内存（最大页内存）
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

/* 方法函数：实现返回指定内存块的地址 */
static void*& GetNextObj(void* obj) {
	return *(void**)obj;
}

/* 自由链表 */
class FreeList {
public:
	/*
	*	内存块的插入：回收空闲内存块（内存回收）
	*/
	void Push(void* obj) {
		assert(obj);
		// 获取 _freeList 指向的第一个结点的地址，
		GetNextObj(obj) = _freeList;
		// _freeList回指，完成头插
		_freeList = obj;
		_size += 1;
	}

	/*
	*	支持范围性插入
	*	param：
	*		start：起始
	*		end：终止
	*		num：插入的个数
	*/
	void PushRange(void* start, void* end, size_t num) {
		GetNextObj(end) = _freeList;
		_freeList = start;
		_size += num;
	}

	/*
	*	内存块的弹出：分配空闲内存块（空间分配）【返回内存块】
	*/
	void* Pop() {
		// 指向链表中的第一个结点
		void* obj = _freeList;
		// 实现指向第二个结点
		_freeList = GetNextObj(obj);
		_size -= 1;

		return obj;
	}
	/*
	*	支持范围性删除
	*	param：
	*		start：起始
	*		end：终止
	*		num：删除的个数
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
	*	判断链表是否为空！即表示：是否有可用内存！
	*/
	bool Empty() {
		return _freeList == nullptr;
	}

	/*
	*	返回链表结点数慢增长的最大限制
	*/
	size_t& MaxSize() {
		return _maxSize;
	}

	/*
	*	获取当前链表长度
	*/
	size_t GetSize() {
		return _size;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
	size_t _size = 0;				// 用于记录链表中的内存块个数！
};


/* 功能类：集合了哈希映射算法、最小内存对齐算法 */
class SizeClass {
public:
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
	*			  在指定区间内，使用特定单元块“拼接”的方式，实现用最小单元的整数倍的内存大小存储给定的对象！
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
			std::cout << "申请的限度不该出现在 ThreadCache 中！" << std::endl;
			return -1;
		}
	}

	/*
	*	映射的桶的编号计算方法：在指定区间中获取到拼接存储所需的最小内存块个数 + 前面的区间长度
	*	如：
	*		15		=> 1（8+8）
	*		129		=> 1 (1 + 15)
	*	如下 _Index 的第二个参数有两种传递策略：
	*		// alignSize：内存块单元大小：8
	*		// alignSize：内存块单元对应二进制位移位数：8 => 2^3 => alignSize：3（采用该形式）
	*/
	static inline size_t Index(size_t size) {
		assert(size <= MAXBTYES);
		static int arr[4] = { 16,56,56,56 };
		if (size <= 128) {
			return _Index(size, 3);																// 8 => 2 ^ 3 => alignSize：3
		}
		else if (size <= 1024) {
			return _Index(size - 128, 4) + arr[0];												// 16 = > 2 ^ 4 = > alignSize：4
		}
		else if (size <= 1024 * 8) {
			return _Index(size - 1024, 7) + arr[0] + arr[1];									// 128 = > 2 ^ 7 = > alignSize：7
		}
		else if (size <= 1024 * 64) {
			return _Index(size - 1024 * 8, 10) + arr[0] + arr[1] + arr[2];					// 1024 = > 2 ^ 10 = > alignSize：10
		}
		else if (size <= 1024 * 256) {
			return _Index(size - 1024 * 64, 13) + arr[0] + arr[1] + arr[2] + arr[3];		// 8 * 1024 = > 2 ^ 13 = > alignSize：13
		}
		else {
			std::cout << "申请的限度不该出现在 ThreadCache 中！" << std::endl;
			return -1;
		}
	}

	/*
	*	内存慢分配的单次“配发”计算
	*	计算规则：
	*		[2, 512]，一次批量移动多少个对象的(慢启动)上限值
	* 		小对象一次批量上限高
	* 		小对象一次批量上限低
	*	注：有 256 kb 的约束
	*/
	static size_t LimitAllocNum(size_t size) {
		assert(size);
		size_t num = MAXBTYES / size;			// 使用约束上限 判断 对指定大小的对象的分配额度
		if (num < 2) {
			num = 2;							// 下限约束：至少要多分配一个单位的富余空间
		}
		if (num > 512)							// 上限约束：对于小对象不要一次过多分配：如 8 bytes 【256 * 1024 / 8 = 32768】
			num = 512;							// 这个上限可以再小 【256 * 1024 / 512 = 512】
		return num;
	}

	/*
	*	对PageCache页进行分配操作！
	*	作用：即计算一次像系统获取几个页！
	*	单个对象 8byte
	* 	...
	* 	单个对象 256KB
	*/
	static size_t GetToPageNum(size_t size)
	{
		size_t spanNum = LimitAllocNum(size);	// 计算对象的实际大小对应的分配约束个数（确保ye中有足够的分配空间）
		size_t nPage = spanNum * size;			// 计算预计分配的大小

		nPage >>= PAGE_SHIFT;					// 空间大小 / 8kb 判断分配的内存需要的页数
		if (nPage == 0)
			nPage = 1;							// 页起始定位为 1【共128个页单位】
		return nPage;
	}

private:
	static inline size_t _RoundUp(size_t size, size_t alignSize) {
		// 算法思路：
		// 根据整除向下对齐的原则：只要加上一个对齐标准值，即可保证获取到存储 size bytes所需的内存块个数！
		// 最少内存块个数 * 内存块大小 = 最小的分配空间大小
		// 对于高聘操作中，位运算效率高于符号运算！
		// 如 152，显然对于 152 而言，需要的就是 10 个内存块单元（大小为16）！
		// 152：1001 1000（二进制）
		// 单元大小为 16 为例： 0001 0000
		// 如何推算出最小的对齐内存段大小？即：如何使：1001 1000（152） => 1010 0000（160）
		return (size + alignSize - 1) & ~(alignSize - 1);
		// size + alignSize 中 -1，作用是使之不会“进位”！
		// 152 + 16 => 1010 1000
		// 16 - 1	=> 0000 1111
		// ~(alignSize - 1) => 1111 0000
		// 1010 1000 & 1111 0000 => 1010 0000 => 160（十进制）
	}

	static inline size_t _Index(size_t size, size_t alignSize) {
		//return (size + alignSize - 1) / alignSize - 1;			// alignSize：内存块单元大小：8
		return ( (size +  (1 << alignSize) - 1) >> alignSize) - 1;	// alignSize：内存块单元对应二进制位移位数：8 => 2^3 => alignSize：3
	}

};


struct Span {
	// 维持SpanList结点关系
	Span* _prev = nullptr;
	Span* _next = nullptr;

	// 维护自由链表
	void* _freeList = nullptr;		

	size_t _useCount = 0;

	PAGE_ID _pageID = 0;			// PageCache下对应的页号！

	size_t _n = 0;					// 记录PageCache中页号[1~128]

	bool isUse = false;				// 表示是否处于使用中“页级”
};

class SpanList {
public:
	// 构造
	SpanList() {
		_head = new Span;
		// 双向链表：起始为空
		_head->_prev = _head;
		_head->_next = _head;
	}

	/*
	*	向SpanList中插入Span元素：可能是来自ThreadCache的归还 / 向 PageCache 申请到的内存
	*	param:
	*		aimPos：双向链表的首结点
	*		newSpan：新结点
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
	*	删除 SpanList 中的指定 Span 元素：可能是归还给 PageCache / 给 ThreadCache 分配内存
	*/
	void Erase(Span* span) {
		assert(span);
		assert(span != _head);		// 删除的不是哨兵结点
		Span* prev = span->_prev;
		Span* next = span->_next;
		prev->_next = next;
		next->_prev = prev;
	}

	/*
	*	获取第一个结点
	*/
	Span* Begin() {
		return _head->_next;
	}

	/*
	*	获取尾结点的下一个位置
	*/
	Span* End() {
		return _head;
	}

	/*
	*	头插法：
	*/
	void PushFront(Span* span) {
		Insert(Begin(), span);
	}

	/*
	*	头删法
	*/
	Span* PopFront() {
		Span* front = _head->_next;
		Erase(front);
		return front;
	}

	/*
	*	判断是否为空！
	*	判断PageCache桶中页对应的链表是否为空！
	*/
	bool Empty() {
		// 双向链表
		return _head->_next == _head;
	}




private:
	Span* _head;

public:
	std::mutex _mtx;
};