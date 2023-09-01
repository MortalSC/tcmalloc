#include <iostream>
//#include <exception>
using std::cout;
using std::endl;


/* 定长内存池的设计实现 */
/*
	核心成员：
		1. 管理一次性申请来的大块内存
		2. 管理被使用归还的内存块

	设计思路：
		池内内存的分配：以字节为单位划分，顺序式“切片”分配内存
		归还的内存块：使用链式管理！使用前一块内存的前四字节存储当前新归还的内存地址，构成链式结构！
*/

#define DEFAULT_SIZE 128*1024

template <class T>
class ObjectPool {
public:
	// 申请
	T* New() {
		T* obj = nullptr;
		if (_freeList) {
			void* next = *((void**)_freeList);				// 记录链表管理的空间中的第二个块内存
			obj = (T*)_freeList;								// 取用链表管理的空间中的第一个块内存
			_freeList = next;								// 链表更新
		}
		else {

			//if (_memory == nullptr)						// 没有空间，就申请空间
			/**
			*	用户申请的空间不可能总是 4 / 8 的整数倍，最终始终存在剩余空间不足的情况
			*	为了解决：申请的目标大小空间与余量不足的问题
			*	引入池内可用容量更新的机制来解决问题！若不够就再申请！
			*/
			if (_remainBytes < sizeof(T))
			{
				_remainBytes = DEFAULT_SIZE;
				_memory = (char*)malloc(DEFAULT_SIZE);
				if (!_memory) {
					throw std::bad_alloc();						// 申请失败，抛出进程异常
				}
			}
			//T* _obj = (T*)_memory;
			obj = (T*)_memory;
			// 【A】优化问题：管理空间块的链表，必须满足能够存储一个指针大小！
			// 当内存小于一个指针空间时！指定至少分配一个指针大小的空间！
			size_t oldSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);

			//_memory += sizeof(T);							// 指向切下剩余的起始位置
			//_remainBytes -= sizeof(T);						// 更新剩余空间
			_memory += sizeof(oldSize);
			_remainBytes -= sizeof(oldSize);

		}

		// 显示初始化
		new(obj)T;

		return obj;
	}

	// 释放
	void Delete(T* obj) {
#if 0
		if (_freeList == nullptr) {
			// 首次回收空间
			_freeList = obj;							// 指向被释放空间【相当于链表的第一个结点】
			/*
				设计链式管理被释放的空间链表！
				尽管我们是，顺序式的分配空间，但是由于不知道使用者的空间释放顺序！
				因此我们采用链式管理的方式来，管理被释放的空间！
				【提问】为什么要管理这部分空间？

				管理的结点形式：指定被释放的内存块，前面 4/8 位来记录下一个结点（内存块）的地址！
			*/
			// *(int*)：即取前四位：可用于存储 32 位下的指针
			//*(int*)obj = nullptr;						// 实现首部地址存储地址的效果！【32位】
			// void** 二级指针：
			* (void**)obj = nullptr;						// 解决 int* 方式的 64 位下，无法适配的问题！
		}
		else {
			// 头插管理
			*(void**)obj = _freeList;
			_freeList = obj;
		}
#endif
		// 【A】代码优化
		obj->~T();										// 显示调用，实例对象的析构
		*(void**)obj = _freeList;
		_freeList = obj;
	}
private:
	char* _memory = nullptr;							// 指向初始内存池
	void* _freeList = nullptr;							// 管理释放归还的内存【注意首次一定是空的！】
	size_t _remainBytes = 0;							// 记录剩余空间大小（池中的剩余可用空间）

};



