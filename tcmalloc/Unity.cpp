#include "ThreadCache.h"
#include "ConcurrentAlloc.h"
#include <thread>


void Alloc1() {
	for (size_t i = 0; i < 7; i++) {
		void* ptr = ConcurrentAlloc(7);
	}
}

void Alloc2() {
	for (size_t i = 0; i < 5; i++) {
		void* ptr = ConcurrentAlloc(7);
	}
}


void TLSTest() {
	std::thread t1(Alloc1);
	std::thread t2(Alloc1);

	t1.join();
	t2.join();
}


void ConcurrentAlloc() {
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(1);
	void* p4 = ConcurrentAlloc(7);
	void* p5 = ConcurrentAlloc(8);
	void* p6 = ConcurrentAlloc(8*512);


	std::cout << p1 << std::endl;
	std::cout << p2 << std::endl;
	std::cout << p3 << std::endl;
	std::cout << p4 << std::endl;
	std::cout << p5 << std::endl;
	std::cout << p6 << std::endl;

	ConcurrentFree(p1,6);
	ConcurrentFree(p2,8);
	ConcurrentFree(p3,1);
	ConcurrentFree(p4,7);
	ConcurrentFree(p5,8);
	ConcurrentFree(p6,8 * 512);

}

void TestConcurrentAlloc2()
{
	for (size_t i = 0; i < 1024; ++i)
	{
		void* p1 = ConcurrentAlloc(6);
		std::cout << p1 << std::endl;
	}

	void* p2 = ConcurrentAlloc(8);
	std::cout << p2 << std::endl;
}

int main() {
	//TLSTest();

	ConcurrentAlloc();
	//TestConcurrentAlloc2();
	return 0;
}