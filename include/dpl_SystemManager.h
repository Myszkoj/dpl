#pragma once


#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <atomic>
#include "dpl_Singleton.h"
#include "dpl_ReadOnly.h"
#include "dpl_Timer.h"
#include "dpl_Binary.h"
#include "dpl_TypeTraits.h"
#include "dpl_NamedType.h"
#include "dpl_Logger.h"
#include "dpl_Membership.h"
#include "dpl_Variation.h"
#include "dpl_ThreadPool.h"


// declarations
namespace dpl
{
	class	SystemManager;

	class	System;

	template<typename SystemT>
	class	ParentSystem;

	template<typename SystemT, typename ParentSystemT>
	class	ChildSystem;
	
	template<typename SystemT>
	class	PhaseSystem;

	template<typename SystemT>
	class	ParallelSystem;


	static const uint32_t INSTALLATION_ORDER_HASH	= 999777111;
	static const uint32_t PHASE_ORDER_HASH			= 888777111;
	static const uint32_t PARALLEL_ORDER_HASH		= 777777111;
}

// concepts
namespace dpl
{
	template<typename T>
	concept is_PhaseSystem		= std::is_base_of_v<PhaseSystem<T>, T>;

	template<typename T>
	concept is_ParallelSystem	= std::is_base_of_v<ParallelSystem<T>, T>;

	template<typename T>
	concept is_ParentSystem		= dpl::is_specialization_v<T, ParentSystem>;

	template<typename T>
	concept is_ChildSystem		= dpl::is_specialization_v<T, ChildSystem>;
}

// phase access
namespace dpl
{
	template<typename SystemT, bool IS_CHILD_SYSTEM>
	struct	BaseSystemQuery;

	template<typename SystemT>
	struct	BaseSystemQuery<SystemT, true>
	{
		using PARENT_SYSTEM = typename SystemT::PARENT_SYSTEM;
	};

	template<typename SystemT>
	struct	BaseSystemQuery<SystemT, false>
	{
		using PARENT_SYSTEM = void;
	};


	class	PhaseUser
	{
	public:		// [FRIENDS]
		template<typename, typename>
		friend class ChildSystem;

		template<typename>
		friend class PhaseSystem;

	private:	// [LIFECYCLE]
		CLASS_CTOR PhaseUser() = default;

	protected:	// [FUNCTIONS]
		dpl::ParallelPhase&	phase()
		{
			return SystemManager::ref().m_phase;
		}
	};


	template<typename SystemT>
	struct	PhaseQuery
	{
	private:	using Base	= typename BaseSystemQuery<SystemT, is_ChildSystem<SystemT>>::PARENT_SYSTEM;
	public:		using PHASE	=	std::conditional_t<	is_PhaseSystem<SystemT>, // ?
																			PhaseUser, 
								std::conditional_t<	is_ChildSystem<SystemT>, // ?
																			PhaseQuery<Base>::PHASE, 
																			std::monostate>>;
	};

	template<>
	struct	PhaseQuery<void>
	{
	using PHASE = std::monostate;
	};


	template<typename SystemT>
	using	MaybePhaseUser = typename PhaseQuery<SystemT>::PHASE;
}

// interface
namespace dpl
{
	class	Settings
	{
	public:		// [SUBTYPES]
		using	SignedIntegers		= dpl::TypeList<int8_t, int16_t, int32_t, int64_t>;
		using	UnsignedIntegers	= dpl::TypeList<uint8_t, uint16_t, uint32_t, uint64_t>;
		using	RealNumbers			= dpl::TypeList<float, double>;
		using	Misc				= dpl::TypeList<bool, std::string>;
		using	KnownTypes			= dpl::merge_t<SignedIntegers, UnsignedIntegers, RealNumbers, Misc>;

	private:	// [SUBTYPES]
		class	DataWrapper
		{
		public:		// [INTERFACE]
			virtual uint32_t	get_typeID() const = 0;
			virtual void		import_from_binary(		std::istream&	stream) = 0;
			virtual void		export_to_binary(		std::ostream&	stream) const = 0;
		};

		template<dpl::is_one_of<KnownTypes> T>
		class	DataWrapper_of : public DataWrapper
		{
		public:		// [DATA]
			T data;

		public:		// [IMPLEMENTATION]
			virtual uint32_t	get_typeID() const final override
			{
				return KnownTypes::index_of<T>();
			}

			virtual void		export_to_binary(		std::ostream&	stream) const final override
			{
				dpl::export_t(stream, data);
			}

