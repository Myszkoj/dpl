#pragma once


#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <dpl_Singleton.h>
#include <dpl_ReadOnly.h>
#include <dpl_Timer.h>
#include <dpl_Binary.h>
#include <dpl_TypeTraits.h>
#include <dpl_NamedType.h>
#include <dpl_Logger.h>
#include <dpl_Labelable.h>
#include <dpl_ThreadPool.h>


// declarations
namespace complex
{
	class	SystemInterface;

	class	ISubSystem;

	template<typename SubT>
	class	PhaseSubSystem;

	template<typename SubT>
	class	ParallelSubSystem;

	template<typename SubT>
	struct	IsSubSystem
	{
		static const bool value = std::is_base_of_v<ISubSystem, SubT>;
	};

	template<typename SUBSYSTEM_TYPES>
	concept	is_SubSystemTypeList	= dpl::is_TypeList<SUBSYSTEM_TYPES> 
									&& SUBSYSTEM_TYPES::ALL_UNIQUE 
									&& SUBSYSTEM_TYPES::template all<IsSubSystem>();

	class	ISupraSystem;

	template<typename SupraT, is_SubSystemTypeList SUBSYSTEMS>
	class	SupraSystem;

	template<typename SysT, is_SubSystemTypeList SUBSYSTEMS>
	class	PhaseSystem;

	template<typename SysT, is_SubSystemTypeList SUBSYSTEMS>
	class	ParallelSystem;

	class	SystemManager;
}

// interfaces
namespace complex
{
	class	SystemInterface : private dpl::Labelable<char>
	{
	public: // friends
		friend	SystemManager;
		
		template<typename, is_SubSystemTypeList>
		friend class SupraSystem;

	private: // subtypes
		using	MyFunction =  std::function<void()>;

	public: // data
		dpl::ReadOnly<uint64_t,		SystemInterface>	updateCycle;
		dpl::ReadOnly<dpl::Timer,	SystemInterface>	updateTimer;

	public: // inherited functions
		using Labelable::get_label;

	protected: // lifecycle
		CLASS_CTOR			SystemInterface(			dpl::Labeler<char>&		systemLabeler,
														const std::string&		NAME)
			: updateCycle(0)
		{
			systemLabeler.label(*this, NAME);
		}

	public: // functions
		inline double		get_avrerage_update_time() const
		{
			return (updateCycle() > 0) ? (updateTimer().duration<dpl::Timer::Milliseconds>().count() / static_cast<double>(updateCycle())) : 0.0;
		}

	public: // interface
		virtual bool		is_subsystem() const = 0;

	private: // interface
		virtual void		on_import_settings(			std::istream&			binary){}

		virtual void		on_export_settings(			std::ostream&			binary){}

		virtual void		on_install(){}

		virtual void		on_update(){}

		virtual void		on_uninstall(){}

	private: // functions used by the system manager
		void				install()
		{
			reset_diagnostic();
			log_and_throw_on_exception([&](){on_install();});
			this->is_subsystem()? dpl::Logger::ref().push_info("Successfully installed subsystem:  " + get_label())
								: dpl::Logger::ref().push_info("Successfully installed system:  " + get_label());
		}

		void				update()
		{
			++(*updateCycle);
			updateTimer().is_started()? updateTimer->unpause() : updateTimer->start();
			log_and_throw_on_exception([&](){on_update();});
			updateTimer->pause();
		}

		void				uninstall()
		{
			log_and_throw_on_exception([&](){on_uninstall();});
			this->is_subsystem()	? log_diagnostic("SUBSYSTEM")
									: log_diagnostic("SYSTEM");
			reset_diagnostic();
		}

	private: // internal functions
		inline void			reset_diagnostic()
		{
			updateCycle = 0;
			updateTimer->stop();
		}

		void				log_diagnostic(				const std::string&		SYSTEM_TYPE)
		{
			dpl::Logger::ref().push_info("-----[" + SYSTEM_TYPE + " DIAGNOSTIC]-----");
			dpl::Logger::ref().push_info("name:               " + get_label());
			dpl::Logger::ref().push_info("cycles:             " + std::to_string(updateCycle()));
			dpl::Logger::ref().push_info("avr update time:    " + std::to_string(get_avrerage_update_time()) + "[ms]");
			dpl::Logger::ref().push_info("total update time:  " + std::to_string(updateTimer().duration<dpl::Timer::Seconds>().count()) + "[s]");
		}

