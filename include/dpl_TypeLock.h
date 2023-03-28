#pragma once


#include <mutex>
#include <atomic>


namespace dpl
{
	/*
		Thread lock associated with a class type T.
	*/
	template<typename T>
	class TypeLock
	{
	private: // data
		static std::mutex						sm_mutex;
		static std::atomic<std::thread::id>		sm_owner;
		std::lock_guard<std::mutex>				m_glock;

	public: // lifecycle
		CLASS_CTOR	TypeLock()
			: m_glock(sm_mutex)
		{
			sm_owner = std::this_thread::get_id();
		}

		CLASS_CTOR	TypeLock(	const TypeLock&	OTHER) = delete;

		CLASS_CTOR	TypeLock(	TypeLock&&		other) noexcept = delete;

		CLASS_DTOR	~TypeLock()
		{
			sm_owner = std::thread::id();
		}

		TypeLock&	operator=(	const TypeLock& OTHER) = delete;

		TypeLock&	operator=(	TypeLock&&		other) noexcept = delete;

	public: // functions
		static bool is_owned_by_current_thread()
		{
			return sm_owner == std::this_thread::get_id();
		}
	};


	template<typename T>
	std::mutex	TypeLock<T>::sm_mutex;
}