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
Any ����
���Խ����������͵Ķ���
��Any����ģ����
*/
class Any {
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
	// 1. ����Ҫ�ܽ����������͵�ʵ��  ===�� ����ģ��
	template<typename T>
	Any(T data):base_(std::make_unique<Derive<T>>(data)){}

	// 3. Ҫ����ȡ�������͵�ʵ�� ==> ����ָ��get()+����ת��dynamic_cast<>()
	template<typename T>
	T cast() const {
		Derive<T>* derive = dynamic_cast<Derive<T>*>(base_.get());// ����ָ��ʹ��get()�����ָ��
		if (derive == nullptr) {
			throw "Bad cast: type mismatch in Any::cast()";
		}
		return derive->get();
	}
private:
	// 2. Ҫ�ܴ洢�������͵�ʵ�� ==> ��ģ�� + ��̬
	// �˴����ֻ��һ����ģ�壬���޷�����תΪ��Ա������Ҳ���޷��洢��Any����
	class Base {
	public:
		virtual ~Base() = default;// �̳�ʱ��Ҫ����������
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
		// ��ȡһ���ź�����Դ
		std::unique_lock<std::mutex> lock(mtx_);
		// ���������������ϵ��̱߳�����
		// 1. ���»�ȡ�������û�л�ȡ������������ȴ�
		// 2. �������������Ѻ󣬼�������Ƿ����㣬��������㣬������ȴ�
		// 3. ��������󣬿�ʼִ���ٽ�������
		cond_.wait(lock, [this]()->bool {return resLimit_ > 0; });// �˴��ǰ�ȫ��
		resLimit_--;
	}

	void post() {
		// ����һ���ź�����Դ
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

class Task;//ǰ������

class Result {
// ����д��
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
	Any data_; // �洢����ķ���ֵ
	Semaphore sem_; // �߳�ͨ���ź���
	// ����ָ�룬���ڽ���Result���ݸ�Task����ʹ�����߳��ܷ��ʲ��������¼
	std::shared_ptr<Task> task_; // һ������ָ��Ƚ��ָ���������ڶ̣�������Ҫʹ�ù���ָ�����ӳ�Task��������
	std::atomic_bool isValid_;// ����ֵ�Ƿ���Ч

};

// ��������࣬�������񶼼̳д�
class Task {
public:
	Task();
	virtual Any run() = 0;
	void setResult(Result* res);
	void exec();
private:
	Result* result_; // ���ܽ����洢Result����
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
	Result submitTask(std::shared_ptr<Task> task); //��������

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