		void				log_and_throw_on_exception(	const MyFunction&		FUNCTION)
		{
			try
			{
				return FUNCTION();
			}
			catch(const dpl::GeneralException& ERROR)
			{
				dpl::Logger::ref().push_error("[" + get_label() + "]: " + ERROR.what());
			}
			catch(...)
			{
				dpl::Logger::ref().push_error("[" + get_label() + "]: Unknown exception");
			}

			throw std::runtime_error("System failure");
		}
	};


	class	ISubSystem : public SystemInterface
	{
	public: // friends
		template<typename>
		friend class PhaseSubSystem;

		template<typename>
		friend class ParallelSubSystem;

	private: // lifecycle
		CLASS_CTOR			ISubSystem(	dpl::Labeler<char>&	systemLabeler,
										const std::string&	NAME)
			: SystemInterface(systemLabeler, NAME)
		{

		}

	public: // lifecycle
		CLASS_DTOR virtual	~ISubSystem() = default;
	};


	class	ISupraSystem : public SystemInterface
	{
	public: // friends
		template<typename, is_SubSystemTypeList>
		friend class SupraSystem;

		friend	SystemManager;

	private: // subtypes
		using	MySubsystemPtr	= std::unique_ptr<ISubSystem>;
		using	MySubsystems	= std::vector<MySubsystemPtr>;

	public: // subtypes
		using	SystemCallback	= std::function<void(SystemInterface&)>;

	private: // lifecycle
		CLASS_CTOR			ISupraSystem(		dpl::Labeler<char>&		systemLabeler,
												const std::string&		NAME)
			: SystemInterface(systemLabeler, NAME)
		{
			
		}

	public: // lifecycle
		CLASS_DTOR virtual	~ISupraSystem() = default;

	public: // implementation
		virtual bool		is_subsystem() const final override
		{
			return false;
		}

	public: // interface
		virtual uint64_t	get_numSubsystems() const = 0;

		virtual void		for_each_subsystem(	const SystemCallback&	CALLBACK) = 0;

	private: // interface
		virtual void		on_start_install(){}

		virtual void		on_subsystems_installed(){}

		virtual void		on_start_update(){}

		virtual void		on_subsystems_updated(){}

		virtual void		on_start_uninstall(){}

		virtual void		on_subsystems_uninstalled(){}
	};
}

// system manager / supra system
namespace complex
{
	class	SystemManager : public dpl::Singleton<SystemManager>
	{
	private: // subtypes
		using	MySupraSystemPtr	= std::unique_ptr<ISupraSystem>;
		using	MySupraSystems		= std::vector<MySupraSystemPtr>;
		using	MySystemCallback	= std::function<void(SystemInterface&)>;

	public: // friends
		template<typename, is_SubSystemTypeList>
		friend class SupraSystem;

		template<typename>
		friend class PhaseSubSystem;

		template<typename SubT>
		friend class ParallelSubSystem;

		template<typename, is_SubSystemTypeList>
		friend class PhaseSystem;

	private: // data
		std::string				m_settingsFile;
		MySupraSystems			m_phaseSystems;
		MySupraSystems			m_parallelSystems;
		dpl::Labeler<char>		m_labeler;
		dpl::Logger				m_logger;
		dpl::ParallelPhase		m_phase; // All tasks must be done between system updates(call ThreadPool::wait when phase is done).
		std::atomic_uint32_t	m_parallelWorkers;
		std::atomic_bool		bParallelUpdate;
		std::atomic_bool		bParallelFailure;

	public: // lifecycle
		CLASS_CTOR				SystemManager(				const std::string&				SETTINGS_FILE,
															const uint32_t					NUM_THREADS = std::thread::hardware_concurrency())
			: m_settingsFile(SETTINGS_FILE)
			, m_phase(NUM_THREADS)
			, m_parallelWorkers(0)
			, bParallelUpdate(false)
		{

		}

		CLASS_DTOR				~SystemManager()
		{
			stop_parallel_update();
		}

	public: // functions
		inline uint64_t			count_phase_systems() const
		{
			return count_systems(m_phaseSystems);
		}

		inline uint64_t			count_parallel_systems() const
		{
			return count_systems(m_parallelSystems);
		}

		inline uint64_t			count_all_systems() const
		{
			return count_phase_systems() + count_parallel_systems();
		}

