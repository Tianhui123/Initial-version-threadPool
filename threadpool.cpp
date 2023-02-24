#include"threadpool.h"


//////////////////////////线程池类///////////////////////

ThreadPool::ThreadPool() :
	maxThreadSize_(MAX_THREAD_SIZE),
	maxTaskSize_(MAX_TASK_SIZE),
	taskSize_(0),
	mode_(Mode::FIXED)
{


}

ThreadPool::~ThreadPool()
{

	if (!threadPool_.empty())
	{
		for (auto& i : threadPool_)
		{
			if (i) delete i;
			i = nullptr;
		}
	}


}

void ThreadPool::setThreadSize(int size)
{
	maxThreadSize_ = size;
}

void ThreadPool::setTaskSize(int size)
{
	maxTaskSize_ = size;
}

void ThreadPool::setPoolMode(Mode mode)
{
	mode_ = mode;
}

Result ThreadPool::submit(Task* task)
{

	{
		std::unique_lock<std::mutex>lock(mutexPool_);

		if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool { return taskSize_ < maxTaskSize_; }))
		{

			switch (mode_)
			{
			case Mode::FIXED:

				std::cerr << "任务队列已满，任务提交失败" << std::endl;
				throw std::runtime_error("submit fail !");

				break;

			case Mode::CATCH:	// 线程增长模式下

				// 需要增加的线程个数
				size_t n = 2;

				size_t size = threadPool_.size();


				// 任务太多，线程不够用，这时增加线程
				for (size_t i = size; i < n + size; ++i)
				{
					threadPool_.push_back(new Thread(std::bind(&ThreadPool::taskFunction, this)));

					threadPool_[i]->star();
				}
				break;


			}

		}

		taskQueue_.emplace(task);
	}


	++taskSize_;

	notEmpty_.notify_all();

	return std::move(*(task->getResult()));

}

void ThreadPool::start(int size)
{
	// 创建线程
	for (int i = 0; i < size; ++i)
	{
		threadPool_.push_back(new Thread(std::bind(&ThreadPool::taskFunction, this)));
	}

	// 启动线程
	for (int i = 0; i < size; ++i)
	{
		threadPool_[i]->star();
	}


}

void ThreadPool::taskFunction()
{

	for (;;)
	{

		Task* temp = nullptr;

		{
			std::unique_lock<std::mutex>lock(mutexPool_);

			// 线程增长模式下，抢到锁之后进行判断，如果任务队列不满 并且此时已经分配了多余线程
			if (mode_ == Mode::CATCH && maxThreadSize_ < threadPool_.size() && taskSize_ < maxTaskSize_)
			{
				//printf("删除线程");
				for (size_t i = threadPool_.size() - 1; i >= maxThreadSize_; --i)
				{
					threadPool_[i]->~Thread();
					threadPool_[i] = nullptr;

					threadPool_.pop_back();

				}

			}


			notEmpty_.wait(lock, [&]()->bool { return taskSize_ > 0; });

			temp = taskQueue_.front();
			taskQueue_.pop();

			--taskSize_;
			if (taskSize_ > 0)
			{
				notEmpty_.notify_all();
			}

			notFull_.notify_all();

		}

		if (temp)
		{

			temp->Task::exac();

		}


	}


}



/////////////////////////Task类///////////////////////

Task::Task() :
	result_(new Result(this))
{}

Task::~Task()
{

	if (result_)
	{

		free(result_);
		result_ = nullptr;

	}


}

Result* Task::getResult()
{
	if (!result_)
		throw std::runtime_error("无返回结果");
	return result_;

}

void Task::exac()
{

	if (result_)
		result_->setData(new Any(run()));

}


/////////////线程类//////////////

Thread::Thread(std::function<void()> func)
	:_func(func)
{}

Thread::~Thread()
{
	std::cout << "the thread is over"<<std::endl;

}

void Thread::star()
{
	std::thread t(_func);

	t.detach();

}




//////////////////////信号量////////////////

Semaphore::Semaphore() :
	count_(0)，
	isExit(false)
{

}

Semaphore::~Semaphore()
{
	isExit = true;

}

Semaphore::Semaphore(const Semaphore& p)
{
	count_ = p.count_;
	isExit = p.isExit;
}

Semaphore::Semaphore(Semaphore&& p)noexcept :
	count_(p.count_),
	isExit(p.isExit)
{

}

Semaphore& Semaphore::operator=(const Semaphore& p)
{
	count_ = p.count_;
	return *this;
}

Semaphore& Semaphore::operator=(Semaphore&& p)noexcept
{
	count_ = p.count_;

	return *this;
	// TODO: 在此处插入 return 语句
}

void Semaphore::wait()
{
	if (isExit)
		return;
	std::unique_lock<std::mutex>lock(mutex_);
	cond_.wait(lock, [&]()->bool {return count_ > 0; });
	--count_;

}

void Semaphore::post()
{
	if (isExit)
		return;

	std::unique_lock<std::mutex>lock(mutex_);
	++count_;
	cond_.notify_all();

}


/////////////////////////Any类//////////////////////////

Any::~Any()
{
	if (dataPtr)
		delete dataPtr;
	dataPtr = nullptr;

}



Any::Any(Any&& p)noexcept :
	dataPtr(nullptr)
{
	//if (p!=nullptr)
	dataPtr = p.dataPtr;
	p.dataPtr = nullptr;

}


Any& Any:: operator=(Any&& p)noexcept
{
	dataPtr = p.dataPtr;
	p.dataPtr = nullptr;
	return *this;
}


////////////////////////Result/////////////////////////

Result::Result() :
	data_(nullptr),
	task_(nullptr),
	sem_(new Semaphore{})
{

}

Result::Result(Task* task) :
	task_(task),
	data_(nullptr),
	sem_(new Semaphore{})
{

	task = nullptr;

}



Result::~Result()
{

	if (data_)
		delete data_;
	data_ = nullptr;

	if (task_)
		delete task_;
	task_ = nullptr;

	if (sem_)
		delete sem_;
	sem_ = nullptr;

}

Result::Result(Result&& p)noexcept :
	data_(p.data_),
	task_(p.task_),
	sem_(p.sem_)
{
	if (this == &p)
		return;



}

Result& Result::operator=(Result&& p)noexcept
{
	if (this == &p)
		return *this;

	data_ = p.data_;
	task_ = p.task_;

	p.task_ = nullptr;
	p.data_ = nullptr;

	return *this;
	// TODO: 在此处插入 return 语句
}

//获取结果，若是结果没有返回就在当前线程阻塞
Any& Result::get()
{
	if (!data_)
	{
		// 外部调用get函数，如果还未执行完毕就在此阻塞 
		sem_->wait();

		data_ = (task_->getResult()->data_);

		(task_->getResult()->data_) = nullptr;
	}


	return *data_;

}


//设置Result存储的数据
void Result::setData(Any* data)
{

	data_ = data;
	data = nullptr;

	sem_->post();

}

