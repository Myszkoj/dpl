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

#pragma warning( push )
#pragma warning( disable : 26495 )

// declarations
namespace dpl
{
	class	SystemManager;

	class	ISystem;

	template<typename SystemT>
	class	System;

	static const uint32_t INSTALLATION_ORDER_HASH	= 999777111;
	static const uint32_t SYSTEM_CATEGORY_HASH		= 888777111;

	// Specialize to set parent system.
	template<typename SystemT>
	struct	Dependency_of
	{
		using ParentSystem = void;
	};

	template<typename T>
	concept is_RootSystem	=  std::is_same_v<typename Dependency_of<T>::ParentSystem, T>
							|| std::is_same_v<typename Dependency_of<T>::ParentSystem, void>
							|| !std::is_base_of_v<ISystem, typename Dependency_of<T>::ParentSystem>;
}

// implementation
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

		bool			save_to_binary(		const std::string&	SETTINGS_FILE) const
		{
			std::ofstream file(SETTINGS_FILE, std::ios::binary | std::ios::trunc);
			if(file.fail() || file.bad())
				return dpl::Logger::ref().push_error("Settings could not be exported.");


			dpl::export_t(file, (uint64_t)m_map.size());
			for(const auto& IT : m_map)
			{
				dpl::export_t(file, IT.first);
				dpl::export_t(file, IT.second->get_typeID());
				IT.second->export_to_binary(file);
			}
			return true;
		}

		bool			load_from_binary(	const std::string&	SETTINGS_FILE)
		{
			std::ifstream file(SETTINGS_FILE, std::ios::binary);
			if(file.fail() || file.bad())
				return dpl::Logger::ref().push_error("Settings file missing or unavailable.");

			std::string tag;
			uint32_t	typeID = -1;

			// Returns true if importer encountered unknown type.
			auto import_data = [&]<typename T>(T* dummy)
			{
				if (typeID != KnownTypes::index_of<T>())
				{
					dpl::Logger::ref().push_error("Unknown setting: Type[%s] Tag[%s]", dpl::undecorate_type_name<T>().c_str(), tag.c_str());
					return true; // UNKNWON_TYPE
				}

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
				dpl::import_t(file, tag);
				dpl::import_t(file, typeID);
				std::invoke([&]<typename... Ts>(const std::tuple<Ts...>* DUMMY)
				{			
					const bool UNKNOWN_TYPE = (... && import_data((Ts*)(0)));
					if(UNKNOWN_TYPE)
					{
						success = false;
					}
						
				}, type_iterator);
			}

			return success;
		}
	};


	class	ISystem	: public dpl::Variant<SystemManager, ISystem>
					, private dpl::Group<ISystem, ISystem>
					, private dpl::Member<ISystem, ISystem>
					, private dpl::Sequenceable<ISystem, INSTALLATION_ORDER_HASH>
					, private dpl::Sequenceable<ISystem, SYSTEM_CATEGORY_HASH>
	{
	private:	// [SUBTYPES]
		using	MyGroup				= dpl::Group<ISystem, ISystem>;
		using	MyMembership		= dpl::Member<ISystem, ISystem>;
		using	MySubsystems		= MyGroup;
		using	MyInstallationOrder = dpl::Sequenceable<ISystem, INSTALLATION_ORDER_HASH>;
		using	MyCategory			= dpl::Sequenceable<ISystem, SYSTEM_CATEGORY_HASH>;

	public:		// [SUBTYPES]
		using	Action	=  std::function<void()>;

	public:		// [FRIENDS]
		friend	MyGroup;
		friend	MyGroup::MyBase;
		friend	MyMembership;
		friend	MyMembership::MyBase;
		friend	MyMembership::MyLink;
		friend	MyInstallationOrder;
		friend	MyInstallationOrder::MyBase;
		friend	MyCategory;
		friend	MyCategory::MyBase;
		friend	SystemManager;

		template<typename>
		friend class System;

	public:		// [DATA]
		dpl::ReadOnly<std::string,	ISystem> name;
		dpl::ReadOnly<uint64_t,		ISystem> updateCycle;
		dpl::ReadOnly<dpl::Timer,	ISystem> updateTimer;

	private:	// [LIFECYCLE]
		CLASS_CTOR			ISystem(					const Binding&		BINDING,
														std::string			sysName)
			: Variant(BINDING)
			, name(sysName)
			, updateCycle(0)
		{
			
		}

		CLASS_CTOR			ISystem(					const ISystem&		OTHER)			= delete;
		CLASS_CTOR			ISystem(					ISystem&&			other) noexcept = default;
		ISystem&			operator=(					const ISystem&		OTHER)			= delete;
		ISystem&			operator=(					ISystem&&			other) noexcept = default;

	public:		// [LIFECYCLE]
		CLASS_DTOR virtual ~ISystem() = default;

	public:		// [FUNCTIONS]
		double				get_total_update_time() const
		{
			return updateTimer().duration<dpl::Timer::Milliseconds>().count();
		}

		double				get_avrerage_update_time() const
		{
			return (updateCycle() > 0) ? (get_total_update_time() / updateCycle()) : 0.0;
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

	private:	// [DIAGNOSTIC]
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

	private:	// [INTERFACE]
		virtual void		on_save(					Settings&			settings) const {}

		virtual void		on_subsystems_saved(		Settings&			settings) const {}

		virtual void		on_load(					const Settings&		SETTINGS) {}

		virtual void		on_subsystems_loaded(		const Settings&		SETTINGS) {}

		virtual void		on_install() {}

		virtual void		on_update(					ParallelPhase&		phase) {}

		virtual void		on_subsystems_updated(		ParallelPhase&		phase) {}

		virtual void		on_uninstall() {}

	private:	// [FUNCTIONS]
		void				save(						Settings&			settings) const
		{
			on_save(settings);
			MySubsystems::for_each_member([&](const ISystem& SUBSYSTEM)
			{
				SUBSYSTEM.save(settings);
			});
			on_subsystems_saved(settings);
		}

		void				load(						const Settings&		SETTINGS)
		{
			on_load(SETTINGS);
			MySubsystems::for_each_member([&](ISystem& subsystem)
			{
				subsystem.load(SETTINGS);
			});
			on_subsystems_loaded(SETTINGS);
		}

		void				install()
		{
			reset_diagnostic();
			log_and_throw_on_exception([&](){on_install();});
			dpl::Logger::ref().push_info("Successfully installed system:  " + name());
		}

		void				update(						ParallelPhase&		phase)
		{
			++(*updateCycle);
			updateTimer().is_started()? updateTimer->unpause() : updateTimer->start();
			log_and_throw_on_exception([&]()
			{
				on_update(phase);
				MySubsystems::for_each_member([&](ISystem& subsystem)
				{
					subsystem.update(phase);
				});
				on_subsystems_updated(phase);
			});
			updateTimer->pause();
		}

		void				uninstall()
		{
			log_and_throw_on_exception([&](){on_uninstall();});
			log_diagnostic();
			reset_diagnostic();
		}
	};


	template<typename SystemT>
	class	System	: public ISystem
					, public dpl::Singleton<SystemT>
	{
	private:	// [SUBTYPES]
		using	MySingletonBase = dpl::Singleton<SystemT>;
		
	public:		// [FRIENDS]
		friend	SystemManager;

	public:		// [SUBTYPES]
		using	Binding = ISystem::Binding;

	protected:	// [LIFECYCLE]
		CLASS_CTOR		System(			const Binding&		BINDING)
			: ISystem(BINDING, dpl::undecorate_type_name<SystemT>())
			, MySingletonBase(SystemManager::ref().owner())
		{

		}

		CLASS_CTOR		System(			const System&		OTHER)			= delete;
		CLASS_CTOR		System(			System&&			other) noexcept = default;
		System&			operator=(		const System&		OTHER)			= delete;
		System&			operator=(		System&&			other) noexcept = default;
	};


	class	SystemManager	: public dpl::Singleton<SystemManager>
							, public dpl::Variation<SystemManager, ISystem>
							, private dpl::Sequence<ISystem, INSTALLATION_ORDER_HASH>
							, private dpl::Sequence<ISystem, SYSTEM_CATEGORY_HASH>
	{
	private:	// [SUBTYPES]
		using	MySystemCallback	= std::function<void(ISystem&)>;
		using	InstallationOrder	= dpl::Sequence<ISystem, INSTALLATION_ORDER_HASH>;
		using	RootSystems			= dpl::Sequence<ISystem, SYSTEM_CATEGORY_HASH>;

	public:		// [SUBTYPES]
		class	Installer
		{
		public:		// [FRIENDS]
			friend SystemManager;

		private:	// [LIFECYCLE]
			CLASS_CTOR	Installer()
			{

			}

		public:		// [FUNCTIONS]
			template<typename SystemT>
			void		install_system()
			{
				SystemManager::ref().install_system<SystemT>();
			}
		};

		using	OnInstall = std::function<void(Installer&)>;

	public:		// [FRIENDS]
		friend	Installer;
		friend	InstallationOrder;
		friend	RootSystems;
		
	private:	// [DATA]
		std::string			m_settingsFile;
		dpl::Logger			m_logger;
		dpl::ParallelPhase	m_phase; // All tasks must be done between system updates(call ThreadPool::wait when phase is done).

	protected:		// [LIFECYCLE]
		CLASS_CTOR		SystemManager(			Multition&			multition,
												const std::string&	SETTINGS_FILE,
												const uint32_t		NUM_THREADS = std::thread::hardware_concurrency())
			: Singleton(multition)
			, m_settingsFile(SETTINGS_FILE)
			, m_logger(multition)
			, m_phase(NUM_THREADS)
		{

		}

	protected:	// [FUNCTIONS]
		void			install_all_systems(	const OnInstall&	ON_INSTALL)
		{
			SystemManager::throw_if_systems_already_installed();
			m_logger.clear();
			m_logger.push_info("Installing...");
			Installer installer;
			ON_INSTALL(installer);
			SystemManager::load_settings();
		}

		void			update_all_systems()
		{
			RootSystems::for_each([&](ISystem& system)
			{
				system.update(m_phase);
				throw_if_phase_not_done();
			});
		}

		void			uninstall_all_systems()
		{
			m_logger.push_info("Uninstalling systems... ");
			SystemManager::save_settings();
			InstallationOrder::iterate_backwards([&](ISystem& system)
			{
				system.uninstall();
			});
			Variation::destroy_all_variants();
		}

	private:	// [IO]
		bool			save_settings() const
		{
			Settings settings;
			InstallationOrder::for_each([&](const ISystem& SYSTEM)
			{
				SYSTEM.on_save(settings);
			});
			return settings.save_to_binary(m_settingsFile);
		}

		bool			load_settings()
		{
			Settings settings;
			if(!settings.load_from_binary(m_settingsFile))	return false;
			InstallationOrder::for_each([&](ISystem& system)
			{
				system.on_load(settings);
			});
			return true;
		}

	private:	// [FUNCTIONS]
		template<typename SystemT>
		void			install_system()
		{
			auto result = Variation::create_variant<SystemT>();

			if(result)
			{
				if constexpr (is_RootSystem<SystemT>)
				{
					RootSystems::add_back(*result.get());
				}
				else // child system
				{
					using ParentT = typename Dependency_of<SystemT>::ParentSystem;
					if(System<ParentT>* parent = Variation::find_variant<ParentT>())
					{
						parent->add_end_member(*result.get());
					}
					else // handle error
					{
						Variation::destroy_variant<SystemT>();
						m_logger.push_error("Could not find given parent system: ", dpl::undecorate_type_name<ParentT>().c_str());
						throw dpl::GeneralException(this, __LINE__, "Installation failure!");
					}
				}

				ISystem*	newSystem = result.get();
							newSystem->install();

				InstallationOrder::add_back(*newSystem);
			}
		}

	private:	// [EXCEPTIONS]
		void			throw_if_systems_already_installed() const
		{
			if(Variation::get_numVariants() > 0)
				throw dpl::GeneralException(this, __LINE__, "Systems already installed.");
		}

		void			throw_if_phase_not_done() const
		{
#ifdef _DEBUG
			if(m_phase.numTasks() > 0)
				throw dpl::GeneralException(this, __LINE__, "Parallel phase not done.");
#endif // _DEBUG
		}
	};
}

#pragma warning( pop )