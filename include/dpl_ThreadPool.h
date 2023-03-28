#pragma once


#include <queue>
#include <vector>
#include <condition_variable>
#include <future>
#include <type_traits>
#include <thread>
#include <memory>
#include <mutex>
#include "dpl_ReadOnly.h"
#include "dpl_DynamicArray.h"
#include "dpl_Logger.h"


#pragma warning(disable : 26812)
#pragma warning(disable : 6385)

namespace dpl
{
	namespace Thread
	{
		/*
			Returns true if variable stored in the future is ready to use.
			See@ https://stackoverflow.com/questions/10890242/get-the-status-of-a-stdfuture
		*/
		template<typename T>
		inline bool is_ready(std::future<T>& future)
		{
			static const auto NOW = std::chrono::system_clock::time_point::min();
			return future.wait_until(NOW) == std::future_status::ready;
		}
	}

	class ThreadPool
	{
	public: // subtypes
		class	Error
		{
		public: // data
			ReadOnly<uint32_t,		Error> workerID;
			ReadOnly<std::string,	Error> message;
			
		public: // lifecycle
			CLASS_CTOR Error(	const uint32_t		WORKER_ID,
								const std::string	MESSAGE)
				: workerID(WORKER_ID)
				, message(MESSAGE)
			{

			}
		};

		using	Task			= std::function<void()>;
		using	NativeHandle	= std::thread::native_handle_type;
		using	ErrorCallback	= std::function<void(const Error&)>;

	private: // data
		mutable std::mutex			m_mtx;
		std::thread::id				m_mainThreadID;
		std::condition_variable		m_finished; // Notifies when worker finishes the task.
		std::condition_variable		m_order;	// Notifies when there is a job to do, or when thread pool needs to terminate.
		std::queue<Task>			m_tasks;
		std::vector<Error>			m_errors;
		size_t						m_numTasks; // Number of tasks in the queue + number of tasks performed by workers
		size_t						m_numWorkers;
		bool						bTerminate;

	public: // lifecycle
		CLASS_CTOR							ThreadPool(							const uint32_t			NUM_THREADS = std::thread::hardware_concurrency())
			: m_mainThreadID(std::this_thread::get_id())
			, m_numTasks(0)
			, m_numWorkers(0)
			, bTerminate(false)
		{
			start(NUM_THREADS);
		}

		CLASS_CTOR							ThreadPool(							const ThreadPool&		OTHER) = delete;

		CLASS_CTOR							ThreadPool(							ThreadPool&&			other) noexcept = delete;

		CLASS_DTOR							~ThreadPool()
		{
			stop();
		}

		ThreadPool&							operator=(							const ThreadPool&		OTHER) = delete;

		ThreadPool&							operator=(							ThreadPool&&			other) noexcept = delete;
			
	public: // functions
		inline size_t						get_numWorkers() const
		{
			std::lock_guard<std::mutex> lk(m_mtx);
			return m_numWorkers;
		}

		inline size_t						get_numTasks() const
		{
			std::lock_guard<std::mutex> lk(m_mtx);
			return m_numTasks;
		}

		inline void							add_task(							Task					task)
		{
			{std::lock_guard lk(m_mtx);
				m_tasks.emplace(std::move(task));
				++m_numTasks;
			}

			m_order.notify_one();
		}

		/*
			Creates new task and adds it to the queue.
			Returns std::future.

			For member function use:
				create_task(&Class::method, &Obj, args...);
		*/
		template<typename F, typename... Args>
		auto								create_task(						F&&						function, 
																				Args&&...				args)
		{
			using ret_t = typename std::invoke_result_t<F&&, Args&&...>;

			/// Using a conditional wrapper to avoid dangling references.
			/// Courtesy of https://stackoverflow.com/a/46565491/4639195.
			
			auto task = std::make_shared<std::packaged_task<ret_t()>>(std::bind(std::forward<F>(function), wrap(std::forward<Args>(args))...));

			std::future<ret_t> result(task->get_future());

			add_task([=] { (*task)(); });
			return result;
		}

		void								wait(								const ErrorCallback&	ERROR_CALLBACK = &log_and_throw_first_worker_error)
		{
#ifdef _DEBUG
			if(std::this_thread::get_id() != m_mainThreadID)
				push_error(std::numeric_limits<uint32_t>::max(), "ThreadPool::wait must be called in the main thread.");
#endif // _DEBUG

			std::unique_lock lk(m_mtx);

			if(m_numTasks > 0)
			{
				m_finished.wait(lk, [&] 
				{
					return m_numTasks == 0 || bTerminate;
				});
			}

			if(!m_errors.empty())
			{
				if(ERROR_CALLBACK)
				{
					for(const auto& ERROR : m_errors)
					{
						ERROR_CALLBACK(ERROR);
					}
				}
				m_errors.clear();
			}
		}

	public: // error handling
		static void							log_and_throw_first_worker_error(	const Error&			ERROR)
		{
			throw dpl::Logger::ref().push_error("Worker[%d] failed: %s", ERROR.workerID, ERROR.message().c_str());
		}

	private: // functions
		template <class T>
		inline std::reference_wrapper<T>	wrap(								T&						val)
		{
			return std::ref(val);
		}