			virtual void		import_from_binary(		std::istream&	stream) final override
			{
				dpl::import_t(stream, data);
			}
		};

		using	Ptr_t = std::unique_ptr<DataWrapper>;
		using	Map_t = std::unordered_map<std::string, Ptr_t>;

	public:		// [FRIENDS]
		friend	SystemManager;

	private:	// [DATA]
		Map_t m_map;

	public:		// [FUNCTIONS]
		template<dpl::is_one_of<KnownTypes> T>
		T*				get(				const std::string&	TAG) const
		{
			if(DataWrapper* wrapper = find_wrapper(TAG))
			{
				return &static_cast<DataWrapper_of<T>*>(wrapper)->data;
			}

			return nullptr;
		}

		template<dpl::is_one_of<KnownTypes> T>
		void			set(				const std::string&	TAG,
											const T&			DATA)
		{
			auto& ptr = m_map[TAG];
			if(!ptr) ptr = std::make_unique<DataWrapper_of<T>>();
			static_cast<DataWrapper_of<T>*>(ptr.get())->data = DATA;
		}
		
	private:	// [FUNCTIONS]
		DataWrapper*	find_wrapper(		const std::string&	HASH) const
		{
			const auto IT = m_map.find(HASH);
			return (IT != m_map.end())? IT->second.get() : nullptr;
		}

	private:	// [IO]
		void			clear()
		{
			m_map.clear();
		}

		bool			export_to_binary(	const std::string&	SETTINGS_FILE) const
		{
			std::ofstream file(SETTINGS_FILE, std::ios::binary | std::ios::trunc);
			if(file.fail() || file.bad())
				return dpl::Logger::ref().push_error("Settings could not be exported.");


			dpl::export_t(file, (uint64_t)m_map.size());
			for(const auto& IT : m_map)
			{
				dpl::export_container(file, IT.first);
				dpl::export_t(file, IT.second->get_typeID());
				IT.second->export_to_binary(file);
			}
			return true;
		}

		bool			import_from_binary(	const std::string&	SETTINGS_FILE)
		{
			std::ifstream file(SETTINGS_FILE, std::ios::binary);
			if(file.fail() || file.bad())
				return dpl::Logger::ref().push_error("Settings file missing or unavailable.");

			std::string tag;
			uint32_t	typeID = -1;

			auto import_data = [&]<typename T>(T* dummy)
			{
				if(typeID != KnownTypes::index_of<T>()) return true; // UNKNWON_TYPE

				auto result = m_map.emplace(tag, std::make_unique<DataWrapper_of<T>>());

				DataWrapper*	wrapper = result.first->second.get();
								wrapper->import_from_binary(file);

				return false;
			};

			KnownTypes::DataPack* type_iterator = nullptr;

			const uint64_t NUM_SETTINGS = dpl::import_t<uint64_t>(file);
			m_map.reserve(NUM_SETTINGS);

			bool success = true;
			for(uint64_t index = 0; index < NUM_SETTINGS; ++index)
			{
				dpl::import_dynamic_container(file, tag);
				dpl::import_t(file, typeID);
				std::invoke([&]<typename... Ts>(const std::tuple<Ts...>* DUMMY)
				{			
					const bool UNKNOWN_TYPE = (... && import_data((Ts*)(0)));
					if(UNKNOWN_TYPE)
					{
						success = dpl::Logger::ref().push_error("Unknwon setting: Type[%s] Tag[%s]", dpl::undecorate_type_name<Ts>().c_str(), tag.c_str());
					}
						
				}, type_iterator);
			}

			return success;
		}
	};


