#pragma once


#include <memory>
#include <dpl_ThreadPool.h>
#include <dpl_Command.h>
#include "complex_Systems.h"
#include "complex_StateMachine.h"
#include "complex_TimeManager.h"

// OBSOLETE
#include <dpl_EventDispatcher.h>
#include "complex_MainWindow.h"


// exit state
namespace complex
{
	/*
		Program state that closes application.
	*/
	class Exit	: public ProgramState
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
		CLASS_CTOR		Exit(			const Binding&	BINDING);

	public: // functions
		inline void		set_condition(	const Condition CONDITION)
		{
			condition = CONDITION;
		}

	private: // implementation
		virtual void	begin(			Progress&		progress) final override;
		virtual void	update(			Progress&		progress) final override;
		virtual void	end() final override;
	};
}

// app
namespace complex
{
	/*
		[Problems]:
		- creation of objects through commands
		
		[TODO]:
		- CommandInvoker should be able to switch between pack-commands and single commands
		- Sort children with any predicate
	*/
	class	Application	: public dpl::Singleton<Application>
						, private SystemManager	
						, public StateMachine
						, public TimeManager
						, public dpl::CommandInvoker
	{
	private: // subtypes
		using	SingletonBase = dpl::Singleton<Application>;

	public: // subtypes
		enum	Flags
		{
			WORKING,
			INSTALLED,	// All systems and components are installed.
			STARTED,	// Application was initialized and has now entered the main loop.
			SHUTDOWN	// User confirmed that he wants to exit the application.
		};

		using	Cycle = uint64_t;

	public: // constants
		static constexpr const char* SETTINGS_EXT = ".settings";

	public: // data
		dpl::ReadOnly<int,							Application>	argc;
		dpl::ReadOnly<char**const,					Application>	argv;
		dpl::ReadOnly<dpl::Mask32<Flags>,			Application>	flags;
		dpl::ReadOnly<std::string,					Application>	name;
		dpl::ReadOnly<std::unique_ptr<MainWindow>,	Application>	mainWindow;
		//dpl::ResourceManager										resources;
		mutable dpl::EventDispatcher								dispatcher;

	public: // system functions
		using SingletonBase::ref;
		using SingletonBase::ptr;
		using SystemManager::count_phase_systems;
		using SystemManager::count_parallel_systems;
		using SystemManager::count_all_systems;
		using SystemManager::get_logger;

	public: // lifecycle
		CLASS_CTOR			Application(				const std::string&				APP_NAME,
														const int						ARG_COUNT, 
														char**const						ARG_VECTOR,
														const uint32_t					NUM_THREADS = std::thread::hardware_concurrency());

		CLASS_DTOR			~Application();

	public: // functions
		/*
			Starts application.
			Returns after shutdown or on terminate.
		*/
		template<	dpl::is_TypeList PhaseSystems, 
					dpl::is_TypeList ParallelSystems>
		inline void			start()
		{
			if(flags().at(WORKING)) return;
			handle_installation([&]()
			{
				this->install_all_systems<PhaseSystems, ParallelSystems>();
			});
			main_loop()? shutdown() : terminate();
		}

		/*
			Ignore current state and shutdown the application.
		*/
		inline void			request_shutdown()
		{
			flags->set_at(SHUTDOWN, true);
		}

	private: // TODO: Move to the GUI system.
		void				show_error(					const char*						TITLE,
														const char*						MESSAGE) const;

		void				show_warning(				const char*						TITLE,
														const char*						MESSAGE) const;

		void				show_information(			const char*						TITLE,
														const char*						MESSAGE) const;

		void				initialize_GUI();

		void				update_events();

	private: // functions
		void				handle_installation(		const std::function<void()>&	INSTALL_ALL_SYSTEMS);

		bool				set_next_cycle();

		bool				main_loop();

		void				shutdown();

		void				terminate();

		void				safe_release() noexcept;

	private: // OBSOLETE
		/*
		enum	Request
		{
			SAVE_PROJECT_AS,
			SAVE_PROJECT,
			OPEN_PROJECT,
			CLOSE_PROJECT,
			NEW_PROJECT,
			EXIT
		};

		enum	Status
		{
			CONFIRM,// User confirmed the request.
			DISCARD,// User wants to discard the request.
			WAIT	// User hasn't decided yet.
		};
		*/
		// dpl::ReadOnly<dpl::Mask32<Request>, Application>	requests;
		//Project											project; //<-- Obsolete
		//std::optional<std::string>						newProjectPath; // SAVE/OPEN file paths
		// 
		//void				request(					const Request					REQUEST);

		/*
		void				handle_SAVE_request();

		void				handle_SAVE_AS_request();

		void				handle_CLOSE_request();

		void				handle_EXIT_request();

		void				handle_OPEN_request();

		void				handle_NEW_request();

		void				handle_all_requests();
		*/

		/*
		virtual void		on_started(){}

		virtual void		on_shutdown(){}

		virtual void		on_new_project(){}

		virtual void		on_save_project(){}

		virtual void		on_load_project(){}

		virtual Status		confirm_SAVE()
		{
			return Status::CONFIRM;
		}

		virtual Status		confirm_SAVE_AS()
		{
			return Status::CONFIRM;
		}

		virtual Status		confirm_CLOSE()
		{
			return Status::CONFIRM;
		}

		virtual Status		confirm_EXIT()
		{
			return Status::CONFIRM;
		}

		virtual Status		confirm_OPEN(				const bool						bREOPEN)
		{
			return Status::CONFIRM;
		}
		*/
	};
}