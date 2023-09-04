#pragma once 

#include <iostream>
#include <cassert>
#include <vector>


/* ThreadCache 的最大限制 */
static const size_t MAX_BYTES = 256 * 1024;

/*
*	作用：获取给定对象的地址！
*/
void*& GetNextObj(void* obj) {
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
	}
	/*
	*	指定方式：头删法
	*/
	void Pop() {
		void* obj = _freeList;
		_freeList = GetNextObj(obj);
	}
private:
	void* _freeList;
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
	*	向上对齐 
	*/
	size_t RoundUp(size_t size) {
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

private:
	/*
	*	作用：返回对齐的映射下标
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
};