	class	System	: public dpl::Variant<SystemManager, System>
					, private dpl::Sequenceable<System, INSTALLATION_ORDER_HASH>
	{
	private:	// [SUBTYPES]
		using	MyInstallationOrder = dpl::Sequenceable<System, INSTALLATION_ORDER_HASH>;

	public:		// [SUBTYPES]
		using	Action	=  std::function<void()>;

	public:		// [FRIENDS]
		friend	SystemManager;
		friend	MyInstallationOrder;
		friend	MyInstallationOrder::MyBase;

		template<typename>
		friend class ParentSystem;

	public:		// [DATA]
		dpl::ReadOnly<std::string,	System> name;
		dpl::ReadOnly<uint64_t,		System>	updateCycle;
		dpl::ReadOnly<dpl::Timer,	System>	updateTimer;

	private:	// [LIFECYCLE]
		CLASS_CTOR			System(						const Binding&		BINDING,
														std::string			sysName)
			: Variant(BINDING)
			, name(sysName)
			, updateCycle(0)
		{
			
		}

		CLASS_CTOR			System(						const System&		OTHER)			= delete;
		CLASS_CTOR			System(						System&&			other) noexcept = default;
		System&				operator=(					const System&		OTHER)			= delete;
		System&				operator=(					System&&			other) noexcept = default;

	public:		// [LIFECYCLE]
		CLASS_DTOR virtual ~System() = default;

	public:		// [FUNCTIONS]
		double				get_avrerage_update_time() const
		{
			return (updateCycle() > 0) ? (updateTimer().duration<dpl::Timer::Milliseconds>().count() / static_cast<double>(updateCycle())) : 0.0;
		}

	protected:	// [DIAGNOSTIC]
		void				reset_diagnostic()
		{
			updateCycle = 0;
			updateTimer->stop();
		}

		void				log_diagnostic()
		{
			dpl::Logger::ref().push_info("-----[SYSTEM DIAGNOSTIC]-----");
			dpl::Logger::ref().push_info("name:               " + name());
			dpl::Logger::ref().push_info("cycles:             " + std::to_string(updateCycle()));
			dpl::Logger::ref().push_info("avr update time:    " + std::to_string(get_avrerage_update_time()) + "[ms]");
			dpl::Logger::ref().push_info("total update time:  " + std::to_string(updateTimer().duration<dpl::Timer::Seconds>().count()) + "[s]");
		}

		void				log_and_throw_on_exception(	const Action&		ACTION)
		{
			try
			{
				return ACTION();
			}
			catch(const dpl::GeneralException& ERROR)
			{
				dpl::Logger::ref().push_error("[" + name() + "]: " + ERROR.what());
			}
			catch(...)
			{
				dpl::Logger::ref().push_error("[" + name() + "]: Unknown exception");
			}

			throw std::runtime_error("System failure");
		}

	private:	// [INTERFACE]
		virtual void		on_save(					Settings&			settings) const{}

		virtual void		on_load(					const Settings&		SETTINGS){}

		virtual void		on_install(){}

		virtual void		on_update(){}

		virtual void		on_uninstall(){}

	private:	// [FUNCTIONS]
		void				install()
		{
			reset_diagnostic();
			log_and_throw_on_exception([&](){on_install();});
			dpl::Logger::ref().push_info("Successfully installed system:  " + name());
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
			log_diagnostic();
			reset_diagnostic();
		}
	};
}

// implementation
namespace dpl
{
	template<typename SystemT>
	class	ParentSystem	: public System
							, public dpl::Singleton<SystemT>
							, private dpl::Group<SystemT, System>
	{
	private:	// [SUBTYPES]
		using	MySingletonBase = dpl::Singleton<SystemT>;
		using	MySubsystems	= dpl::Group<SystemT, System>;

	public:		// [FRIENDS]
		friend	MySubsystems;
		friend	MySubsystems::MyBase;
		friend	System;
		friend	SystemManager;

		template<typename, typename>
		friend class ChildSystem;

		template<typename, typename>
		friend class PhaseSystem;

		template<typename, typename>
		friend class ParallelSystem;

	public:		// [SUBTYPES]
		using	Binding = System::Binding;

	private:	// [LIFECYCLE]
		CLASS_CTOR			ParentSystem(			const Binding&						BINDING)
			: System(BINDING, dpl::undecorate_type_name<SystemT>())
			, MySingletonBase(SystemManager::ref().owner())
		{

		}

		CLASS_CTOR			ParentSystem(			const ParentSystem&					OTHER)			= delete;
		CLASS_CTOR			ParentSystem(			ParentSystem&&						other) noexcept = default;
		ParentSystem&		operator=(				const ParentSystem&					OTHER)			= delete;
		ParentSystem&		operator=(				ParentSystem&&						other) noexcept = default;

	private:	// [FUNCTIONS]
		template<typename SubsystemT>
		void				add_system(				ChildSystem<SubsystemT, SystemT>&	subsystem)
		{
			MySubsystems::add_end_member(subsystem);
		}

	private:	// [INTERFACE]
		virtual void		on_subsystems_saved(	Settings&							settings) const{}

		virtual void		on_subsystems_loaded(	const Settings&						SETTINGS){}

		virtual void		on_start_update(){}

		virtual void		on_subsystems_updated(){}

	private:	// [IMPLEMENTATION]
		virtual void		on_save(				Settings&							settings) const final override
		{
			MySubsystems::for_each_member([&](const System& SUBSYSTEM)
			{
				SUBSYSTEM.on_save(settings);
			});
			on_subsystems_saved(settings);
		}

		virtual void		on_load(				const Settings&						SETTINGS) final override
		{
			MySubsystems::for_each_member([&](System& subsystem)
			{
				subsystem.on_load(SETTINGS);
			});
			on_subsystems_loaded(SETTINGS);
		}

		virtual void		on_update() final override
		{
			on_start_update();
			MySubsystems::for_each_member([&](System& subsystem)
			{
				subsystem.update();
			});
			on_subsystems_updated();
		}
	};


