#include "ThreadCache.h"

void* ThreadCache::Allocate(size_t size){
    assert(size <= MAX_BYTES);
    size_t alignSize = CalSize::RoundUp(size);
    size_t index = CalSize::Index(size);
    if(!_freeLists[index].Empty()){
        return _freeLists[index].Pop();
    }else{
        // return FetchFromCentralCache(index, alignSize);
    }
}


void ThreadCache::Deallocate(void* ptr, size_t size){

}