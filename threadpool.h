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

/*
Any 类型
可以接受任意类型的对象
且Any不是模板类
*/
class Any {
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
	// 1. 首先要能接受任意类型的实例  ===》 函数模板
	template<typename T>
	Any(T data):base_(std::make_unique<Derive<T>>(data)){}

	// 3. 要能提取任意类型的实例 ==> 智能指针get()+类型转换dynamic_cast<>()
	template<typename T>
	T cast() const {
		Derive<T>* derive = dynamic_cast<Derive<T>*>(base_.get());// 智能指针使用get()获得裸指针
		if (derive == nullptr) {
			throw "Bad cast: type mismatch in Any::cast()";
		}
		return derive->get();
	}
private:
	// 2. 要能存储任意类型的实例 ==> 类模板 + 多态
	// 此处如果只用一个类模板，则无法将其转为成员变量，也就无法存储在Any类中
	class Base {
	public:
		virtual ~Base() = default;// 继承时需要虚析构函数
	private:
	};
	template<typename T>
	class Derive : public Base {
	public:
		Derive(T t) : data_(t) {}
		~Derive() = default;
		T& get(){ return data_; }
	private:
		T data_;
	};
	std::unique_ptr<Base> base_;
};

class Semaphore
{
public:
	Semaphore(int limit = 0) :resLimit_(limit) {}
	~Semaphore() = default;

	void wait() {
		// 获取一个信号量资源
		std::unique_lock<std::mutex> lock(mtx_);
		// 阻塞在条件变量上的线程被唤醒
		// 1. 重新获取锁，如果没有获取到锁，则继续等待
		// 2. 条件变量被唤醒后，检查条件是否满足，如果不满足，则继续等待
		// 3. 条件满足后，开始执行临界区代码
		cond_.wait(lock, [this]()->bool {return resLimit_ > 0; });// 此处是安全的
		resLimit_--;
	}

	void post() {
		// 增加一个信号量资源
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

class Task;//前置声明

class Result {
// 读者写者
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result();
	void setVal(Any any);
	Any get();
	Result(Result&&) = default;
	Result& operator=(Result&&) = default;
	Result(const Result&) = delete;
	Result& operator=(const Result&) = delete;
private:
	Any data_; // 存储任务的返回值
	Semaphore sem_; // 线程通信信号量
	// 任务指针，用于将该Result传递给Task对象，使得子线程能访问并将结果记录
	std::shared_ptr<Task> task_; // 一般任务指针比结果指针生命周期短，所以需要使用共享指针来延长Task对象周期
	std::atomic_bool isValid_;// 返回值是否有效

};

// 任务抽象类，所有任务都继承此
class Task {
public:
	Task();
	virtual Any run() = 0;
	void setResult(Result* res);
	void exec();
private:
	Result* result_; // 不能仅仅存储Result对象
};

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
	Result submitTask(std::shared_ptr<Task> task); //生产任务

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

	std::queue<std::shared_ptr<Task>> taskQue_;
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
