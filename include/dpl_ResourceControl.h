#pragma once


#include "dpl_Ownership.h"
#include "dpl_Subject.h"
#include "dpl_Labelable.h"
#include "dpl_NamedType.h"
#include "dpl_Variation.h"


// forward declarations
namespace dpl
{
	using	ResourceCharT = char;

	template<typename T>
	class	Resource;

	template<typename T>
	class	ResourceUser;

	class	IResourceControl;

	template<typename T>
	class	ResourceControl;

	class	ResourceManager;
}

// implementations
namespace dpl
{
	/*
		Base class for uniquely named resources.

		Derived class must implement "Resource(	const Ownership& OWNERSHIP, const Binding& BINDING);" constructor.
	*/
	template<typename T>
	class	Resource	: public Property<ResourceControl<T>, T>
						, public Labelable<ResourceCharT>
						, public Subject<T>
	{
	private: // subtypes
		using	MyOwner		= Property<ResourceControl<T>, T>;
		using	MyObservers	= Subject<T>;
		using	MyLabeler	= Labeler<ResourceCharT>;
		using	MyLabel		= typename Label<ResourceCharT>::Type;

	protected: // subtypes
		class	Binding
		{
		public: // relations
			friend ResourceControl<T>;
			friend Resource<T>;

		public: // lifecycle
			CLASS_CTOR	Binding()
				: labeler(nullptr)
				, name(nullptr)
			{

			}

		private: // lifecycle
			CLASS_CTOR	Binding(MyLabeler&		labeler,
								const MyLabel&	NAME)
				: labeler(&labeler)
				, name(&NAME)
			{

			}

		private: // data
			ReadOnly<MyLabeler*,		Binding> labeler;
			ReadOnly<const MyLabel*,	Binding> name;
		};

	public: // relations
		friend ResourceControl<T>;

	public: // functions
		using Labelable::import_label;
		using Labelable::export_label;

	protected: // lifecycle
		CLASS_CTOR		Resource(		const Ownership&		OWNERSHIP,
										const Binding&			BINDING)
			: MyOwner(OWNERSHIP)
		{
			if(MyLabeler* labeler = BINDING.labeler())
			{
				BINDING.name()	? labeler->label(*this, *BINDING.name()) 
								: labeler->label_with_postfix(*this, dpl::undecorate_type_name<T>());
			}					
		}

		CLASS_CTOR		Resource(		const Ownership&		OWNERSHIP, 
										Resource&&				other) noexcept
			: MyOwner(OWNERSHIP, std::move(other))
			, Labelable(std::move(other))
			, MyObservers(std::move(other))
		{
			
		}

		CLASS_DTOR		~Resource() = default;

		Resource&		operator=(		Swap<T>					other)
		{
			MyOwner::operator=(Swap(*other));
			Labelable::operator=(Swap<Labelable>(*other));
			MyObservers::operator=(Swap(*other));
			return *this;
		}

	private: // lifecycle
		CLASS_CTOR		Resource(		const Resource&			OTHER) = delete;

		CLASS_CTOR		Resource(		Resource&&				other) noexcept = delete;

		Resource&		operator=(		const Resource&			OTHER) = delete;

		Resource&		operator=(		Resource&&				other) noexcept = delete;

	private: // implementation
		virtual void	on_observers_changed() override final
		{
			if(!this->is_observed())
			{
				MyOwner::get_owner().destroy_resource(static_cast<T&>(*this));
			}
		}
	};


	template<typename T>
	class	ResourceUser	: public Observer<T>
	{
	public: // relations
		friend ResourceControl<T>;

	private: // subtypes 
		using MyBase	= Observer<T>;
		using MyLabel	= typename Label<ResourceCharT>::Type;

	private: // hidden functions
		MyBase::observe;
		MyBase::stop_observation;
		MyBase::has_subject;
		MyBase::get_subject;

	public: // lifecycle
		CLASS_CTOR				ResourceUser() = default;

		CLASS_CTOR				ResourceUser(		const ResourceUser&	OTHER) = delete;

		CLASS_CTOR				ResourceUser(		ResourceUser&&		other) noexcept = default;

		ResourceUser&			operator=(			const ResourceUser&	OTHER) = delete;

		ResourceUser&			operator=(			ResourceUser&&		other) noexcept = default;

		inline ResourceUser&	operator=(			Swap<ResourceUser>	other)
		{
			MyBase::operator=(Swap<Observer<T>>(*other));
			return *this;
		}

	public: // functions
		inline bool				has_resource() const
		{
			return MyBase::has_subject();
		}

		inline const T&			get_resource() const
		{
			return *MyBase::get_subject();
		}

	protected: // access
		inline T&				get_resource()
		{
			return *MyBase::get_subject();
		}

		inline void				discard()
		{
			MyBase::stop_observation();
		}

	protected: // Exportable implementation
		inline void				import_from(		std::istream&		binary, 
													ResourceControl<T>&	control)
		{
			MyLabel resourceName;
			dpl::import_dynamic_container(binary, resourceName);
			if(resourceName.size() > 0)
			{
				auto* resource = control.find_resource(resourceName);
				resource ? set_resource(*resource) : discard();
			}	
		}