		inline dpl::Logger&		get_logger()
		{
			return m_logger;
		}

	protected: // functions
		template<	dpl::is_TypeList PhaseSystems, 
					dpl::is_TypeList ParallelSystems>
		void					install_all_systems()
		{
			SystemManager::throw_if_systems_already_installed();
			bParallelFailure = false;
			m_logger.clear();
			m_logger.push_info("Installing...");
			SystemManager::install_systems(PhaseSystems(), m_phaseSystems);
			SystemManager::install_systems(ParallelSystems(), m_parallelSystems);
			SystemManager::import_settings();
		}

		void					update_all_systems()
		{
			parallel_update();
			for_each_system(m_phaseSystems, [&](SystemInterface& system)
			{
				system.update();
				throw_if_phase_not_done();
			});
		}

		void					uninstall_all_systems()
		{
			m_logger.push_info("Uninstalling systems... ");
			stop_parallel_update();
			SystemManager::export_settings();
			for_each_system(m_phaseSystems,		[&](SystemInterface& system)
			{
				system.uninstall();
			});
			for_each_system(m_parallelSystems,	[&](SystemInterface& system)
			{
				system.uninstall();
			});
		}

	private: // functions
		inline void				for_each_system(			MySupraSystems&					systems,
															const MySystemCallback&			CALLBACK)
		{
			for(auto it = systems.begin(); it != systems.end(); ++it) CALLBACK(*it->get());
		}

		inline uint64_t			count_systems(				const MySupraSystems&			SUPRA_SYSTEMS) const
		{
			uint64_t count = 0;
			for(const auto& IT : SUPRA_SYSTEMS)
			{
				count += 1 + IT->get_numSubsystems();
			}
			return count;
		}

		inline void				throw_if_systems_already_installed() const
		{
			if(!m_phaseSystems.empty() || !m_parallelSystems.empty())
				throw dpl::GeneralException(this, __LINE__, "Systems already installed.");
		}

		inline void				throw_if_phase_not_done() const
		{
#ifdef _DEBUG
			if(m_phase.numTasks() > 0)
				throw dpl::GeneralException(this, __LINE__, "Parallel phase not done.");
#endif // _DEBUG
		}

		template<typename SupraT>
		inline void				install_system(				MySupraSystems&					systems)
		{
			SystemInterface*	newSystem = systems.emplace_back(std::make_unique<SupraT>()).get();
								newSystem->install(m_logger);
		}

		template <typename... SupraTs>
		inline void				install_systems(			const dpl::TypeList<SupraTs...> DUMMY,
															MySupraSystems&					systems) 
		{
			(SystemManager::install_system<SupraTs>(systems), ...);
		}

		inline SystemInterface*	find_system(				const std::string&				SYSTEM_NAME)
		{
			return static_cast<SystemInterface*>(m_labeler.find_entry(SYSTEM_NAME));
		}

		bool					import_settings()
		{
			std::ifstream file(m_settingsFile, std::ios::binary);
			if(file.fail() || file.bad())
				return m_logger.push_error("Settings file missing or unavailable.");

			std::string systemName;

			const uint64_t NUM_SETTINGS = dpl::import_t<uint64_t>(file);
			for(uint64_t index = 0; index < NUM_SETTINGS; ++index)
			{
				dpl::import_dynamic_container(file, systemName);
				const auto SETTINGS_END = dpl::import_t<int64_t>(file);

				if(auto* system = find_system(systemName))
				{
					system->on_import_settings(file);
					const auto CURRENT_POSITION = file.tellg();
					if(CURRENT_POSITION < SETTINGS_END)
					{
						file.seekg(SETTINGS_END);
					}
					else if(CURRENT_POSITION > SETTINGS_END)
					{
						throw dpl::GeneralException(this, __LINE__, "System imported too many bytes: " + system->get_label());
					}
				}
				else // move after settings
				{
					file.seekg(SETTINGS_END);
				}
			}
			return true;
		}

		void					export_settings(			SystemInterface&				system,
															std::ostream&					binary)
		{
			dpl::export_container(binary, system.get_label());	// System name
			dpl::export_t<int64_t>(binary, 0);					// End of the settings

			// Export settings and calculate their size.
			int64_t settingsStart	= binary.tellp();
			system.on_export_settings(binary);
			int64_t settingsEnd		= binary.tellp();

			// Override settingsEnd and go back to the current position in the stream.
			binary.seekp(settingsStart - sizeof(int64_t));
			dpl::export_t(binary, settingsEnd);
			binary.seekp(settingsEnd);
		}