		template <class T>
		inline T&&							wrap(								T&&						val)
		{
			return std::forward<T>(val);
		}

		inline void							start(								const uint32_t			NUM_THREADS)
		{
			for (uint32_t workerID = 0; workerID < NUM_THREADS; ++workerID) 
			{
				add_worker(workerID);
			}
		}

		inline void							add_worker(							const uint32_t			WORKER_ID)
		{
			std::thread([&, WORKER_ID]
			{
				{std::lock_guard lk(m_mtx);
					++m_numWorkers;
				}

				while(true)
				{
					std::unique_lock lock(m_mtx);

					m_order.wait(lock, [&] 
					{
						return !m_tasks.empty() || bTerminate;
					});

					if (bTerminate) break;

					std::function<void()> task = std::move(m_tasks.front());
					m_tasks.pop();

					lock.unlock();
					try
					{
						task();
					}
					catch(const std::runtime_error& EXCEPTION)
					{
						push_error(WORKER_ID, EXCEPTION.what());
					}
					catch(...)
					{
						push_error(WORKER_ID, "ThreadPool: Unknown exception");
					}
					lock.lock();

					--m_numTasks;
					m_finished.notify_all();
					if (bTerminate) break;
				}

				notify_worker_release();

			}).detach();
		}

		void								notify_worker_release()
		{
			std::unique_lock lock(m_mtx);
			if(m_numWorkers > 0)
			{
				--m_numWorkers;
				lock.unlock();
				m_finished.notify_all();
			}
		}

		inline void							push_error(							const uint32_t			WORKER_ID,
																				const char*				MESSAGE)
		{
			{std::lock_guard<std::mutex> lk(m_mtx);
				m_errors.emplace_back(WORKER_ID, MESSAGE);
				bTerminate = true;
			}

			m_order.notify_all();
		}

		void								stop()
		{
			std::unique_lock lk(m_mtx);
			bTerminate = true;
			lk.unlock();

			m_order.notify_all();

			lk.lock(); // Wait until all workers are released.
			if(m_numWorkers > 0)
			{
				m_finished.wait(lk, [&] 
				{
					return m_numWorkers == 0;
				});
			}
		}
	};

	/*
		ThreadPool optimization for large amount of tasks.

		TODO:
		- Do not derive from ThreadPool
		- Disable adding task while threads are running.
		- Notify all threads with their own tasks.
		- Threads should increment work counter while they start and decrement it when they finish.
		- Main thread should wait on the semaphore until counter reaches 0.
	*/
	class ParallelPhase : private ThreadPool
	{
	public: // subtypes
		using	Task			= ThreadPool::Task;
		using	Error			= ThreadPool::Error;
		using	ErrorCallback	= ThreadPool::ErrorCallback;

		struct	Job
		{
			std::vector<Task>	tasks;
			uint64_t			rating = 0;

			inline void add_task(const uint32_t RATING, Task task)
			{
				tasks.emplace_back(task);
				rating += RATING;
			}
		};

	public: // data
		ReadOnly<uint32_t, ParallelPhase> numTasks;

	private: // data
		dpl::DynamicArray<Job>		jobs;
		dpl::DynamicArray<uint32_t>	workOrder;

	public: // lifecycle
		CLASS_CTOR		ParallelPhase(	const uint32_t			NUM_THREADS = std::thread::hardware_concurrency())
			: ThreadPool(NUM_THREADS)
			, numTasks(0)
		{
			jobs.resize(NUM_THREADS);
			workOrder.resize(jobs.size());
			for(uint32_t index = 0; index < jobs.size(); ++index)
			{
				workOrder[index] = index;
			}
		}

	public: // functions
		inline uint32_t numJobs() const
		{
			return jobs.size();
		}

		void			reserve_tasks(	const size_t			NUM_TASKS)
		{
			const size_t CAPACITY = 2 * (1 + NUM_TASKS/jobs.size());
			jobs.for_each([&](Job& job)
			{
				job.tasks.reserve(CAPACITY);
			});
		}

		/*
			Add tasks with user defined rating of time complexity.
		*/
		inline void		add_task(		const uint32_t			RATING, 
										Task					task)
		{
			jobs[workOrder[0]].add_task(RATING, task);
			update_work_order();
			++(*numTasks);
		}

		void			start(			const ErrorCallback&	ERROR_CALLBACK = &ThreadPool::log_and_throw_first_worker_error)
		{
			jobs.for_each([&](Job& job)
			{
				ThreadPool::add_task([&]()
				{
					for(auto& iTask : job.tasks)
					{
						iTask();
					}
				});
			});
			
			ThreadPool::wait(ERROR_CALLBACK);

			jobs.for_each([&](Job& job)
			{
				job.tasks.clear();
				job.rating = 0;
			});

			numTasks = 0;
		}

	private: // functions
		void			update_work_order()
		{
			const uint64_t NUM_JOBS = workOrder.size();
			uint32_t nextID = 1;
			while(nextID < NUM_JOBS)
			{
				auto& current	= workOrder[nextID-1];
				auto& next		= workOrder[nextID];

				if(jobs[current].rating > jobs[next].rating)
				{
					std::swap(current, next);
					++nextID;
				}
				else
				{
					break;
				}
			}
		}
	};
}

#pragma warning(default : 26812)