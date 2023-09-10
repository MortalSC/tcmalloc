#include "PageCache.h"

PageCache PageCache::_Instan;

/*
*	作用：从可用页中获取内存
*/
Span* PageCache::GetNewPage(size_t npage)
{
	assert(npage > 0 && npage < PAGENUMS);

	// 1. 判断指定的页中是否有可用内存！
	// 若没有，则顺序查看下一页直到遇到非空页 / 最后一页（无可用内存，向堆申请）
	if (!_spanListPage[npage].Empty()) {
		return _spanListPage->PopFront();
	}

	// 2. 顺序查看下一页直到遇到非空页 / 最后一页（无可用内存，向堆申请）
	for (size_t i = npage + 1; i < PAGENUMS; i++) {
		if (!_spanListPage[i].Empty()) {
			// 遇到非空页！
			
			// 页上的内存操作规则：该页分成两部分：
			// 第一部分：申请对象时需求的页大小 + 剩余大小（向前挂在到PageCache中的特定页上）
			Span* prevSpan = _spanListPage[i].PopFront();		// 获取到页中内存块
			Span* kSpan = new Span;

			// 在prevSpan的头部切一个 npage 页下来
			// npage 页 npage 返回
			// prevSpan再挂到对应映射的位置

			// 获取到页 ID
			kSpan->_pageID = prevSpan->_pageID;					// 本属于同一页中！分割后的新 span 需要记录页id
			kSpan->_n = npage;									// 记录页号

			prevSpan->_pageID += npage;							// 相对页id是向上递增！即：第128页的页id是最小值！
			prevSpan->_n -= npage;

			_spanListPage[prevSpan->_n].PushFront(prevSpan);	// 大页（第一部分：对一开始的空页进行补充【看图更容易理解】）

			// 存储 prevSpan 的首位页号跟 prevSpan 映射，方便 page cache 回收内存时
			// 进行的合并查找：即记录了大块页的分配去向（便于回收合并）
			_idSpanMap[prevSpan->_pageID] = prevSpan;
			_idSpanMap[prevSpan->_pageID + prevSpan->_n - 1] = prevSpan;


			// 建立id和span的映射，方便central cache回收小块内存时，查找对应的span
			for (PAGE_ID i = 0; i < kSpan->_n; ++i)
			{
				_idSpanMap[kSpan->_pageID + i] = kSpan;
			}

			return kSpan;
		}
	}

	// 3. 到此位置，即：直到最后一页页没有可用空间，需要向相同申请新的大页：去找堆要一个128页的span
	Span* bigSpan = new Span;
	void* ptr = SystemAlloc(PAGENUMS - 1);
	bigSpan->_pageID = (PAGE_ID)ptr >> PAGE_SHIFT;		// 计算 Span 的页 ID ！
	// 计算方法：通过地址（值） / 页大小基本单位（8kb）获取到相对页号！
	bigSpan->_n = PAGENUMS - 1;							// 新获取的页号一定在第 128 页

	_spanListPage[bigSpan->_n].PushFront(bigSpan);		// 把新页放入映射“阶梯”桶中

	return GetNewPage(npage);
}

Span* PageCache::MapObjectToSpan(void* ptr)
{
	PAGE_ID id = ((PAGE_ID)ptr >> PAGE_SHIFT);	// 获取页id
	auto ret = _idSpanMap.find(id);				
	// 通过映射关系找到对应的Span
	if (ret != _idSpanMap.end()) {
		// 找到了映射对象
		return ret->second;						// 返回映射找到的 Span
	}
	else {
		//没有找到：理论上不可能出现
		assert(false);
		return nullptr;
	}

	return nullptr;
}

/*
*	页整理思路：
*		通过前后所搜，查找页id
*/

void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// 向前合并！
	while (1) {
		PAGE_ID prevID = span->_pageID - 1;		// 向前搜索页ID
		//【页ID就是根据地址与设定的页固定块固定大小计算所得】
		// -1：就是向前移动一个页固定块大小单元

		// 在映射中查找是否存在！
		auto ret = _idSpanMap.find(prevID);
		if (ret == _idSpanMap.end()) break;		// 没找到，说明前无内存

		// 走到此处，说明存在连续页，结合被使用状态确定是否可合并
		Span* prevSpan = ret->second;			// 获取到 Span
		if (prevSpan->isUse == true) break;
		// 说明前面的内存存在，但处于被使用状态 => 不能合并！

		// 注意：判断合并的页大小，超过128页就不能合并了
		if (prevSpan->_n + span->_n > PAGENUMS) break;

		// 到此，进行实际页合并！
		span->_pageID = prevSpan->_pageID;		// 更新页ID
		span->_n += prevSpan->_n;				// 计算页大小

		_spanListPage[prevSpan->_n].Erase(prevSpan);
		delete prevSpan;

	}

	// 向后合并
	while (1)
	{
		PAGE_ID nextId = span->_pageID + span->_n;
		auto ret = _idSpanMap.find(nextId);
		if (ret == _idSpanMap.end())
		{
			break;
		}

		Span* nextSpan = ret->second;
		if (nextSpan->isUse == true)
		{
			break;
		}

		if (nextSpan->_n + span->_n > PAGENUMS - 1)
		{
			break;
		}

		span->_n += nextSpan->_n;

		_spanListPage[nextSpan->_n].Erase(nextSpan);
		delete nextSpan;
	}


	_spanListPage[span->_n].PushFront(span);
	span->isUse = false;
	_idSpanMap[span->_pageID] = span;
	_idSpanMap[span->_pageID + span->_n - 1] = span;
}
