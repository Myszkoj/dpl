#pragma once


#include <memory>
#include <limits>
#include <typeindex>
#include <type_traits>
#include <string>
#include <shared_mutex>
#include "dpl_ReadOnly.h"
#include "dpl_Mask.h"


namespace dpl
{
//============ DECLARATIONS ============//

	template<typename StateT>
	class State;

	class StateMachine;

//============ IMPLEMENTATIONS ============//

	/*
		Interface for states.
		Returning false from functions; 'begin', 'update' and 'end' notifies state machine that 
		state is no longer valid and should be destroyed.
	*/
	class IState
	{
	public: // relations
		friend StateMachine;

		template<typename>
		friend class State;

	public: // subtypes
		enum	Flags
		{
			SELFMUTABLE // State can only be changed by internal call to 'set_next'.
		};

		class	Binding
		{
		public: // relation
			friend IState;
			friend StateMachine;

		private: // data
			StateMachine* m_stateMachine;

		private: // lifecycle
			CLASS_CTOR Binding(StateMachine& stateMachine)
				: m_stateMachine(&stateMachine)
			{

			}
		};

		struct	Progress
		{
			uint32_t numSteps	= 0;
			uint32_t step		= 0;

			inline float get_fraction() const
			{
				return (numSteps > 0) ? (float)(step) / (float)(numSteps) : 0.f;
			}
		};

	public: // constants
		static const uint32_t INVALID_TYPE_ID = std::numeric_limits<uint32_t>::max();

	public: // data
		static dpl::ReadOnly<uint32_t,	IState> numStates;
		ReadOnly<Mask32<Flags>,			IState> flags;
		ReadOnly<Progress,				IState> progress;
		ReadOnly<std::string,			IState> info;

	private: // data
		StateMachine*				m_stateMachine;
		mutable std::shared_mutex	m_mtx;

	public: // lifecycle
		CLASS_CTOR			IState(				const Binding&		BINDING,
												const Mask32<Flags>	FLAGS,
												const uint32_t		NUM_PROGRESS_STEPS)
			: flags(FLAGS)
			, m_stateMachine(BINDING.m_stateMachine)
		{
			progress->numSteps = NUM_PROGRESS_STEPS;
		}

		CLASS_DTOR virtual	~IState() = default;

	public: // interface
		virtual bool		same_typeID(		const uint32_t		TYPE_ID) const = 0;

		template<typename... Ts>
		bool				is_next_state() const;

	protected: // functions
		template<typename T, typename... CTOR>
		bool				set_next_state(		CTOR&&...			args);

		inline bool			set_progress_step(	const uint32_t		PROGRESS_STEP)
		{
			std::lock_guard lock(m_mtx);
			if(PROGRESS_STEP < progress().numSteps) 
			{
				progress->step = PROGRESS_STEP;
				return true;
			}
			return false;
		}

		inline void			set_info(			const char*			NEW_INFO)
		{
			std::lock_guard lock(m_mtx);
			*info = NEW_INFO;
		}

		inline void			set_info(			const std::string&	NEW_INFO)
		{
			std::lock_guard lock(m_mtx);
			*info = NEW_INFO;
		}

	private: // functions
		static uint32_t		assign_ID()
		{
			return (*numStates)++;
		}

		inline void			update_address(		StateMachine*		newAddress)
		{
			m_stateMachine = newAddress;
		}

	private: // interface
		virtual bool		on_change_requested(const uint32_t		NEXT_TYPE_ID) const
		{
			return !flags().at(IState::SELFMUTABLE);
		}

		virtual bool		on_begin() = 0;

		virtual bool		on_update() = 0;

		virtual bool		on_end() = 0;
	};


	template<typename StateT>
	class State : public IState
	{
	public: // relations
		friend StateMachine;

	public: // data
		static ReadOnly<uint32_t, State> typeID;

	public: // lifecycle
		CLASS_CTOR		State(		const Binding&		BINDING,
									const Mask32<Flags> FLAGS				= 0x00000000,
									const uint32_t		NUM_PROGRESS_STEPS	= 0)
			: IState(BINDING, FLAGS, NUM_PROGRESS_STEPS)
		{

		}

	public: // interface
		virtual bool	same_typeID(const uint32_t		TYPE_ID) const final override
		{
			return typeID() == TYPE_ID;
		}
	};


	template<typename StateT>
	ReadOnly<uint32_t, State<StateT>> State<StateT>::typeID = IState::assign_ID();


	class NullState : public State<NullState>
	{
	public: //lifecycle
		CLASS_CTOR			NullState(		const Binding&		BINDING)
			: State(BINDING)
		{

		}

	private: // State implementation
		virtual bool		on_begin() final override
		{
			return true;
		}

		virtual bool		on_update() final override
		{
			return true;
		}

