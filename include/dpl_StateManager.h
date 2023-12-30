#pragma once


#include <typeindex>
#include <shared_mutex>
#include <optional>
#include "dpl_TypeTraits.h"
#include "dpl_ReadOnly.h"
#include "dpl_Variation.h"
#include "dpl_Logger.h"

#pragma warning( push )
#pragma warning( disable : 26110) //<-- Solves "Caller failing to hold lock..." VS bug.

// declarations
namespace dpl
{
	class Progress;
	class State;
	class StateManager;

	template<typename StateT>
	concept is_State = std::is_base_of_v<State, StateT>;
}

// state implementation
namespace dpl
{
	class	State : private dpl::Variant<StateManager, State>
	{
	public:		// [SUBTYPES]
		template<is_State T>
		using	Invoke	= std::function<void(T&)>;

		using	Binding	= Variant::Binding;

	public:		// [FRIENDS]
		friend	Variant;
		friend	dpl::Variation<StateManager, State>;
		friend	VCTOR_Base;
		friend	StateManager;

	protected:	// [LIFECYCLE]
		CLASS_CTOR			State(			const Binding&		BINDING)
			: Variant(BINDING)
		{
			
		}

	protected:	// [FUNCTIONS]
		void				set_previous_state();

		template<is_State T>
		void				set_next_state(	const Invoke<T>&	INVOKE_STATE = nullptr)
		{
			Variant::get_variation().set_next_state<T>(INVOKE_STATE);
		}

	private:	// [INTERFACE]
		virtual void		begin(			Progress&			progress) = 0;

		virtual void		update(			Progress&			progress) = 0;

		virtual void		end() = 0;
	};


	class	NullState : public State
	{
	public:		// [LIFECYCLE]
		CLASS_CTOR			NullState(	const Binding&	BINDING)
			: State(BINDING)
		{

		}

	private:	// [IMPLEMENTATION]
		virtual void		begin(		Progress&		progress) final override{}
		virtual void		update(		Progress&		progress) final override{}
		virtual void		end() final override{}
	};
}

// Progress & StateManager implementation
namespace dpl
{
	class	Progress
	{
	public:		// [SUBTYPES]
		struct Status
		{
			std::optional<float>	fraction;
			std::string				info;

			CLASS_CTOR Status(	const std::string&	INFO,
								const uint32_t		NUM_STEPS,
								const uint32_t		CURR_STEP)
				: info(INFO)
			{
				if(NUM_STEPS > 0) fraction.emplace((float)CURR_STEP / (float)NUM_STEPS);
			}
		};

	private:	// [DATA]
		uint32_t					m_numSteps;
		uint32_t					m_step;
		std::string					m_info;
		mutable std::shared_mutex	m_mtx;

	public:		// [LIFECYCLE]
		CLASS_CTOR			Progress()
			: m_numSteps(0)
			, m_step(0)
		{

		}

	public:		// [FUNCTIONS]
		void				reset(					const uint32_t		NUM_STEPS		= 0,
													const std::string&	INITIAL_INFO	= "")
		{
			std::lock_guard lock(m_mtx);
			m_numSteps	= NUM_STEPS;
			m_step		= 0;
			m_info		= INITIAL_INFO;
		}

		void				set_step(				const uint32_t		STEP,
													const std::string&	INFO)
		{
			std::lock_guard lock(m_mtx);
			throw_if_invalid_step(STEP);
			m_step = STEP;
			m_info = INFO;
		}

		Status				get() const
		{
			std::lock_guard	lock(m_mtx);
			return Status(m_info, m_numSteps, m_step);
		}

	private:	// [EXCEPTIONS]
		void				throw_if_invalid_step(	const uint32_t		STEP) const
		{
			if(STEP < m_numSteps) return;
			throw dpl::GeneralException(this,__LINE__, "Invalid step: " + std::to_string(STEP) + ", max supported: " + std::to_string(m_numSteps));
		}
	};


	class	StateManager : private dpl::Variation<StateManager, State>
	{
	public:		// [FRIENDS]
		friend State;
		friend Variation;

	public:		// [SUBTYPES]
		using	Action	= std::function<void()>;
		
		template<is_State T>
		using	Invoke	= std::function<void(T&)>;

		using	ULock	= std::unique_lock<std::shared_mutex>;

	public:		// [DATA]
		dpl::ReadOnly<Progress,						StateManager>	progress;

	private:	// [DATA]
		dpl::ReadOnly<uint32_t,						StateManager>	prevID;
		dpl::ReadOnly<uint32_t,						StateManager>	currID;
		dpl::ReadOnly<uint32_t,						StateManager>	nextID;
		dpl::ReadOnly<bool,							StateManager>	switchable;
		mutable dpl::ReadOnly<std::shared_mutex,	StateManager>	mtx;

