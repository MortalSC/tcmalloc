#include "PageCache.h"

PageCache PageCache::_sInstan;


Span* PageCache::NewSpan(size_t num) {
	/* 页数的合法判断 */
	assert(num > 0 && num < NPAGES);

	// 检查特定页是否有span
	if (!_spanLists[num].Empty()) {
		// 有 span 则分配！
		return _spanLists->PopFront();
	}

	// 走到此处，说明特定的页是空
	// 此时，向后搜索非空页，进行切分并调整页
	for (size_t i = num + 1; i < NPAGES; ++i) {
		if (!_spanLists[i].Empty()) {
			// 顺序找到第一个非空的桶！ => 开始分割，并调整页
			// 分割：切分成 num 页的 span 和 一个 
			Span* nSpan = _spanLists[i].PopFront();				// 拿到非空页中的第一个 span（具有特定大小）
			Span* kSpan = new Span;

			// num 页分配给 central cache
			kSpan->_page_id = nSpan->_page_id;					
			kSpan->_n = num;
			nSpan->_page_id += num;
			nSpan->_n -= num;

			_spanLists[nSpan->_n].PushFront(nSpan);				// 切后后的属于部分，调整到页管理的映射位置

			// 存储未被分配出去的页单元的起始与终止页，便于page cache 回收内存
			_idSpanMap[nSpan->_page_id] = nSpan;
			_idSpanMap[nSpan->_page_id + nSpan->_n - 1] = nSpan;

			// 建立页 id 与 span 的映射，便于 central cache 回收小内存时，查找对应的span
			for (PAGE_ID i = 0; i < kSpan->_n; i++) {
				_idSpanMap[kSpan->_page_id + i] = kSpan;
			}

			return kSpan;
		}
	}

	// 走到此处，说明 page cache 中没有更大的可用页 span 了
	// 此时，向堆申请 128 页 的span
	Span* bigSpan = new Span;

	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_page_id = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;

	_spanLists[bigSpan->_n].PushFront(bigSpan);
	return NewSpan(num);
}

Span* PageCache::MapObjectToSpan(void* obj) {
	// 计算页号
	PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;	
	// 通过页号匹配span
	auto ret = _idSpanMap.find(id);
	if (ret != _idSpanMap.end()) {
		// 找到映射
		return ret->second;
	}
	else {
		// 没找到！就是有问题（理论上不会出现）
		assert(false);
		return nullptr;
	}
}

void PageCache::ReleaseSpanToPageCache(Span* span) {
	// 对 span 前后的页，尝试进行合并，缓解内存碎片问题
	while (1) {
		PAGE_ID prev_id = span->_page_id = 1;					// 先向前探查
		auto ret = _idSpanMap.find(prev_id);
		if (ret == _idSpanMap.end()) {
			// 前面的页号没有，不合并
			break;
		}
		Span* prevSpan = ret->second;
		if (prevSpan->isUse == true) {
			// 存在响铃页，但是正在被使用
			break;
		}
		// 若合并的大小大于 128 就不合并！
		if (prevSpan->_n + span->_n >= NPAGES - 1) {
			break;
		}

		span->_page_id = prevSpan->_page_id;
		span->_n += prevSpan->_n;

		_spanLists[prevSpan->_n].Erase(prevSpan);
		delete prevSpan;
	}

	// 向后合并
	while (1) {
		PAGE_ID next_id = span->_page_id + span->_n - 1;
		auto ret = _idSpanMap.find(next_id);
		if (ret == _idSpanMap.end()) {
			// 前面的页号没有，不合并
			break;
		}
		Span* nextSpan = ret->second;
		if (nextSpan->isUse == true) {
			// 存在响铃页，但是正在被使用
			break;
		}
		// 若合并的大小大于 128 就不合并！
		if (nextSpan->_n + span->_n >= NPAGES - 1) {
			break;
		}

		span->_n += nextSpan->_n;

		_spanLists[nextSpan->_n].Erase(nextSpan);
		delete nextSpan;
	}

	_spanLists[span->_n].PushFront(span);
	span->isUse = false;
	_idSpanMap[span->_page_id] = span;
	_idSpanMap[span->_page_id + span->_n - 1] = span;
}