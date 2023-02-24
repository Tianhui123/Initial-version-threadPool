#ifndef TH
#define TH

#include<mutex>
#include<vector>
#include<queue>
#include<functional>
#include<thread>
#include<condition_variable>
#include<iostream>
#include<atomic>

const int MAX_THREAD_SIZE = 1024;	// 默认线程最大数量

const int MAX_TASK_SIZE = 1024;		// 默认任务的最大数量

class Semaphore
{
public:

	Semaphore();

	~Semaphore();

	Semaphore(const Semaphore&);

	Semaphore(Semaphore&&)noexcept;

	Semaphore& operator=(const Semaphore&);

	Semaphore& operator=(Semaphore&&)noexcept;
	//一直等待，知道有空余信号量的产生
	void wait();
	
	//增加一个信号量
	void post();

private:
	std::atomic_bool isExit;		// 判断是否退出
	std::mutex mutex_;				// 互斥锁
	std::condition_variable cond_;	// 条件变量
	int count_;						// 计数
};



// 接收任意类型的数据
class Any
{
public:
	template<typename Ty>
	Any(Ty &&type):
		dataPtr(new son<Ty>{std::forward<Ty>( type)})
	{}


	~Any();

	/*template<typename T>
	Any(const Any&p):
		dataPtr(nullptr)
	{
		T&& temp = p.dataPtr->data_;

		dataPtr = new son<T>{ temp };

	}

	template<typename T>
	Any&operator=(const Any& p) :
		dataPtr(nullptr)
	{
		T&& temp = p.dataPtr->data_;

		dataPtr = new son<T>{ temp };

		return *this;
	}*/

	Any(Any&&)noexcept;

	Any& operator=(Any&&)noexcept;

	// 填入函数返回值的类型返回结果
	template<typename Ty>
	Ty cast()
	{
		son<Ty>* temp = dynamic_cast<son<Ty>*>(dataPtr);
		if (!temp)
		{
			throw std::runtime_error("获取类型不匹配");
		}

		return temp->data_;

	}

private:
	//定义一个父类指针来接受任意类型
	class parent
	{
	public:
		parent() = default;
		virtual ~parent() = default;

	};

	//用子类模板接受任意数据，再用父类指针进行操作
	template<typename Ty>
	class son :public parent
	{
	public:
		son(Ty &&data) :
			data_(data)
		{}

		~son()=default;

	public:
		Ty data_;			// 存储的任意类型数据
	};


private:
	parent* dataPtr;

};

class Task;
//实现线程间数据传输获取
class Result
{
public:
	Result();
	Result(Task* task);

	~Result();

	Result(const Result&)=delete;
	Result& operator=(const Result&)=delete;

	Result(Result&&)noexcept;
	
	Result& operator=(Result&&)noexcept;

	// 获取存储结果
	Any& get();


	// 设置存储的结果
	void setData(Any*);

	

private:

	Any *data_;					// 存储的任意类型数据 返回值

	Semaphore* sem_;			// 信号量，用于两个线程之间的交流

	Task* task_;				// 关联任务和返回结果

};


enum class Mode
{
	FIXED,
	CATCH
};


class Task
{
public:
	Task();

	virtual ~Task();

	//多态，线程任务的运行函数
	virtual Any run() = 0;

	Result* getResult();

	// 因为函数运行后的结果需要赋值给私有成员result_，无法在外部访问该私有成员
	// 因此用函数将其包装起来
	void exac();
	
private:

	Result* result_;

};


class Thread
{
public:
	Thread(std::function<void()> &func);
	~Thread();
	void star();


private:
	std::function<void()> _func;
};


class ThreadPool
{

public:
	ThreadPool();						// 创建线程池

	~ThreadPool();						// 销毁线程池

	void setThreadSize(int size);		// 设置线程池里线程的最大的数量

	void setTaskSize(int size);			// 设置任务队列里任务的最大数量

	void setPoolMode(Mode mode);		// 设置线程池的模式

	Result submit(Task * task);			// 提交任务进行运行

	template<typename _Ty, typename ..._list>
	void submitTask(_Ty(&& _callback), _list && ...t);

	void start(int size=4);				// 启动线程池并设置启动的线程个数

private:

	void taskFunction();				// 处理任务列表

private:

	std::mutex mutexPool_;				// 线程池锁

	int maxThreadSize_;					// 线程池里线程的最大的数量

	int maxTaskSize_;					// 任务队列里任务的最大的数量

	int taskSize_;						// 目前任务队列里面的任务数量

	Mode mode_;							// 线程工作模式

	std::vector<Thread*> threadPool_;	// 线程池

	std::queue<Task*> taskQueue_;		// 任务队列

	std::condition_variable notEmpty_;	// 任务队列不空信号量

	std::condition_variable notFull_;	// 任务队列不满信号量

};








template<typename _Ty, typename ..._list>
inline void ThreadPool::submitTask(_Ty(&& _callback), _list && ...t)
{

	std::thread _tr(std::forward<_Ty>(_callback), std::forward<_list>(t)...);
	_tr.detach();

	return;
}


#endif

