#include<iostream>
#include <chrono>
#include "threadpool.h"

class myTask : public Task {
public:
	void run() {
		std::cout << "id:" << std::this_thread::get_id << "begin" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::cout << "id:" << std::this_thread::get_id() << "end" << std::endl;
	}
private:
};

int main() {
	ThreadPool pool;
	pool.start();
	// Example task submission
	pool.submitTask(std::make_shared<myTask>());
	pool.submitTask(std::make_shared<myTask>());
	pool.submitTask(std::make_shared<myTask>());
	pool.submitTask(std::make_shared<myTask>());
	pool.submitTask(std::make_shared<myTask>());
	pool.submitTask(std::make_shared<myTask>());
	pool.submitTask(std::make_shared<myTask>());
	pool.submitTask(std::make_shared<myTask>());
	pool.submitTask(std::make_shared<myTask>());
	pool.submitTask(std::make_shared<myTask>());

	std::this_thread::sleep_for(std::chrono::seconds(10)); // Wait to ensure threads are started
	std::cout << "ThreadPool initialized and started." << std::endl;
	return 0;
}