	template<typename SystemT, typename ParentSystemT>
	class	ChildSystem		: public ParentSystem<SystemT>
							, public MaybePhaseUser<ParentSystemT>
							, private dpl::Member<ParentSystemT, System>
	{
	private:	// [SUBTYPES]
		using	MyMembership	= dpl::Member<ParentSystemT, System>;

	public:	// [SUBTYPES]
		using	PARENT_SYSTEM	= ParentSystemT;

	public:		// [FRIENDS]
		friend  SystemManager;
		friend	MyMembership;
		friend	MyMembership::MyBase;
		friend	MyMembership::MyLink;
		friend	ParentSystem<PARENT_SYSTEM>;
	};


	template<typename SystemT>
	class	PhaseSystem		: public ParentSystem<SystemT>
							, public PhaseUser
							, private dpl::Sequenceable<System, PHASE_ORDER_HASH>
	{
	private:	// [SUBTYPES]
		using	MySystemBase	= ParentSystem<SystemT>;
		using	MySequence		= dpl::Sequenceable<System, PHASE_ORDER_HASH>;

	public:		// [FRIENDS]
		friend	MySequence;
		friend	MySequence::MyBase;

	protected:	// [LIFECYCLE]
		using	MySystemBase::MySystemBase;
		using	MySystemBase::operator=;
	};


	template<typename SystemT>
	class	ParallelSystem	: public ParentSystem<SystemT>
							, private dpl::Sequenceable<System, PARALLEL_ORDER_HASH>
	{
	private:	// [SUBTYPES]
		using	MySystemBase	= ParentSystem<SystemT>;
		using	MySequence		= dpl::Sequenceable<System, PARALLEL_ORDER_HASH>;

	public:		// [FRIENDS]
		friend	MySequence;
		friend	MySequence::MyBase;

	protected:	// [LIFECYCLE]
		using	MySystemBase::MySystemBase;
		using	MySystemBase::operator=;
	};
}

// manager
namespace dpl
{
	class	SystemInstaller
	{
	public:		// [FRIENDS]
		friend SystemManager;

	private:	// [LIFECYCLE]
		CLASS_CTOR	SystemInstaller()
		{

		}

	public:		// [FUNCTIONS]
		template<typename SystemT>
		void		install_system()
		{
			SystemManager::ref().install_system<SystemT>();
		}
	};



