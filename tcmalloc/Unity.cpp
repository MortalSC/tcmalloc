#include "ThreadCache.h"
#include "Alloc.h"

/* 测试获取最小内存对齐分配值，即哈希映射到的桶编号：OK */
void GetMinBucketCapacity() {
	ThreadCache t;

	t.Allocate(1);

	t.Allocate(10);

	t.Allocate(100);

	t.Allocate(511);

	t.Allocate(6012);
}

void Alloc1() {
	for (size_t i = 0; i < 7; i++) {
		void* ptr = Alloc(7);
	}
}

void Alloc2() {
	for (size_t i = 0; i < 5; i++) {
		void* ptr = Alloc(65535);
	}
}

/* 测试多线程申请内存 */
void TestThreadAlloc() {
	std::thread t1(Alloc1);
	std::thread t2(Alloc2);

	t1.join();
	t2.join();

}



int main() {

	//GetMinBucketCapacity();
	TestThreadAlloc();
	return 0;
}