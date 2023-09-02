#pragma once

#include "Common.h"

class ThreadCache
{
public:
    // 申请和释放内存对象
    // 参数：申请的对象大小
    void *Allocate(size_t size);
    // 参数：释放的目标地址 + 释放的空间大小
    void Deallocate(void *ptr, size_t size);

    void FetchFromCentralCache();


private:
    FreeList _freeLists[NFREE_LISTS];
};

// static _declspec(thread) ThreadCache* pTSLThreadCache = nullptr;
// TLS thread local storage
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;