		virtual bool		on_end() final override
		{
			return true;
		}
	};


	class StateMachine
	{
	public: // relations
		friend IState;

	public: // subtypes
		enum UpdateResult
		{
			SUCCESS,
			FAIL_TO_BEGIN,
			FAIL_TO_UPDATE,
			FAIL_TO_END
		};

	private: // data
		std::unique_ptr<IState>	m_currentState;
		std::unique_ptr<IState>	m_nextState;

	public: // lifecycle
		CLASS_CTOR						StateMachine()
		{
			m_currentState = std::make_unique<NullState>(IState::Binding(*this));
		}

		CLASS_CTOR						StateMachine(		const StateMachine&		OTHER) = delete;

		CLASS_CTOR						StateMachine(		StateMachine&&			other) noexcept
			: m_currentState(std::move(other.m_currentState))
			, m_nextState(std::move(other.m_nextState))
		{
			on_moved();
		}

		StateMachine&					operator=(			const StateMachine&		OTHER) = delete;

		StateMachine&					operator=(			StateMachine&&			other) noexcept
		{
			m_currentState	= std::move(other.m_currentState);
			m_nextState		= std::move(other.m_nextState);
			on_moved();
			return *this;
		}

	public: // functions
		template<typename T>
		static void						validate_state()
		{
			static_assert(std::is_base_of<State<T>, T>::value, "Invalid state type.");
		}

		template<typename T>
		inline bool						is_current_state() const
		{
			validate_state<T>();
			return m_currentState ? m_currentState->same_typeID(T::typeID()) : false;
		}

		template<typename T1, typename or_T2, typename... or_Tn>
		inline bool						is_current_state() const
		{
			if(is_current_state<T1>())				return true;
			if(is_current_state<or_T2, or_Tn...>())	return true;
			return false;
		}

		template<typename T>
		inline T*						get_current_state()
		{
			return is_current_state<T>() ? static_cast<T*>(m_currentState.get()) : nullptr;
		}

		template<typename T>
		inline bool						is_next_state() const
		{
			validate_state<T>();
			return m_nextState ? m_nextState->same_typeID(T::typeID()) : false;
		}

		template<typename T1, typename or_T2, typename... or_Tn>
		inline bool						is_next_state() const
		{
			if(is_next_state<T1>())					return true;
			if(is_next_state<or_T2, or_Tn...>())	return true;
			return true;
		}

		template<typename T>
		inline T*						get_next_state()
		{
			return is_next_state<T>() ? static_cast<T*>(m_nextState.get()) : nullptr;
		}

		template<typename T>
		inline bool						can_set() const
		{
			validate_state<T>();
			return m_currentState? m_currentState->on_change_requested(State<T>::typeID()) : true;
		}

		inline bool						can_set_any_state() const
		{
			return m_currentState? m_currentState->on_change_requested(IState::INVALID_TYPE_ID) : true;
		}

		/*
			Returns progress of the current state.
		*/
		inline const IState::Progress&	get_state_progress() const
		{
			static const IState::Progress NULL_PROGRESS;
			return m_currentState ? m_currentState->progress() : NULL_PROGRESS;
		}

		/*
			Returns info of the current state.
		*/
		inline const std::string&		get_state_info() const
		{
			static const std::string NULL_INFO;
			return m_currentState ? m_currentState->info() : NULL_INFO;
		}

	protected: // functions
		template<typename T, typename... CTOR>
		inline bool						request_next_state(	CTOR&&...				args)
		{
			return set_next_state<T>(false, std::forward<CTOR>(args)...);
		}

		void							update();

	private: // functions
		template<typename T, typename... CTOR>
		bool							set_next_state(		const bool				FORCED_CHANGE,
															CTOR&&...				args)
		{
			if(is_current_state<T>() || is_next_state<T>())
				return false;

			if(FORCED_CHANGE || this->can_set<T>())
			{
				m_nextState = std::make_unique<T>(IState::Binding(*this), std::forward<CTOR>(args)...);
				return true;
			}

			return false;
		}

		inline void						on_moved()
		{
			if(m_currentState) m_currentState->update_address(this);
			if(m_nextState) m_nextState->update_address(this);
		}

	private: // interface
		virtual void					on_state_updated(	const UpdateResult		RESULT){}
	};


	template<typename... Ts>
	bool	IState::is_next_state() const
	{
		std::lock_guard lock(m_mtx);
		return m_stateMachine ? m_stateMachine->is_next_state<Ts...>() : false;
	}

	template<typename T, typename... CTOR>
	bool	IState::set_next_state(		CTOR&&...			args)
	{
		std::lock_guard lock(m_mtx);
		return m_stateMachine? m_stateMachine->set_next_state<T>(true, std::forward<CTOR>(args)...) : false;
	}
}