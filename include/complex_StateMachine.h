#pragma once


#include <typeindex>
#include <shared_mutex>
#include <optional>
#include <dpl_TypeTraits.h>
#include <dpl_ReadOnly.h>
#include <dpl_Variation.h>
#include <dpl_Logger.h>

#pragma warning( push )
#pragma warning( disable : 26110) //<-- Solves "Caller failing to hold lock..." VS bug.


// declarations
namespace complex
{
	class Progress;
	class ProgramState;
	class StateMachine;

	template<typename StateT>
	concept is_State = std::is_base_of_v<ProgramState, StateT>;
}

// state implementation
namespace complex
{
	/*
		Specialize in order to store/restore state of the given entity.
	*/
	template<typename T>
	class	EntityState
	{
	public: // functions
		inline void store(	const T&	ENTITY){}

		inline void restore(T&			entity){}
	};


	/*
		All program states must be derived from this class.
	*/
	class	ProgramState : private dpl::Variant<StateMachine, ProgramState>
	{
	public: // subtypes
		template<is_State T>
		using	Invoke	= std::function<void(T&)>;

		using	Binding	= Variant::Binding;

	public: // friends
		friend	Variant;
		friend	dpl::Variation<StateMachine, ProgramState>;
		friend	VCTOR_Base;
		friend	StateMachine;

	protected: // lifecycle
		CLASS_CTOR			ProgramState(	const Binding&		BINDING)
			: Variant(BINDING)
		{
			
		}

	protected: // functions
		inline void			set_previous_state();

		template<is_State T>
		inline void			set_next_state(	const Invoke<T>&	INVOKE_STATE = nullptr)
		{
			Variant::get_variation().set_next_state<T>(INVOKE_STATE);
		}

	private: // interface
		virtual void		begin(			Progress&			progress) = 0;

		virtual void		update(			Progress&			progress) = 0;

		virtual void		end() = 0;
	};


	class	NullState : public ProgramState
	{
	public: //lifecycle
		CLASS_CTOR			NullState(	const Binding&	BINDING)
			: ProgramState(BINDING)
		{

		}

	private: // State implementation
		virtual void		begin(		Progress&		progress) final override{}
		virtual void		update(		Progress&		progress) final override{}
		virtual void		end() final override{}
	};
}

// Progress & StateMachine implementation
namespace complex
{
	class	Progress
	{
	public: // subtypes
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

	private: // data
		uint32_t					m_numSteps;
		uint32_t					m_step;
		std::string					m_info;
		mutable std::shared_mutex	m_mtx;

	public: // lifecycle
		CLASS_CTOR				Progress()
			: m_numSteps(0)
			, m_step(0)
		{

		}

	public: // functions
		inline void				reset(					const uint32_t		NUM_STEPS		= 0,
														const std::string&	INITIAL_INFO	= "")
		{
			std::lock_guard lock(m_mtx);
			m_numSteps	= NUM_STEPS;
			m_step		= 0;
			m_info		= INITIAL_INFO;
		}

		inline void				set_step(				const uint32_t		STEP,
														const std::string&	INFO)
		{
			std::lock_guard lock(m_mtx);
			throw_if_invalid_step(STEP);
			m_step = STEP;
			m_info = INFO;
		}

		inline Status			get() const
		{
			std::lock_guard	lock(m_mtx);
			return Status(m_info, m_numSteps, m_step);
		}

	private: // exceptions
		inline void				throw_if_invalid_step(	const uint32_t		STEP) const
		{
			if(STEP < m_numSteps) return;
			throw dpl::GeneralException(this,__LINE__, "Invalid step: " + std::to_string(STEP) + ", max supported: " + std::to_string(m_numSteps));
		}
	};


	class	StateMachine : private dpl::Variation<StateMachine, ProgramState>
	{
	public: // friends
		friend ProgramState;
		friend Variation;

	public: // subtypes
		using	Action	= std::function<void()>;
		
		template<is_State T>
		using	Invoke	= std::function<void(T&)>;

