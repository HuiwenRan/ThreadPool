#include<iostream>
#include <chrono>
#include "threadpool.h"

using ULong = unsigned long long;

class myTask : public Task {
// �����һ����������Ա�����ǲ�����run()�Ǻ�����
// ��begin_��end_�ĺ�
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
	{
		ThreadPool pool;
		pool.setMode(PoolMode::MODE_CACHED);

		pool.start();

		// ���̼߳�ʱ��ʼ
		auto start_mt = std::chrono::high_resolution_clock::now();

		Result res1 = pool.submitTask(std::make_shared<myTask>(1, 10000000));
		Result res2 = pool.submitTask(std::make_shared<myTask>(10000001, 20000000));
		Result res3 = pool.submitTask(std::make_shared<myTask>(20000001, 30000000));
		Result res4 = pool.submitTask(std::make_shared<myTask>(30000001, 40000000));
		Result res5 = pool.submitTask(std::make_shared<myTask>(40000001, 50000000));
		Result res6 = pool.submitTask(std::make_shared<myTask>(50000001, 60000000));
		Result res7 = pool.submitTask(std::make_shared<myTask>(60000001, 70000000));
		ULong sum1 = res1.get().cast<ULong>();
		ULong sum2 = res2.get().cast<ULong>();
		ULong sum3 = res3.get().cast<ULong>();
		ULong sum4 = res4.get().cast<ULong>();
		ULong sum5 = res5.get().cast<ULong>();
		ULong sum6 = res6.get().cast<ULong>();
		ULong sum7 = res7.get().cast<ULong>();
		ULong total = sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7;
		std::cout << "���̼߳�����: " << total << std::endl;


		auto end_mt = std::chrono::high_resolution_clock::now();
		auto duration_mt = std::chrono::duration_cast<std::chrono::milliseconds>(end_mt - start_mt).count();

		std::cout << "���̺߳�ʱ: " << duration_mt << " ms" << std::endl;

		// ���̼߳�ʱ��ʼ
		auto start_st = std::chrono::high_resolution_clock::now();

		ULong result = 0;
		for (ULong i = 0; i <= 70000000; ++i) {
			result += i;
		}

		auto end_st = std::chrono::high_resolution_clock::now();
		auto duration_st = std::chrono::duration_cast<std::chrono::milliseconds>(end_st - start_st).count();

		std::cout << "���̼߳�����: " << result << std::endl;
		std::cout << "���̺߳�ʱ: " << duration_st << " ms" << std::endl;
	}
	std::cout << "�����̶߳��˳���" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(2));
	return 0;
}
