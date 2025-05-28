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

// �����̳߳�
void ThreadPool::start(int initThreadSize)
{
	if (initThreadSize > 0) {
		initThreadSize_ = initThreadSize;
	}
	else {
		std::cerr << "Invalid thread size. Must be greater than 0." << std::endl;
	}
	for (int i = 0; i < initThreadSize_; ++i) {
		// �������Ǵ���д����unique_ptr����������ֵ�����ǿ����ƶ�std::move(ptr)
		/*auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc));
		threads_.emplace_back(ptr);*/
		// ����д����Ȼ��ȷ������ֱ��ʹ��new�������emplace_back�����������ڴ�й©
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));
		threads_.emplace_back(std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this)));
	}
	for (auto& thread : threads_) {
		thread->start();
	}
}

void ThreadPool::threadFunc() 
{
	// ע�����������߳���ִ�еĺ�������������������
	while (1) 
	{
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		notEmpty_.wait(lock, [this]()->bool {return !taskQue_.empty(); });

		std::shared_ptr<Task> task = taskQue_.front();
		taskQue_.pop();
		taskSize_--;
		notFull_.notify_one();

		// ����������񣬿���֪ͨ�����߳�
		if (taskSize_ > 0) {
			notEmpty_.notify_all();
		}

		// �ͷ����������ǵȵ�ִ��������ɺ���ͷţ�Ϊ�˲����������̶߳�������з���
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

// ������Thread���ʵ��

Thread::Thread(ThreadFunc func)
	: func_(func)
{ }

Thread::~Thread(){}

void Thread::start()
{
	// �����߳���ִ��func_
	std::thread t(func_);
	t.detach(); 
}