		inline void				export_to(			std::ostream&		binary) const
		{
			static MyLabel NO_NAME;
			const MyLabel& RESOURCE_NAME = this->is_valid() ? get_resource().get_label() : NO_NAME;
			dpl::export_container(binary, RESOURCE_NAME);
		}

	private: // functions
		inline void				set_resource(		T&					newResource)
		{
			return MyBase::observe(newResource);
		}
	};


	class	IResourceControl : public Variant<ResourceManager, IResourceControl>
	{
	public: // relations
		friend ResourceManager;

	protected: // lifecycle
		CLASS_CTOR IResourceControl(const Binding& BINDING)
			: Variant(BINDING)
		{

		}

	public: // lifecycle
		CLASS_DTOR ~IResourceControl() = default;
	};


	template<typename ResourceT>
	class	ResourceControl	: public IResourceControl
							, public DynamicOwner<ResourceControl<ResourceT>, ResourceT>
	{
	public: // relations
		friend Resource<ResourceT>;

	private: // subtypes
		using	MyResources		= DynamicOwner<ResourceControl<ResourceT>, ResourceT>;
		using	MyLabeler		= Labeler<wchar_t>;
		using	ResourceBinding	= typename Resource<ResourceT>::Binding;

	public: // subtypes
		/*
			Stores TRUE/FALSE result of the function and associated data.
		*/
		template<typename DataT = const char*>
		class Result
		{
		private: // data
			ReadOnly<DataT,	Result> data;
			ReadOnly<bool,	Result> state;

		public: // lifecycle
			CLASS_CTOR		Result(	const bool	STATE,
									DataT		DATA)
				: data(DATA)
				, state(STATE)
			{

			}

			operator bool() const
			{
				return state();
			}

			inline DataT	get() const
			{
				return data();
			}

			inline DataT	operator->()
			{
				return get();
			}
		};

	private: // data
		ReadOnly<MyLabeler, ResourceControl> labeler;

	public: // lifecycle
		CLASS_CTOR			ResourceControl(	const Binding&				BINDING)
			: IResourceControl(BINDING)
		{

		}

	public: // inherited functions
		using MyResources::get_property_at;
		using MyResources::for_each_property;

	public: // functions
		inline uint32_t		get_numResources() const
		{
			return MyResources::numProperties();
		}

	private: // functions
		inline ResourceT*	find_resource(		const std::string&			NAME)
		{
			return static_cast<ResourceT*>(labeler->find_entry(NAME));
		}

		/*
			Creates new resource and assigns it to the user.
			REQUESTED_NAME may not be used if it's already taken.
		*/
		template<typename... CTOR>
		inline void			create_resource(	ResourceUser<ResourceT>&	user,
												const std::string&			REQUESTED_NAME,
												CTOR&&...					args)
		{
			user.set_resource(MyResources::create_property(ResourceBinding(*labeler, REQUESTED_NAME), std::forward<CTOR>(args)...));
		}

		/*
			Assigns resource with given NAME to the user.
			New resource is created with there is no existing one.
		*/
		template<typename... CTOR>
		inline void			use_resource(		ResourceUser<ResourceT>&	user,
												const std::string&			NAME,
												CTOR&&...					args)
		{
			if(ResourceT* existingResource = find_resource(NAME)) return user.set_resource(*existingResource);
			create_resource(user, NAME, std::forward<CTOR>(args)...);
		}

		inline bool			destroy_resource(	ResourceT&					resource)
		{
			return MyResources::destroy_property(resource);
		}
	};


	class	ResourceManager : public Variation<ResourceManager, IResourceControl>
	{
	private: // subtypes
		using MyControls	= Variation<ResourceManager, IResourceControl>;
		using MyLabel		= typename Label<ResourceCharT>::Type;

	public: // functions
		/*
			Creates new resource and assigns it to the user.
			REQUESTED_NAME may not be used if it's already taken.
		*/
		template<typename ResourceT, typename... CTOR>
		inline void							create_resource(ResourceUser<ResourceT>&	user,
															const MyLabel&				REQUESTED_NAME,
															CTOR&&...					args)
		{
			ResourceControl<ResourceT>& control = get_control<ResourceT>();
										control.create_resource(user, REQUESTED_NAME, std::forward<CTOR>(args)...);
		}

		/*
			Assigns resource with given NAME to the user.
			New resource is created with there is no existing one.
		*/
		template<typename ResourceT, typename... CTOR>
		inline void							use_resource(	ResourceUser<ResourceT>&	user,
															const MyLabel&				NAME,
															CTOR&&...					args)
		{
			ResourceControl<ResourceT>& control = get_control<ResourceT>();
										control.use_resource(user, NAME, std::forward<CTOR>(args)...);
		}

		template<typename ResourceT>
		inline void							destroy_resources()
		{
			MyControls::destroy_variant<ResourceT>();
		}

		inline void							destroy_all_resource_types()
		{
			MyControls::destroy_all_variants();
		}

	private: // functions
		template<typename ResourceT>
		inline ResourceControl<ResourceT>&	get_control()
		{
			auto* control = MyControls::find_variant<ResourceControl<ResourceT>>();
			return control? *control : *MyControls::create_variant<ResourceControl<ResourceT>>().get();
		}
	};
}