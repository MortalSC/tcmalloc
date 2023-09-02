#pragma once

#include <iostream>
#include <vector>
#include <ctime>
#include <cassert>
#include <pthread.h>
#include <unistd.h>
using std::cout;
using std::endl;

static const size_t MAX_BYTES = 256 * 1024;
static const size_t NFREE_LISTS = 208;

// 用于获取内存块的头部中存储“指针”单位的空间
void *&NextObj(void *obj)
{
    return *(void **)obj;
}

// 用于管理分配后空间记录的自由链表
class FreeList
{
public:
    void Push(void *obj)
    {
        assert(obj);
        // 头插（高效）
        // *(void**)obj = _freeList;
        NextObj(obj) = _freeList;
        _freeList = obj;
    }
    void *Pop()
    {
        assert(_freeList);
        // 头删（高效）
        void *obj = _freeList;
        _freeList = NextObj(obj);
        return _freeList;
    }

    bool Empty()
    {
        return _freeList == nullptr;
    }

private:
    void *_freeList = nullptr;
};

class CalSize
{
    // 整体控制在最多10%左右的内碎片浪费
    // [1,128]              8byte对齐       freelist[0,16)
    // [128+1,1024]         16byte对齐      freelist[16,72)
    // [1024+1,8*1024]      128byte对齐     freelist[72,128)
    // [8*1024+1,64*1024]   1024byte对齐    freelist[128,184)
    // [64*1024+1,256*1024] 8*1024byte对齐  freelist[184,208)

    // 参数：申请的大小 + 区间对齐标准数
    // 返回值：对齐大小
    static inline size_t _RoundUp(size_t size, size_t align)
    {
#if 0
        size_t alignNum;
        if (size % align != 0)
        {
            alignNum = (size / alignNum + 1) * alignNum;
        }
        else
        {
            alignNum = size;
        }
        return alignNum;
#endif
        return ((size + align - 1) & ~(align - 1));
        /*
            ~(align - 1)：低位全归零（对齐标准数的二进制）
            (size + align - 1)：
        */
    }

    static inline size_t _Index(size_t bytes, size_t align_shift)
    {
        return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
    }

public:
    // 计算对齐标准（大小）
    static inline size_t RoundUp(size_t size)
    {
        if (size <= 128)
        {
            return _RoundUp(size, 8);
        }
        else if (size <= 1024)
        {
            return _RoundUp(size, 16);
        }
        else if (size <= 8 * 1024)
        {
            return _RoundUp(size, 128);
        }
        else if (size <= 64 * 1024)
        {
            return _RoundUp(size, 1024);
        }
        else if (size <= 256 * 1024)
        {
            return _RoundUp(size, 8 * 1024);
        }
        else
        {
            assert(false);
            return -1;
        }
    }

    // 计算对应的桶
    static inline size_t Index(size_t bytes)
    {
        assert(bytes <= MAX_BYTES);
        // 每个区间有多少个链
        static int group_array[4] = {16, 56, 56, 56};
        if (bytes <= 128)
        {
            return _Index(bytes, 3);
        }
        else if (bytes <= 1024)
        {
            return _Index(bytes - 128, 4) + group_array[0];
        }
        else if (bytes <= 8 * 1024)
        {
            return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
        }
        else if (bytes <= 64 * 1024)
        {
            return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
        }
        else if (bytes <= 256 * 1024)
        {
            return _Index(bytes - 64 * 1024, 13) + group_array[3] +
                   group_array[2] + group_array[1] + group_array[0];
        }
        else
        {
            assert(false);
        }
        return -1;
    }
};