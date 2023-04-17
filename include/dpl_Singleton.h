#pragma once


#include "dpl_GeneralException.h" // TODO: remove
#include <typeindex>
#include <unordered_map>
#include <any>


// declarations
namespace dpl
{
	class Multition;

	template<class T>
	class Singleton;
}

// implementations
namespace dpl
{
	/*
		This class acts as a context for all singleton types.
	*/
	class Multition
	{
	public: // friends
		template<typename>
		friend class Singleton;

	private: // data
		std::unordered_map<std::type_index, void*> m_singletonTypes;

	public: // lifecycle
		CLASS_CTOR		Multition() = default;

	private: // lifecycle
		CLASS_CTOR		Multition(			const Multition& OTHER) = delete;
		CLASS_CTOR		Multition(			Multition&&		other) = delete;
		Multition&		operator=(			const Multition& OTHER) = delete;
		Multition&		operator=(			Multition&&		other) = delete;

	private: // functions
		template<typename SingletonT>
		void*			get()
		{
			auto it = m_singletonTypes.find(typeid(SingletonT));
			if (it == m_singletonTypes.end()) return nullptr;
			return it->second;
		}

		template<typename SingletonT>
		void			register_instance(	SingletonT*		INSTANCE)
		{
			if (!m_singletonTypes.emplace(typeid(SingletonT), INSTANCE).second)
			{
				throw dpl::GeneralException(__FILE__, __LINE__, "Fail to register singleton. Given type already registered: ", typeid(SingletonT).name());
			}
		}

		template<typename SingletonT>
		void			unregister_instance(SingletonT*		INSTANCE)
		{
			auto it = m_singletonTypes.find(typeid(SingletonT));
			if (it != m_singletonTypes.end())
			{
				if (it->second == INSTANCE)
				{
					m_singletonTypes.erase(it);
					return;
				}
			}

			throw dpl::GeneralException(__FILE__, __LINE__, "Fail to unregister singleton. Unknown type: %s", typeid(SingletonT).name());
		}
	};


	/*
		Assures that only one instance of the given type is created.
		Note: In case of the DLL, class T should synchronize its resources with the multition.

		TODO: @at -rdynamic flag
	*/
	template<class T>
	class Singleton
	{
	private: // data
		static Singleton*	sm_instance;
		Multition*			m_owner;

	protected: // lifecycle
		CLASS_CTOR			Singleton(		Multition&			multition)
			: m_owner(&multition)
		{
			multition.register_instance<T>(static_cast<T*>(this));
			sm_instance = this;
		}

		CLASS_DTOR			~Singleton()
		{
			dpl::no_except([&]()
			{
				if (m_owner)
				{
					m_owner->unregister_instance<T>(static_cast<T*>(this));
					sm_instance = nullptr;
					m_owner		= nullptr;
				}
			});
		}

	private: // lifecycle
		CLASS_CTOR			Singleton(		const Singleton&	OTHER) = delete;
		CLASS_CTOR			Singleton(		Singleton&&			other) = delete;
		Singleton&			operator=(		const Singleton&	OTHER) = delete;
		Singleton&			operator=(		Singleton&&			other) = delete;

	public: // access
		static inline T*	ptr()
		{
			return static_cast<T*>(sm_instance);
		}

		static inline T*	get()
		{
			return static_cast<T*>(sm_instance);
		}

		static inline T&	ref()
		{
			return static_cast<T&>(*sm_instance);
		}

	public: // functions
		inline Multition&	owner() const
		{
			return *m_owner;
		}

	protected: // functions
		static inline void	synchronize(	Multition&			multition)
		{
			sm_instance = static_cast<Singleton<T>*>(multition.get<T>());
		}
	};

	template<class T>
	Singleton<T>*	Singleton<T>::sm_instance	= nullptr;
}