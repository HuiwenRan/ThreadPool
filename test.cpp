#include<iostream>
#include <chrono>
#include "threadpool.h"

using ULong = unsigned long long;

class myTask : public Task {
// 相对于一个函数，成员数据是参数，run()是函数体
// 从begin_到end_的和
public:
	Any run() {
		ULong result = 0;
		for (int i = begin_; i <= end_; ++i) {
			result += i;
		}
		return result;
	}
	myTask(int begin, int end) : begin_(begin), end_(end) {}
private:
	int begin_;
	int end_;
};

int main() {
	ThreadPool pool;
	pool.start();

	// 多线程计时开始
	auto start_mt = std::chrono::high_resolution_clock::now();
	
	Result res1 = pool.submitTask(std::make_shared<myTask>(1, 10000000));
	Result res2 = pool.submitTask(std::make_shared<myTask>(10000001, 20000000));
	Result res3 = pool.submitTask(std::make_shared<myTask>(20000001, 30000000));
	ULong sum1 = res1.get().cast<ULong>();
	ULong sum2 = res2.get().cast<ULong>();
	ULong sum3 = res3.get().cast<ULong>();

	auto end_mt = std::chrono::high_resolution_clock::now();
	auto duration_mt = std::chrono::duration_cast<std::chrono::milliseconds>(end_mt - start_mt).count();

	std::cout << "多线程计算结果: " << (sum1 + sum2 + sum3) << std::endl;
	std::cout << "多线程耗时: " << duration_mt << " ms" << std::endl;

	// 单线程计时开始
	auto start_st = std::chrono::high_resolution_clock::now();

	ULong result = 0;
	for (ULong i = 0; i <= 30000000; ++i) {
		result += i;
	}

	auto end_st = std::chrono::high_resolution_clock::now();
	auto duration_st = std::chrono::duration_cast<std::chrono::milliseconds>(end_st - start_st).count();

	std::cout << "单线程计算结果: " << result << std::endl;
	std::cout << "单线程耗时: " << duration_st << " ms" << std::endl;

	std::this_thread::sleep_for(std::chrono::seconds(2));
	std::cout << "ThreadPool initialized and started." << std::endl;
	return 0;
}