		using	ULock	= std::unique_lock<std::shared_mutex>;

	public: // data
		dpl::ReadOnly<Progress,						StateMachine>	progress;

	private: // data
		dpl::ReadOnly<uint32_t,						StateMachine>	prevID;
		dpl::ReadOnly<uint32_t,						StateMachine>	currID;
		dpl::ReadOnly<uint32_t,						StateMachine>	nextID;
		dpl::ReadOnly<bool,							StateMachine>	switchable;
		mutable dpl::ReadOnly<std::shared_mutex,	StateMachine>	mtx;

	public: // lifecycle
		CLASS_CTOR		StateMachine()
			: prevID(ProgramState::INVALID_INDEX)
			, currID(ProgramState::INVALID_INDEX)
			, nextID(ProgramState::INVALID_INDEX)
			, switchable(true)
		{
			add_state<NullState>();
			nextID = Variation::get_typeID<NullState>();
		}

	public: // functions
		template<is_State T>
		inline bool		add_state()
		{
			std::lock_guard lock(*mtx);
			return Variation::create_variant<T>();
		}

		template<is_State... Tn>
		inline void		add_states()
		{
			std::lock_guard lock(*mtx);
			StateMachine::add_states_internal<Tn...>();
		}

		template<is_State T>
		inline T&		get_state()
		{
			std::lock_guard lock(*mtx);
			return Variation::get_variant<T>();
		}

		template<is_State T>
		inline const T&	get_state() const
		{
			std::lock_guard lock(*mtx);
			return Variation::get_variant<T>();
		}

		inline void		set_previous_state()
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
			if(nextID() == currID())	nextID = ProgramState::INVALID_INDEX;
			else if(INVOKE_STATE)		INVOKE_STATE(Variation::get_variant<T>());
		}

		template<is_State T>
		inline bool		is_previous_state() const
		{
			std::lock_guard lock(*mtx);
			return prevID() == Variation::get_typeID<T>();
		}

		template<is_State T>
		inline bool		is_current_state() const
		{
			std::lock_guard lock(*mtx);
			return currID() == Variation::get_typeID<T>();
		}

		template<is_State T>
		inline bool		is_next_state() const
		{
			std::lock_guard lock(*mtx);
			return nextID() == Variation::get_typeID<T>();
		}

	protected: // functions
		void			update_states(				dpl::Logger&		logger)
		{
			std::unique_lock lock(*mtx);
			if(nextID() != ProgramState::INVALID_INDEX)
			{
				stop_current_state(logger, lock);
				start_next_state(logger, lock);
			}

			log_and_throw_on_exception(logger, [&]()
			{
				if(ProgramState* current = Variation::find_base_variant(currID))
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

	private: // functions
		void			stop_current_state(			dpl::Logger&		logger,
													ULock&				lock)
		{
			log_and_throw_on_exception(logger, [&]()
			{
				prevID = currID;
				currID = ProgramState::INVALID_INDEX;
				progress->reset();

				if(ProgramState* previous = Variation::find_base_variant(prevID))
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
				if(ProgramState* next = Variation::find_base_variant(nextID))
				{
					switchable = false;
					lock.unlock();
					next->begin(*progress);
					lock.lock();
					switchable = true;
					currID = nextID;
				}

				nextID = ProgramState::INVALID_INDEX;
			});
		}

		template<is_State T>
		inline void		add_states_internal()
		{
			Variation::create_variant<T>();
		}

		template<is_State T0, is_State T1, is_State... Tn>
		inline void		add_states_internal()
		{
			StateMachine::add_states_internal<T0>();
			StateMachine::add_states_internal<T1, Tn...>();
		}

	private: // debug
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

		inline void		throw_if_not_switchable() const
		{
			if(!switchable)
				throw dpl::GeneralException(this, __LINE__, "State cannot be changed at this moment.");
		}
	};


	inline void	ProgramState::set_previous_state()
	{
		Variant::get_variation().set_previous_state();
	}
}

#pragma warning( pop ) 