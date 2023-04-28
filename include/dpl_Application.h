#pragma once


#include "dpl_EntityManager.h"
#include "dpl_SystemManager.h"
#include "dpl_TimeManager.h"
#include "dpl_StateManager.h"


// exit state
namespace dpl
{
	/*
		Program state that closes application.
	*/
	class Exit	: public dpl::State
				, public dpl::Singleton<Exit>
	{
	public: // subtypes
		enum	Response
		{
			ABORTED,
			CONFIRMED,
			WAITING
		};

		using	Condition = std::function<Response()>;

	private: // data
		dpl::ReadOnly<Condition,	Exit> condition;
		dpl::ReadOnly<Response,		Exit> response;

	public: // lifecycle
		CLASS_CTOR		Exit(			const Binding&	BINDING)
			: State(BINDING)
			, Singleton(SystemManager::ref().owner())
			, response(Response::WAITING)
		{

		}

	public: // functions
		void			set_condition(	const Condition CONDITION)
		{
			condition = CONDITION;
		}

	private: // implementation
		virtual void	begin(			Progress&		progress) final override
		{
			progress.reset(0, "Exiting...");
			response = Response::WAITING;
		}

		virtual void	update(			Progress&		progress) final override
		{
			if(condition())
			{
				response = condition()();
			
				switch(response())
				{
				case Response::ABORTED:		return set_previous_state();
				case Response::CONFIRMED:	return set_next_state<NullState>();
				default:					return; // Waiting for response...
				}
			}
			else // No condition required,just exit.
			{
				response = Response::CONFIRMED;
				set_next_state<NullState>();
			}
		}

		virtual void	end() final override
		{
			response = Response::CONFIRMED;
			set_next_state<NullState>();
		}
	};
}

// app implementation
namespace dpl
{
	class Application final	: public dpl::Singleton<Application>
							, public dpl::EntityManager
							, public dpl::SystemManager
							, public dpl::TimeManager
							, public dpl::StateManager
							, public dpl::BinaryInvoker
	{
	private:	// [SUBTYPES]
		using	SingletonBase = dpl::Singleton<Application>;

	public:		// [SUBTYPES]
		enum	Flags
		{
			WORKING,
			INSTALLED,	// All systems are installed.
			STARTED,	// Application was initialized and has now entered the main loop.
			SHUTDOWN	// User confirmed that he wants to exit the application.
		};

		using	Cycle = uint64_t;

	public:		// [CONSTANTS]
		static constexpr const char* SETTINGS_EXT = ".settings";

	public:		// [DATA]
		dpl::ReadOnly<int,					Application>	argc;
		dpl::ReadOnly<char**const,			Application>	argv;
		dpl::ReadOnly<dpl::Mask32<Flags>,	Application>	flags;
		dpl::ReadOnly<std::string,			Application>	name;

	public:		// [SINGLETON FIX]
		using	SingletonBase::ref;
		using	SingletonBase::ptr;

	public:		// [LIFECYCLE]
		CLASS_CTOR			Application(		dpl::Multition&			multition,
												const std::string&		APP_NAME,
												const int				ARG_COUNT, 
												char**const				ARG_VECTOR,
												const uint32_t			NUM_THREADS = std::thread::hardware_concurrency())
			: SingletonBase(multition)
			, EntityManager(multition)
			, SystemManager(multition, APP_NAME + SETTINGS_EXT, NUM_THREADS)
			, argc(ARG_COUNT)
			, argv(ARG_VECTOR)
		{
			add_state<Exit>();
		}

	public:		// [FUNCTIONS]
		/*
			Starts application.
			Returns after shutdown or on terminate.
		*/
		void				start(				const OnInstall&		ON_INSTALL)
		{
			if(flags().at(WORKING)) return;
			try
			{
				TimeManager::reset();	
				flags->set_at(WORKING, true);
				install_all_systems(ON_INSTALL);
				flags->set_at(STARTED, true);
			}
			catch(const dpl::GeneralException& e)
			{
				dpl::Logger::ref().push_error("Application >> %s", e.what());
				terminate();
				return;
			}
			catch(...)
			{
				dpl::Logger::ref().push_error("Application >> Unknown error");			
				terminate();
				return;
			}

			main_loop()? shutdown() : terminate();
		}

		/*
			Ignore current state and shutdown the application.
		*/
		void				request_shutdown()
		{
			flags->set_at(SHUTDOWN, true);
		}

	private:	// [FUNCTIONS]
		bool				set_next_cycle()
		{
			if(!flags().at(WORKING)) return false;
			if(flags().at(SHUTDOWN)) return false;
			TimeManager::update();
			return true;
		}

		bool				main_loop()
		{
			while(set_next_cycle())
			{
				try
				{
					update_states(dpl::Logger::ref());
					update_all_systems();
				}
				catch(const dpl::GeneralException& e)
				{
					dpl::Logger::ref().push_error("Fail to update: %s", e.what());
					return false;
				}
				catch(...)
				{
					dpl::Logger::ref().push_error("Fail to update: UNKNOWN_ERROR.");
					return false;
				}
			}

			return true;
		}

		void				shutdown()
		{
			if(flags().at(WORKING))
			{		
				try
				{
					BinaryInvoker::clear();
					release_states(dpl::Logger::ref());
					uninstall_all_systems();
				}
				catch(const dpl::GeneralException& e)
				{
					dpl::Logger::ref().push_error("Fail to shutdown application: %s", e.what());
					terminate();
					return;
				}
				catch(...)
				{
					dpl::Logger::ref().push_error("Fail to shutdown application: UNKNOWN_ERROR");
					terminate();
					return;
				}

				flags->set_at(WORKING, false);
				dpl::Logger::ref().export_lines("log.txt");
			}

			flags->clear();
		}

		void				terminate()
		{
			//show_error("Crash", "Unexpected program termination. See log.txt for more info."); //<-- This can be invoked through event.
			dpl::Logger::ref().export_lines("log.txt");
			flags->clear();
		}
	};
}