		void					export_settings(			MySupraSystems&					systems,
															std::ostream&					binary)
		{
			for(auto& it : systems)
			{
				export_settings(*it, binary);

				it->for_each_subsystem([&](SystemInterface& subsystem)
				{
					export_settings(subsystem, binary);
				});
			}
		}

		bool					export_settings()
		{
			std::ofstream file(m_settingsFile, std::ios::binary | std::ios::trunc);
			if(file.fail() || file.bad())
				return m_logger.push_error("Settings could not be exported.");

			dpl::export_t(file, count_all_systems());
			export_settings(m_phaseSystems, file);
			export_settings(m_parallelSystems, file);
			return true;
		}

	private: // functions
		inline void				launch_parallel_system(		SystemInterface&				system)
		{
			std::thread([&]
			{
				++m_parallelWorkers;
				while(bParallelUpdate)
				{
					try
					{
						system.update();
					}
					catch(const dpl::GeneralException& ERROR)
					{
						m_logger.push_error("[" + system.get_label() + "]: " + ERROR.what());
						bParallelUpdate		= false;
						bParallelFailure	= true;
					}
					catch(...)
					{
						m_logger.push_error("[" + system.get_label() + "]: Unknown exception");
						bParallelUpdate		= false;
						bParallelFailure	= true;
					}
				}
				--m_parallelWorkers;

			}).detach();
		}

		inline void				parallel_update()
		{
			if(!bParallelUpdate)
			{
				if(bParallelFailure)
					throw dpl::GeneralException(this, __LINE__, "Exception in parallel system.");
				
				bParallelUpdate = true;

				for(auto& it : m_parallelSystems)
				{
					launch_parallel_system(*it);
				}

				std::this_thread::sleep_for(std::chrono::microseconds(10));
			}
		}

		inline void				stop_parallel_update()
		{
			bParallelUpdate = false;
			while(m_parallelWorkers > 0)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
	};


	template<typename SupraT, typename... SubTs>
	class	SupraSystem<SupraT, dpl::TypeList<SubTs...>>	: public ISupraSystem
															, public dpl::NamedType<SupraT>
															, public dpl::Singleton<SupraT>
	{
	private: // subtypes
		using	MyNamedBase		= dpl::NamedType<SupraT>;
		using	MySubsystemPtr	= std::unique_ptr<ISubSystem>;
		using	MySubsystems	= std::vector<MySubsystemPtr>;

	public: // subtypes
		using	SystemCallback	= ISupraSystem::SystemCallback;

	public: // friends
		template<typename, is_SubSystemTypeList>
		friend class	PhaseSystem;

		template<typename, is_SubSystemTypeList>
		friend class	ParallelSystem;

	private: // data
		MySubsystems m_subsystems;

	private: // lifecycle
		CLASS_CTOR			SupraSystem()
			: ISupraSystem(SystemManager::ref().m_labeler, MyNamedBase::typeName())
		{
			create_subsystems();
		}

	public: // implementation
		virtual uint64_t	get_numSubsystems() const final override
		{
			return m_subsystems.size();
		}

		virtual void		for_each_subsystem(			const SystemCallback&	CALLBACK) final override
		{
			for(auto& it : m_subsystems) CALLBACK(*it);
		}

	private: // implementation
		virtual void		on_install() final override
		{
			on_start_install();
			install_subsystems();
			on_subsystems_installed();
		}

		virtual void		on_update() final override
		{
			on_start_update();
			for(auto& it : m_subsystems)
			{
				SystemInterface*	subsystem = it.get();
									subsystem->update();
			}
			on_subsystems_updated();
		}

		virtual void		on_uninstall() final override
		{
			on_start_uninstall();
			for(auto& it : m_subsystems)
			{
				SystemInterface*	subsystem = it.get();
									subsystem->uninstall();
			}
			on_subsystems_uninstalled();
		}

	private: // functions
		inline void			create_subsystems()
		{
			m_subsystems.clear();
			(m_subsystems.emplace_back(std::make_unique<SubTs>()), ...);
		}

		inline void			install_subsystems()
		{
			for(auto& it : m_subsystems)
			{
				SystemInterface*	subsystem = it.get();
									subsystem->install();
			}
		}
	};
}

// phase systems
namespace complex
{
	template<typename SubT>
	class	PhaseSubSystem	: public ISubSystem
							, public dpl::NamedType<SubT>
							, private dpl::Singleton<SubT>
	{
	private: // subtypes
		using	MyNamedBase	= dpl::NamedType<SubT>;

	protected: // lifecycle
		CLASS_CTOR					PhaseSubSystem()
			: ISubSystem(SystemManager::ref().m_labeler, MyNamedBase::typeName())
		{

		}

	protected: // functions
		inline dpl::ParallelPhase&	phase()
		{
			return SystemManager::ref().m_phase;
		}

	public: // implementation
		virtual bool				is_subsystem() const final override
		{
			return true;
		}
	};


