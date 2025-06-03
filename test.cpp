#include<iostream>
#include <chrono>
#include "threadpool.h"

using ULong = unsigned long long;

int sum(int a, int b) {
	return a + b;
}

int main() 
{
	ThreadPool pool;

	pool.start(2);

	// 多线程计时开始
	auto start_mt = std::chrono::high_resolution_clock::now();

	std::future<int> r1 = pool.submitTask(sum, 1, 2);
	std::future<int> r2 = pool.submitTask([](int a, int b, int c) {return a + b + c; },1,2,3);

	std::cout << r1.get() << std::endl;
	std::cout << r2.get() << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(2));
	return 0;
}
