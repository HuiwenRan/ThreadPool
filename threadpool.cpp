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
		std::cout << "tid" << std::this_thread::get_id() << " waiting for task." << std::endl;
		notEmpty_.wait(lock, [this]()->bool {return !taskQue_.empty(); });
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
	// ��Result���ݸ�Task������
	// ����Ҫ������Resultֱ�ӷ��ظ��û���ͬʱ��Ҫ��Result������Task�����������ѵ�
	return Result(task);
}

/////////// ������Thread���ʵ��

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