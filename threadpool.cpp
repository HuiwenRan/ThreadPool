#include "threadpool.h"

const int TASK_MAX_THRESHHOLD = 1024;

ThreadPool::ThreadPool()
	: taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED) 
{}

ThreadPool::~ThreadPool(){}

void ThreadPool::setMode(PoolMode mode) {
	poolMode_ = mode;
}

void ThreadPool::setInitThreadSize(int size) {
	
}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
	if (initThreadSize > 0) {
		initThreadSize_ = initThreadSize;
	}
	else {
		std::cerr << "Invalid thread size. Must be greater than 0." << std::endl;
	}
	for (int i = 0; i < initThreadSize_; ++i) {
		// 下面这是错误写法，unique_ptr不允许拷贝赋值，但是可以移动std::move(ptr)
		/*auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc));
		threads_.emplace_back(ptr);*/
		// 下面写法虽然正确，但是直接使用new，如果在emplace_back出错，则会出现内存泄漏
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));
		threads_.emplace_back(std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this)));
	}
	for (auto& thread : threads_) {
		thread->start();
	}
}

void ThreadPool::threadFunc() 
{
	// 注意这是在子线程中执行的函数，！！！！消费者
	while (1) 
	{
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		notEmpty_.wait(lock, [this]()->bool {return !taskQue_.empty(); });

		std::shared_ptr<Task> task = taskQue_.front();
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
			task->run();
		}
		catch (const std::exception& e) {
			std::cerr << "Task exception: " << e.what() << std::endl;
		}
		catch (...) {
			std::cerr << "Unknown task exception." << std::endl;
		}
	}
}


void ThreadPool::submitTask(std::shared_ptr<Task> task)
{
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	if (!notFull_.wait_for(lock,
		std::chrono::seconds(1),
		[this]()->bool {return taskSize_ < taskQueMaxThreshHold_; }))
	{
		std::cerr << "Task queue is full, cannot submit task." << std::endl;
		return;
	}

	taskQue_.emplace(task);
	taskSize_++;

	notEmpty_.notify_all();
}

// 以下是Thread类的实现

Thread::Thread(ThreadFunc func)
	: func_(func)
{ }

Thread::~Thread(){}

void Thread::start()
{
	// 创建线程来执行func_
	std::thread t(func_);
	t.detach(); 
}