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
const int IdelTimeout = 2; // 空闲线程超时时间，单位秒

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
	inline static int generateNum = 0; //用来辅助生成线程ID
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

	// 使用可变参模板编程，让submitTask可以接受任意函数和任意数量的参数
	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		// 先使用std::packaged_task将函数和参数打包成一个任务
		// 这样可以直接获得任务的返回
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
		// 增加中间件，使得任务统一为void()对象
		taskQue_.emplace([task](){(*task)(); });

		taskSize_++;

		notEmpty_.notify_all();

		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& threads_.size() < threadSizeThreshHold_)
		{
			std::cout << "Creating new thread to handle task." << std::endl;
			// 如果是缓存模式，并且任务队列中的任务数量大于空闲线程数量，并且线程数量小于阈值
			// 则创建新的线程来处理任务
			auto threadPtr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int threadId = threadPtr->getId();
			threads_.insert(std::make_pair(threadId, std::move(threadPtr)));
			threads_[threadId]->start();
			idleThreadSize_++;
		}

		// 将Result传递给Task！！！
		// 这里要将对象Result直接返回给用户，同时还要将Result对象与Task关联起来，难点
		return task->get_future();
	}

	ThreadPool(const ThreadPool& threadPool) = delete;
	ThreadPool& operator=(const ThreadPool& threadPool) = delete;

private:
	// vector在出作用域后会自动调用元素的析构函数，释放内存
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;
	int initThreadSize_;
	std::atomic<int> idleThreadSize_;
	int threadSizeThreshHold_;

	void threadFunc(int); //线程池指定线程执行的函数！！！消费任务

	bool checkRunningState() const {
		return isPoolrunning_;
	}


	// Task 任务==》函数对象
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
