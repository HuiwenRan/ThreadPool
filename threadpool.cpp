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
		std::cout << "tid" << std::this_thread::get_id() << " waiting for task." << std::endl;
		notEmpty_.wait(lock, [this]()->bool {return !taskQue_.empty(); });
		std::cout << "tid" << std::this_thread::get_id() << " got a task." << std::endl;
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
			// 执行任务，把任务返回值setVal给Result对象
			task->exec();
		}
		catch (const std::exception& e) {
			std::cerr << "Task exception: " << e.what() << std::endl;
		}
		catch (...) {
			std::cerr << "Unknown task exception." << std::endl;
		}
	}
}


Result ThreadPool::submitTask(std::shared_ptr<Task> task)
{
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	if (!notFull_.wait_for(lock,
		std::chrono::seconds(1),
		[this]()->bool {return taskSize_ < taskQueMaxThreshHold_; }))
	{
		std::cerr << "Task queue is full, cannot submit task." << std::endl;
		return Result(task, false);
	}

	taskQue_.emplace(task);
	taskSize_++;

	notEmpty_.notify_all();
	// 将Result传递给Task！！！
	// 这里要将对象Result直接返回给用户，同时还要将Result对象与Task关联起来，难点
	return Result(task);
}

/////////// 以下是Thread类的实现

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

//////////// 以下是Task类的实现
Task::Task() 
	:result_(nullptr)
{}

void Task::exec()
{
	// 执行任务的run方法，多态
	if(result_ != nullptr)
		result_->setVal(run());
}

void Task::setResult(Result* res)
{
	result_ = res;
}

//////////// 以下是Result类的实现
Result::Result(std::shared_ptr<Task> task, bool isValid)
	: task_(task),
	isValid_(isValid)
{
	task_->setResult(this); // 设置任务的结果指针为当前Result对象
}

Any Result::get()
{
	// 用户调用，获取任务返回值，但是可能没有执行完，需要等待
	if (!isValid_) {
		return "";
	}
	sem_.wait();  //如果还没有计算完，就先阻塞等待
	return std::move(data_);
}

void Result::setVal(Any data)
{
	//  存储task的返回值，在任务执行完后主动调用
	data_ = std::move(data);
	sem_.post(); // 通知等待的线程，数据已经准备好了
}