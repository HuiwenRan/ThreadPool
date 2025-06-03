#include "threadpool.h"



ThreadPool::ThreadPool()
	: taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolrunning_(false)
	, initThreadSize_(0)
	, idleThreadSize_(0)
{}

ThreadPool::~ThreadPool()
{
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	// 关闭线程池，设置运行状态为false
	isPoolrunning_ = false;
	// 通知所有线程退出
	notEmpty_.notify_all();
	exitCond_.wait(lock, [this]() { return threads_.empty(); });
}

void ThreadPool::setMode(PoolMode mode) {
	if (checkRunningState()) return;
	poolMode_ = mode;
}
void ThreadPool::setThreadSizeThreshHold(int threshHold) {
	if (checkRunningState()) return;
	if (threshHold > 0 && poolMode_ == PoolMode::MODE_CACHED) {
		threadSizeThreshHold_ = threshHold;
	}
	else {
		std::cerr << "Invalid thread size threshold. Must be greater than 0." << std::endl;
	}
}
void ThreadPool::setTaskQueMaxThreshHold(int taskQueMaxThreshHold) {
	if (checkRunningState()) return;
	if (taskQueMaxThreshHold > 0) {
		taskQueMaxThreshHold_ = taskQueMaxThreshHold;
	}
	else {
		std::cerr << "Invalid task queue max threshold. Must be greater than 0." << std::endl;
	}
}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
	isPoolrunning_ = true; // 设置线程池运行状态为true

	if (initThreadSize > 0) {
		initThreadSize_ = initThreadSize;

	}
	else {
		std::cerr << "Invalid thread size. Must be greater than 0." << std::endl;
	}
	for (int i = 0; i < initThreadSize_; ++i) {
		auto threadPtr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		threads_.insert(std::make_pair(threadPtr->getId(),std::move(threadPtr)));
	}
	for (auto& thread : threads_) {
		thread.second->start();
		idleThreadSize_++;
	}
}

void ThreadPool::threadFunc(int threadId) 
{
	// 注意这是在子线程中执行的函数，！！！！消费者
	while (1) 
	{
		std::cout << "tid" << std::this_thread::get_id() << " waiting for task." << std::endl;

		std::unique_lock<std::mutex> lock(taskQueMtx_);

		
		while (taskQue_.empty())
		{
			if (isPoolrunning_ == false) {
				std::cout << "tid" << std::this_thread::get_id() << " exiting due to pool shutdown." << std::endl;
				threads_.erase(threadId);
				exitCond_.notify_one();
				return; // 线程池关闭，退出当前线程
			}
			if (poolMode_ == PoolMode::MODE_FIXED)
				notEmpty_.wait(lock);
			else {
				if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(IdelTimeout)))
				{
					// 超时返回，线程数量大于初始值，则退出当前线程
					if (threads_.size() > initThreadSize_)
					{
						std::cout << "tid" << std::this_thread::get_id() << " exiting due to idle timeout." << std::endl;
						idleThreadSize_--; // 确保空闲线程计数正确
						threads_.erase(threadId); // 从线程池中移除当前线程
						return; // 退出当前线程
					}
				}
			}
		}
	
		idleThreadSize_--;
		std::cout << "tid" << std::this_thread::get_id() << " got a task." << std::endl;
		Task task = taskQue_.front();
		taskQue_.pop();
		taskSize_--;
		notFull_.notify_one();

		// 如果还有任务，可以通知其他线程
		if (taskSize_ > 0) {
			notEmpty_.notify_all();
		}

		// 释放锁，而不是等到执行任务完成后才释放，为了不耽搁其他线程对任务队列访问
		lock.unlock();

		try {
			// 执行任务，把任务返回值setVal给Result对象
			task();
		}
		catch (const std::exception& e) {
			std::cerr << "Task exception: " << e.what() << std::endl;
		}
		catch (...) {
			std::cerr << "Unknown task exception." << std::endl;
		}
		// 任务执行完毕，线程可以继续等待下一个任务
		std::cout << "tid" << std::this_thread::get_id() << " finished a task." << std::endl;
		idleThreadSize_++;
	}
}

/////////// 以下是Thread类的实现

Thread::Thread(ThreadFunc func)
	: func_(func)
	, threadId_(generateNum++) // 生成唯一的线程ID
{ }

Thread::~Thread(){}

void Thread::start()
{
	// 创建线程来执行func_
	std::thread t(func_, threadId_);
	t.detach(); 
}