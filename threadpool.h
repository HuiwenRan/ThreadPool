#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <atomic>
#include <future>

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
const int IdelTimeout = 2; // �����̳߳�ʱʱ�䣬��λ��

enum class PoolMode {
	MODE_FIXED,
	MODE_CACHED
};

class Thread {
public:
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc);
	~Thread();
	void start();
	inline static int generateNum = 0; //�������������߳�ID
	int getId() const { return threadId_; }
private:
	ThreadFunc func_;
	int threadId_;
};

class ThreadPool{
public:
	ThreadPool();
	~ThreadPool();
	void start(int initThreadSize = 4);

	void setMode(PoolMode mode);
	void setThreadSizeThreshHold(int threshHold);
	void setTaskQueMaxThreshHold(int threshHold);

	// ʹ�ÿɱ��ģ���̣���submitTask���Խ������⺯�������������Ĳ���
	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		// ��ʹ��std::packaged_task�������Ͳ��������һ������
		// ��������ֱ�ӻ������ķ���
		using ReturnType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<ReturnType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
		);

		std::unique_lock<std::mutex> lock(taskQueMtx_);

		if (!notFull_.wait_for(lock,
			std::chrono::seconds(1),
			[this]()->bool {return taskSize_ < taskQueMaxThreshHold_; }))
		{
			std::cerr << "Task queue is full, cannot submit task." << std::endl;
			auto task = std::make_shared<std::packaged_task<ReturnType()>>(
				[]()->ReturnType {
					return ReturnType();
				});
			(*task)();
			return task->get_future();
		}
		// �����м����ʹ������ͳһΪvoid()����
		taskQue_.emplace([task](){(*task)(); });

		taskSize_++;

		notEmpty_.notify_all();

		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& threads_.size() < threadSizeThreshHold_)
		{
			std::cout << "Creating new thread to handle task." << std::endl;
			// ����ǻ���ģʽ��������������е������������ڿ����߳������������߳�����С����ֵ
			// �򴴽��µ��߳�����������
			auto threadPtr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int threadId = threadPtr->getId();
			threads_.insert(std::make_pair(threadId, std::move(threadPtr)));
			threads_[threadId]->start();
			idleThreadSize_++;
		}

		// ��Result���ݸ�Task������
		// ����Ҫ������Resultֱ�ӷ��ظ��û���ͬʱ��Ҫ��Result������Task�����������ѵ�
		return task->get_future();
	}

	ThreadPool(const ThreadPool& threadPool) = delete;
	ThreadPool& operator=(const ThreadPool& threadPool) = delete;

private:
	// vector�ڳ����������Զ�����Ԫ�ص������������ͷ��ڴ�
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;
	int initThreadSize_;
	std::atomic<int> idleThreadSize_;
	int threadSizeThreshHold_;

	void threadFunc(int); //�̳߳�ָ���߳�ִ�еĺ�����������������

	bool checkRunningState() const {
		return isPoolrunning_;
	}


	// Task ����==����������
	using Task = std::function<void()>;
	std::queue<Task> taskQue_;
	std::atomic_int taskSize_;
	int taskQueMaxThreshHold_;

	std::mutex taskQueMtx_;
	std::condition_variable notFull_;
	std::condition_variable notEmpty_;
	std::condition_variable exitCond_;

	PoolMode poolMode_;
	std::atomic<bool> isPoolrunning_;
};

#endif // THREADPOOL_H