	public:		// [LIFECYCLE]
		CLASS_CTOR		StateManager()
			: prevID(State::INVALID_INDEX)
			, currID(State::INVALID_INDEX)
			, nextID(State::INVALID_INDEX)
			, switchable(true)
		{
			add_state<NullState>();
			nextID = Variation::get_typeID<NullState>();
		}

	public:		// [FUNCTIONS]
		template<is_State T>
		bool			add_state()
		{
			std::lock_guard lock(*mtx);
			return Variation::create_variant<T>();
		}

		template<is_State... Tn>
		void			add_states()
		{
			std::lock_guard lock(*mtx);
			StateManager::add_states_internal<Tn...>();
		}

		template<is_State T>
		T&				get_state()
		{
			std::lock_guard lock(*mtx);
			return Variation::get_variant<T>();
		}

		template<is_State T>
		const T&		get_state() const
		{
			std::lock_guard lock(*mtx);
			return Variation::get_variant<T>();
		}

		void			set_previous_state()
		{
			std::lock_guard lock(*mtx);
			throw_if_not_switchable();
			nextID = prevID;
		}

		template<is_State T>
		void			set_next_state(				const Invoke<T>&	INVOKE_STATE = nullptr)
		{
			std::lock_guard lock(*mtx);
			throw_if_not_switchable();
			nextID = Variation::get_typeID<T>();
			if(nextID() == currID())	nextID = State::INVALID_INDEX;
			else if(INVOKE_STATE)		INVOKE_STATE(Variation::get_variant<T>());
		}

		template<is_State T>
		bool			is_previous_state() const
		{
			std::lock_guard lock(*mtx);
			return prevID() == Variation::get_typeID<T>();
		}

		template<is_State T>
		bool			is_current_state() const
		{
			std::lock_guard lock(*mtx);
			return currID() == Variation::get_typeID<T>();
		}

		template<is_State T>
		bool			is_next_state() const
		{
			std::lock_guard lock(*mtx);
			return nextID() == Variation::get_typeID<T>();
		}

	protected:	// [FUNCTIONS]
		void			update_states(				dpl::Logger&		logger)
		{
			std::unique_lock lock(*mtx);
			if(nextID() != State::INVALID_INDEX)
			{
				stop_current_state(logger, lock);
				start_next_state(logger, lock);
			}

			log_and_throw_on_exception(logger, [&]()
			{
				if(State* current = Variation::find_base_variant(currID))
				{
					lock.unlock();
					current->update(*progress);
				}
			});	
		}

		void			release_states(				dpl::Logger&		logger)
		{
			std::unique_lock lock(*mtx);
			stop_current_state(logger, lock);
			Variation::destroy_all_variants();
		}

	private:	// [FUNCTIONS]
		void			stop_current_state(			dpl::Logger&		logger,
													ULock&				lock)
		{
			log_and_throw_on_exception(logger, [&]()
			{
				prevID = currID;
				currID = State::INVALID_INDEX;
				progress->reset();

				if(State* previous = Variation::find_base_variant(prevID))
				{
					switchable = false;
					lock.unlock();
					previous->end();
					lock.lock();
					switchable = true;
				}
			});
		}

		void			start_next_state(			dpl::Logger&		logger,
													ULock&				lock)
		{
			log_and_throw_on_exception(logger, [&]()
			{
				if(State* next = Variation::find_base_variant(nextID))
				{
					switchable = false;
					lock.unlock();
					next->begin(*progress);
					lock.lock();
					switchable = true;
					currID = nextID;
				}

				nextID = State::INVALID_INDEX;
			});
		}

		template<is_State T>
		void			add_states_internal()
		{
			Variation::create_variant<T>();
		}

		template<is_State T0, is_State T1, is_State... Tn>
		void			add_states_internal()
		{
			StateManager::add_states_internal<T0>();
			StateManager::add_states_internal<T1, Tn...>();
		}

	private:	// [EXCEPTIONS]
		void			log_and_throw_on_exception(	dpl::Logger&		logger,
													const Action&		FUNCTION)
		{
			try
			{
				return FUNCTION();
			}
			catch(const dpl::GeneralException& ERROR)
			{
				logger.push_error(ERROR.what());
			}
			catch(...)
			{
				logger.push_error("Unknown exception");
			}

			switchable = true;
			throw std::runtime_error("State failure");
		}

		void			throw_if_not_switchable() const
		{
			if(!switchable)
				throw dpl::GeneralException(this, __LINE__, "State cannot be changed at this moment.");
		}
	};


	inline void	State::set_previous_state()
	{
		Variant::get_variation().set_previous_state();
	}
}

#pragma warning( pop ) 