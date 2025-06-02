#include "threadpool.h"

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
const int IdelTimeout = 2; // �����̳߳�ʱʱ�䣬��λ��

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
	// �ر��̳߳أ���������״̬Ϊfalse
	isPoolrunning_ = false;
	// ֪ͨ�����߳��˳�
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

// �����̳߳�
void ThreadPool::start(int initThreadSize)
{
	isPoolrunning_ = true; // �����̳߳�����״̬Ϊtrue

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
	// ע�����������߳���ִ�еĺ�������������������
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
				return; // �̳߳عرգ��˳���ǰ�߳�
			}
			if (poolMode_ == PoolMode::MODE_FIXED)
				notEmpty_.wait(lock);
			else {
				if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(IdelTimeout)))
				{
					// ��ʱ���أ��߳��������ڳ�ʼֵ�����˳���ǰ�߳�
					if (threads_.size() > initThreadSize_)
					{
						std::cout << "tid" << std::this_thread::get_id() << " exiting due to idle timeout." << std::endl;
						idleThreadSize_--; // ȷ�������̼߳�����ȷ
						threads_.erase(threadId); // ���̳߳����Ƴ���ǰ�߳�
						return; // �˳���ǰ�߳�
					}
				}
			}
		}
	
		idleThreadSize_--;
		std::cout << "tid" << std::this_thread::get_id() << " got a task." << std::endl;
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
			// ִ�����񣬰����񷵻�ֵsetVal��Result����
			task->exec();
		}
		catch (const std::exception& e) {
			std::cerr << "Task exception: " << e.what() << std::endl;
		}
		catch (...) {
			std::cerr << "Unknown task exception." << std::endl;
		}
		// ����ִ����ϣ��߳̿��Լ����ȴ���һ������
		std::cout << "tid" << std::this_thread::get_id() << " finished a task." << std::endl;
		idleThreadSize_++;
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

	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& threads_.size() < threadSizeThreshHold_)
	{
		std::cout << "Creating new thread to handle task." << std::endl;
		// ����ǻ���ģʽ��������������е������������ڿ����߳������������߳�����С����ֵ
		// �򴴽��µ��߳�����������
		auto threadPtr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		int threadId = threadPtr->getId();
		threads_.insert(std::make_pair(threadId, std::move(threadPtr)));
		threads_[threadId]->start();
		idleThreadSize_++;
	}

	// ��Result���ݸ�Task������
	// ����Ҫ������Resultֱ�ӷ��ظ��û���ͬʱ��Ҫ��Result������Task�����������ѵ�
	return Result(task);
}

/////////// ������Thread���ʵ��

Thread::Thread(ThreadFunc func)
	: func_(func)
	, threadId_(generateNum++) // ����Ψһ���߳�ID
{ }

Thread::~Thread(){}

void Thread::start()
{
	// �����߳���ִ��func_
	std::thread t(func_, threadId_);
	t.detach(); 
}

//////////// ������Task���ʵ��
Task::Task() 
	:result_(nullptr)
{}

void Task::exec()
{
	// ִ�������run��������̬
	if(result_ != nullptr)
		result_->setVal(run());
}

void Task::setResult(Result* res)
{
	result_ = res;
}

//////////// ������Result���ʵ��
Result::Result(std::shared_ptr<Task> task, bool isValid)
	: task_(task),
	isValid_(isValid)
{
	task_->setResult(this); // ��������Ľ��ָ��Ϊ��ǰResult����
}
Result::~Result() {
	// Result��ջ�϶��󣬺ܿ��ܱ���ǰ����������������ִ���꣬��ʱ�ͻ��������ָ������
	task_->setResult(nullptr);// �������Ľ��ָ�룬��������ָ��!!!
}

Any Result::get()
{
	// �û����ã���ȡ���񷵻�ֵ�����ǿ���û��ִ���꣬��Ҫ�ȴ�
	if (!isValid_) {
		return "";
	}
	sem_.wait();  //�����û�м����꣬���������ȴ�
	return std::move(data_);
}

void Result::setVal(Any data)
{
	//  �洢task�ķ���ֵ��������ִ�������������
	data_ = std::move(data);
	sem_.post(); // ֪ͨ�ȴ����̣߳������Ѿ�׼������
}