	class	SystemManager	: public dpl::Singleton<SystemManager>
							, public dpl::Variation<SystemManager, System>
							, private dpl::Sequence<System, INSTALLATION_ORDER_HASH>
							, private dpl::Sequence<System, PHASE_ORDER_HASH>
							, private dpl::Sequence<System, PARALLEL_ORDER_HASH>
	{
	private:	// [SUBTYPES]
		using	MySystemCallback	= std::function<void(System&)>;
		using	InstallationOrder	= dpl::Sequence<System, INSTALLATION_ORDER_HASH>;
		using	PhaseOrder			= dpl::Sequence<System, PHASE_ORDER_HASH>;
		using	ParallelOrder		= dpl::Sequence<System, PARALLEL_ORDER_HASH>;

	public:		// [SUBTYPES]
		using	OnInstall			= std::function<void(SystemInstaller&)>;

	public:		// [FRIENDS]
		friend	InstallationOrder;
		friend	PhaseUser;
		
	private:	// [DATA]
		std::string				m_settingsFile;
		dpl::Logger				m_logger;
		dpl::ParallelPhase		m_phase; // All tasks must be done between system updates(call ThreadPool::wait when phase is done).
		std::atomic_uint32_t	m_parallelWorkers;
		std::atomic_bool		bParallelUpdate;
		std::atomic_bool		bParallelFailure;

	protected:		// [LIFECYCLE]
		CLASS_CTOR				SystemManager(				Multition&				multition,
															const std::string&		SETTINGS_FILE,
															const uint32_t			NUM_THREADS = std::thread::hardware_concurrency())
			: Singleton(multition)
			, m_settingsFile(SETTINGS_FILE)
			, m_logger(multition)
			, m_phase(NUM_THREADS)
			, m_parallelWorkers(0)
			, bParallelUpdate(false)
		{

		}

		CLASS_DTOR				~SystemManager()
		{
			dpl::no_except([&](){ stop_parallel_update(); });
		}

	protected:	// [FUNCTIONS]
		void					install_all_systems(		const OnInstall&		ON_INSTALL)
		{
			SystemManager::throw_if_systems_already_installed();
			bParallelFailure = false;
			m_logger.clear();
			m_logger.push_info("Installing...");
			SystemInstaller installer;
			ON_INSTALL(installer);
			SystemManager::load_settings();
		}

		void					update_all_systems()
		{
			parallel_update();
			PhaseOrder::for_each([&](System& system)
			{
				system.update();
				throw_if_phase_not_done();
			});
		}

		void					uninstall_all_systems()
		{
			m_logger.push_info("Uninstalling systems... ");
			stop_parallel_update();
			SystemManager::save_settings();
			InstallationOrder::iterate_backwards([&](System& system)
			{
				system.uninstall();
			});
			Variation::destroy_all_variants();
		}

	private:	// [IO]
		bool					save_settings() const
		{
			Settings settings;
			PhaseOrder::for_each([&](const System& SYSTEM)
			{
				SYSTEM.on_save(settings);
			});
			ParallelOrder::for_each([&](const System& SYSTEM)
			{
				SYSTEM.on_save(settings);
			});
			return settings.export_to_binary(m_settingsFile);
		}

		bool					load_settings()
		{
			Settings settings;
			if(!settings.import_from_binary(m_settingsFile))	return false;
			PhaseOrder::for_each([&](System& system)
			{
				system.on_load(settings);
			});
			ParallelOrder::for_each([&](System& system)
			{
				system.on_load(settings);
			});
			return true;
		}

	private:	// [FUNCTIONS]
		template<typename SystemT>
		void					install_system()
		{
			auto result = Variation::create_variant<SystemT>();

			if(result)
			{
				if constexpr (is_PhaseSystem<SystemT>)
				{
					PhaseOrder::add_back(*result.get());
				}
				else if constexpr (is_ParallelSystem<SystemT>)
				{
					ParallelOrder::add_back(*result.get());
				}
				else // child system
				{
					using ParentT = typename SystemT::PARENT_SYSTEM;
					if(ParentSystem<ParentT>* parent = Variation::find_variant<ParentT>())
					{
						parent->add_system(*result.get());
					}
					else // handle error
					{
						// [TODO]: Log error
						throw dpl::GeneralException(this, __LINE__, "Could not find given parent system: ", dpl::undecorate_type_name<ParentT>().c_str())
						Variation::destroy_variant<SystemT>();
						return;
					}
				}

				System*	newSystem = result.get();
						newSystem->install();

				InstallationOrder::add_back(*newSystem);
			}
		}

		void					launch_parallel_system(		System&					system)
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
						m_logger.push_error("[" + system.name() + "]: " + ERROR.what());
						bParallelUpdate		= false;
						bParallelFailure	= true;
					}
					catch(...)
					{
						m_logger.push_error("[" + system.name() + "]: Unknown exception");
						bParallelUpdate		= false;
						bParallelFailure	= true;
					}
				}
				--m_parallelWorkers;

			}).detach();
		}

		void					parallel_update()
		{
			if(!bParallelUpdate)
			{
				if(bParallelFailure)
					throw dpl::GeneralException(this, __LINE__, "Exception in parallel system.");
				
				bParallelUpdate = true;

				ParallelOrder::for_each([&](System& system)
				{
					launch_parallel_system(system);
				});

				std::this_thread::sleep_for(std::chrono::microseconds(10));
			}
		}

		void					stop_parallel_update()
		{
			bParallelUpdate = false;
			while(m_parallelWorkers > 0)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}

	private:	// [EXCEPTIONS]
		void					throw_if_systems_already_installed() const
		{
			if(Variation::get_numVariants() > 0)
				throw dpl::GeneralException(this, __LINE__, "Systems already installed.");
		}

		void					throw_if_phase_not_done() const
		{
#ifdef _DEBUG
			if(m_phase.numTasks() > 0)
				throw dpl::GeneralException(this, __LINE__, "Parallel phase not done.");
#endif // _DEBUG
		}
	};
}