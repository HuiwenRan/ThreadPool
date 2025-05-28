#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>

// 任务抽象类，所有任务都继承此
class Task {
public:
	virtual void run() = 0;
private:
};

enum class PoolMode {
	MODE_FIXED,
	MODE_CACHED
};

class Thread {
public:
	using ThreadFunc = std::function<void()>;
	Thread(ThreadFunc);
	~Thread();
	void start();
private:
	ThreadFunc func_;
};

class ThreadPool{
public:
	ThreadPool();
	~ThreadPool();
	void start(int initThreadSize = 4);
	void setMode(PoolMode mode);

	void setInitThreadSize(int size);

	void submitTask(std::shared_ptr<Task> task); //生产任务

	ThreadPool(const ThreadPool& threadPool) = delete;
	ThreadPool& operator=(const ThreadPool& threadPool) = delete;

private:
	// vector在出作用域后会自动调用元素的析构函数，释放内存
	std::vector<std::unique_ptr<Thread>> threads_;
	int initThreadSize_;
	void threadFunc(); //线程池指定线程执行的函数！！！消费任务

	std::queue<std::shared_ptr<Task>> taskQue_;
	std::atomic_int taskSize_;
	int taskQueMaxThreshHold_;

	std::mutex taskQueMtx_;
	std::condition_variable notFull_;
	std::condition_variable notEmpty_;

	PoolMode poolMode_;
};

#endif // THREADPOOL_H
