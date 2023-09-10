#include "Alloc.h"
#include "Common.hpp"
using std::cout;
using std::endl;

/* 第一阶段测试：内存申请（单线程） */
void Alloc1()
{
	for (size_t i = 0; i < 5; ++i)
	{
		void* ptr = Alloc(6);
	}
}

void Alloc2()
{
	for (size_t i = 0; i < 5; ++i)
	{
		void* ptr = Alloc(7);
	}
}

/* 第二阶段测试：内存申请（多线程） */
void TLSTest()
{
	std::thread t1(Alloc1);
	std::thread t2(Alloc2);
	
	t1.join();
	t2.join();
}


/* 第三阶段测试：内存申请与释放（单线程） */
void TestConcurrentAlloc1()
{
	void* p1 = Alloc(6);
	void* p2 = Alloc(8);
	void* p3 = Alloc(1);
	void* p4 = Alloc(7);
	void* p5 = Alloc(8);
	void* p6 = Alloc(8);
	void* p7 = Alloc(8);


	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;
	cout << p6 << endl;
	cout << p7 << endl;


	Dealloc(p1, 6);
	Dealloc(p2, 8);
	Dealloc(p3, 1);
	Dealloc(p4, 7);
	Dealloc(p5, 8);
	Dealloc(p6, 8);
	Dealloc(p7, 8);
}

void TestConcurrentAlloc2()
{
	for (size_t i = 0; i < 1024; ++i)
	{
		void* p1 = Alloc(6);
		cout << p1 << endl;
	}

	void* p2 = Alloc(8);
	cout << p2 << endl;
}

/* 页ID与地址映射计算测试 */
void TestAddressShift()
{
	PAGE_ID id1 = 2000;
	PAGE_ID id2 = 2001;
	char* p1 = (char*)(id1 << PAGE_SHIFT);
	char* p2 = (char*)(id2 << PAGE_SHIFT);
	while (p1 < p2)
	{
		cout << (void*)p1 << ":" << ((PAGE_ID)p1 >> PAGE_SHIFT) << endl;
		p1 += 8;
	}
}


/* 第四阶段测试：内存申请与释放（多线程） */
void MultiThreadAlloc1()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = Alloc(6);
		v.push_back(ptr);
	}

	for (auto e : v)
	{
		Dealloc(e, 6);
	}
}

void MultiThreadAlloc2()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = Alloc(16);
		v.push_back(ptr);
	}

	for (auto e : v)
	{
		Dealloc(e, 16);
	}
}

void TestMultiThread()
{
	std::thread t1(MultiThreadAlloc1);
	std::thread t2(MultiThreadAlloc2);

	t1.join();
	t2.join();
}

int main()
{
	//TestObjectPool();
	//TLSTest();

	TestConcurrentAlloc1();
	//TestAddressShift();

	//TestMultiThread();

	return 0;
}