	template<typename SysT, typename... SubTs> requires (std::is_base_of_v<PhaseSubSystem<SubTs>, SubTs> && ...)
	class	PhaseSystem<SysT, dpl::TypeList<SubTs...>> : public SupraSystem<SysT, dpl::TypeList<SubTs...>>
	{
	protected: // lifecycle
		CLASS_CTOR					PhaseSystem() = default;

	protected: // functions
		inline dpl::ParallelPhase&	phase()
		{
			return SystemManager::ref().m_phase;
		}
	};
}

// parallel systems
namespace complex
{
	template<typename SubT>
	class	ParallelSubSystem	: public ISubSystem
								, public dpl::NamedType<SubT>
								, private dpl::Singleton<SubT>
	{
	private: // subtypes
		using	MyNamedBase	= dpl::NamedType<SubT>;

	protected: // lifecycle
		CLASS_CTOR		ParallelSubSystem()
			: ISubSystem(SystemManager::ref().m_labeler, MyNamedBase::typeName())
		{

		}

	public: // implementation
		virtual bool	is_subsystem() const final override
		{
			return true;
		}
	};

	template<typename SysT, typename... SubTs> requires (std::is_base_of_v<ParallelSubSystem<SubTs>, SubTs> && ...)
	class	ParallelSystem<SysT, dpl::TypeList<SubTs...>> : public SupraSystem<SysT, dpl::TypeList<SubTs...>>
	{
	protected: // lifecycle
		CLASS_CTOR	ParallelSystem() = default;
	};
}

// tests
namespace complex
{
	class SystemA : public PhaseSubSystem<SystemA>
	{
	public: // implementation
		virtual void	on_update() final override
		{
			dpl::Logger::ref().push_info("A updated");
		}
	};

	class SystemB : public PhaseSubSystem<SystemB>
	{
	public: // implementation
		virtual void	on_update() final override
		{
			dpl::Logger::ref().push_info("B updated");
		}
	};

	class SystemC : public PhaseSubSystem<SystemC>
	{
	public: // implementation
		virtual void	on_update() final override
		{
			dpl::Logger::ref().push_info("C updated");
		}
	};

	class SystemABC : public PhaseSystem<SystemABC, dpl::TypeList<SystemA, SystemB, SystemC>>{};

	class SystemX : public ParallelSubSystem<SystemX>
	{
	public: // implementation
		virtual void	on_update()
		{
			dpl::Logger::ref().push_info("X updated");
		}
	};

	class SystemY : public ParallelSubSystem<SystemY>
	{
	public: // implementation
		virtual void	on_update()
		{
			dpl::Logger::ref().push_info("Y updated");
		}
	};

	class SystemZ : public ParallelSubSystem<SystemZ>
	{
	public: // implementation
		virtual void	on_update()
		{
			dpl::Logger::ref().push_info("Z updated");
		}
	};

	class SystemXYZ : public ParallelSystem<SystemXYZ, dpl::TypeList<SystemX, SystemY, SystemZ>>{};

	class MyEngine : public SystemManager
	{
	public:
		MyEngine()
			: SystemManager("settings.bset")
		{

		}

		inline void start()
		{
			install_all_systems<dpl::TypeList<SystemABC>, dpl::TypeList<SystemXYZ>>();
			update_all_systems();
			uninstall_all_systems();
		}
	};

	inline void test_systems()
	{
		MyEngine	engine;
					engine.start();

		dpl::Logger& logger = engine.get_logger();
		for(const auto& LINE : logger.lines())
		{
			std::cout << LINE.str << '\n';
		}

		int breakpoint = 0;
	}
}