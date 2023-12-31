#pragma once


#include <algorithm>
#include "dpl_NamedType.h"
#include "dpl_Membership.h"
#include "dpl_Labelable.h"
#include "dpl_StaticHolder.h"
#include "dpl_std_addons.h"
#include "dpl_Logger.h"
#include "dpl_ComponentManager.h"
#include "dpl_Command.h"


/*
	[TODO]:
	- PartnerBase::set_partner
	- ParentBase::add_child (ONE_TO_ONE relation)
	- entity cmd locking (lock all DEPENDANT entities that were created with commands)
	- custom factory pattern for EntityManager and EntityPack
	- parent-child relation should not be possible if parent type is derived from one of the types that are already on the child's list of parents
	- parent-child relation should not be possible if child type is derived from one of the types that are already on the parent's list of children
	- Relation should accept any uint32_t except 0 as first argument which should be interpreted as max number of children.
*/


#pragma pack(push, 4)

// forward declarations		(internal)
namespace dpl
{
	class	Name;
	class	Origin;
	class	Identity;

	template<typename EntityT>
	class	Entity;

	class	EntityPack;

	template<typename EntityT>
	class	EntityStorageNode;

	template<typename EntityT>
	class	EntityPack_of;

	template<typename EntityT>
	class	EntityPackView;

	class	EntityManager;
}

// entity info				<------------------------------ FOR THE USER
namespace dpl
{
	enum	RelationType : uint32_t
	{
		ONE_TO_ONE = 1, // A parent can only have one child of the given type.
		// ONE_TO_SPECIFIC_NUMBER // Allows specific number of children. (TODO)
		ONE_TO_MANY = std::numeric_limits<uint32_t>::max() // A parent can have many children of the given type.
	};

	enum	Dependency
	{
		WEAK_DEPENDENCY,	// Child will not be destroyed along with the parent on EntityManager::cmd_destroy_hierarchy.
		STRONG_DEPENDENCY	// Child will be destroyed along with the parent on EntityManager::cmd_destroy_hierarchy.
	};

	template<RelationType USER_TYPE, Dependency USER_DEPENDENCY>
	struct	Relation
	{
		static const RelationType	TYPE		= USER_TYPE;
		static const Dependency		DEPENDENCY	= USER_DEPENDENCY;
	};

	template<typename ParentT, typename ChildT>
	struct	Relation_between;

	//template<typename ParentT, typename ChildT>
	//struct	ChildFilter;

	template<typename EntityT>
	struct	Description_of;

	template<typename EntityT>
	using	Base_of				= typename Description_of<EntityT>::BaseType;

	template<typename EntityT>
	using	ParentList_of		= typename Description_of<EntityT>::ParentTypes;

	template<typename EntityT>
	using	ChildList_of		= typename Description_of<EntityT>::ChildTypes;

	template<typename EntityT>
	using	PartnerList_of		= typename Description_of<EntityT>::PartnerTypes;

	template<typename EntityT>
	using	ComponentList_of	= typename Description_of<EntityT>::ComponentTypes;
}

// common concepts			(internal)
namespace dpl
{
	template<typename EntityT>
	concept is_Entity				=  std::is_base_of_v<Entity<EntityT>, EntityT>; //dpl::is_base_of_template<Entity, EntityT>;

	template<typename EntityT>
	concept is_FinalEntity			=  std::is_final_v<EntityT> 
									&& is_Entity<EntityT>;

	template<typename EntityT>
	struct	IsEntity : public std::conditional_t<is_Entity<EntityT>, std::true_type, std::false_type>{};

	template<typename ENTITY_TYPES>
	concept	is_EntityTypeList		=  dpl::is_TypeList<ENTITY_TYPES> 
									&& ENTITY_TYPES::ALL_UNIQUE 
									&& ENTITY_TYPES::template all<IsEntity>();

	template<typename DerivedT>
	struct	IsDerived
	{
		template<typename BaseT>
		struct	EntityType : public std::conditional_t<std::is_base_of_v<BaseT, DerivedT>, std::true_type, std::false_type>{};
	};

	template<typename T, typename ENTITY_TYPES>
	concept is_one_of_base_types	=  dpl::is_TypeList<ENTITY_TYPES> 
									&& ENTITY_TYPES::template any<IsDerived<T>::template EntityType>();

	template<typename T, typename ENTITY_TYPES> requires dpl::is_TypeList<ENTITY_TYPES> 
	using	Base_in_list			=  ENTITY_TYPES::template Subtypes<IsDerived<T>::template EntityType>::template At<0>;

	template<typename EntityT>
	concept has_Base				= !std::is_same_v<Base_of<EntityT>, EntityT>;
}

// queries					(internal)
namespace dpl
{
	template<typename EntityT>
	struct	RootBaseQuery
	{
	private:	using Base	= std::conditional_t<has_Base<EntityT>, typename Base_of<EntityT>, void>;
	public:		using Type	= std::conditional_t<has_Base<Base>, typename RootBaseQuery<Base>::Type, EntityT>;
	};

	template<>
	struct	RootBaseQuery<void>
	{
	public:		using Type	= void;
	};



	template<typename EntityT>
	struct	ParentQuery
	{
		using Base					= std::conditional_t<has_Base<EntityT>, typename Base_of<EntityT>, void>;
		using OwnParentTypes		= typename ParentList_of<EntityT>;
		using InheritedParentTypes	= typename ParentQuery<Base>::AllParentTypes;

		using AllParentTypes		= dpl::merge_t<	OwnParentTypes, 
													InheritedParentTypes>;
	};

	template<>
	struct	ParentQuery<void>
	{
		using AllParentTypes		= dpl::TypeList<>;
	};



	template<typename EntityT>
	struct	ChildQuery
	{
		using Base					= std::conditional_t<has_Base<EntityT>, typename Base_of<EntityT>, void>;
		using OwnChildTypes			= typename ChildList_of<EntityT>;
		using InheritedChildTypes	= typename ChildQuery<Base>::AllChildTypes;

		using AllChildTypes			= dpl::merge_t<	OwnChildTypes, 
													InheritedChildTypes>;
	};

	template<>
	struct	ChildQuery<void>
	{
		using AllChildTypes			= dpl::TypeList<>;
	};



	template<typename EntityT>
	struct	PartnerQuery
	{
		using Base					= std::conditional_t<has_Base<EntityT>, typename Base_of<EntityT>, void>;
		using OwnPartnerTypes		= typename PartnerList_of<EntityT>;
		using InheritedPartnerTypes	= typename PartnerQuery<Base>::AllPartnerTypes;

		using AllPartnerTypes		= dpl::merge_t<	OwnPartnerTypes, 
													InheritedPartnerTypes>;
	};

	template<>
	struct	PartnerQuery<void>
	{
		using AllPartnerTypes		= dpl::TypeList<>;
	};



	template<typename EntityT>
	struct	ComponentQuery
	{
		using Base						= std::conditional_t<has_Base<EntityT>, typename Base_of<EntityT>, void>;
		using OwnComponentTypes			= typename ComponentList_of<EntityT>;
		using InheritedComponentTypes	= typename ComponentQuery<Base>::AllComponentTypes;

		using AllComponentTypes			= dpl::merge_t<	OwnComponentTypes, 
														InheritedComponentTypes>;
	};

	template<>
	struct	ComponentQuery<void>
	{
		using AllComponentTypes = dpl::TypeList<>;
	};
}

// query results			(internal)
namespace dpl
{
	template<typename EntityT>
	using	RootBase_of				= typename RootBaseQuery<EntityT>::Type;

	template<typename EntityT>
	using	InheritedParentTypes_of	= typename ParentQuery<EntityT>::InheritedParentTypes;

	template<typename EntityT>
	using	InheritedChildTypes_of	= typename ChildQuery<EntityT>::InheritedChildTypes;

	template<typename EntityT>
	using	AllParentTypes_of		= typename ParentQuery<EntityT>::AllParentTypes;

	template<typename EntityT>
	using	AllChildTypes_of		= typename ChildQuery<EntityT>::AllChildTypes;

	template<typename EntityT>
	using	AllPartnerTypes_of		= typename PartnerQuery<EntityT>::AllPartnerTypes;

	template<typename EntityT>
	using	AllComponentTypes_of	= typename ComponentQuery<EntityT>::AllComponentTypes;


	template<typename ParentT, typename ChildT>
	concept one_of_parent_types_of	= dpl::is_one_of<ParentT, AllParentTypes_of<ChildT>>;

	template<typename ChildT, typename ParentT>
	concept one_of_child_types_of	= dpl::is_one_of<ChildT, AllChildTypes_of<ParentT>>;

	template<typename PartnerT, typename EntityT>
	concept one_of_partner_types_of	= dpl::is_one_of<PartnerT, AllPartnerTypes_of<EntityT>>;
}

// identity & origin		(internal)
namespace dpl
{
	// Stores naming convention of the entity.
	class	Name
	{
	public:		// [SUBTYPES]
		enum Type
		{
			UNIQUE, // Entity will be named with exact name given by the user.
			GENERIC // Entity name will be appended with generic postfix.
		};

	public:	// [DATA]
		dpl::ReadOnly<Type,			Name>	type;
		dpl::ReadOnly<std::string,	Name>	str;

	public:		// [LIFECYCLE]
		CLASS_CTOR		Name(		const Type				TYPE,
									const std::string_view	STR)
			: type(TYPE)
			, str(STR.data(), STR.size())
		{

		}

		CLASS_CTOR		Name(		const Type				TYPE,
									const std::string&		STR)
			: type(TYPE)
			, str(STR)
		{

		}

	public:		// [OPERATORS]
		operator const std::string&() const
		{
			return str;
		}

	public:		// [FUNCTIONS]
		void			change(		const Type				TYPE,
									const std::string&		STR)
		{
			type	= TYPE;
			str		= STR;
		}

		bool			has_type(	const Type				TYPE) const
		{
			return type() == TYPE;
		}

		bool			is_unique() const
		{
			return has_type(UNIQUE);
		}

		bool			is_generic() const
		{
			return has_type(GENERIC);
		}
	};


	// Wrapps name convention of the entity and typeID of the buffer that stores it.
	class	Origin
	{
	public:		// [FRIENDS]
		friend Identity;

		template<typename>
		friend class Entity;

		template<typename>
		friend class EntityPack_of;

	private:	// [SUBTYPES]
		using	Labelable_t	= dpl::Labelable<char>;
		using	Labeler_t	= dpl::Labeler<char>;

	public:		// [SUBTYPES]
		using	StorageID	= uint32_t;

	public:		// [DATA]
		dpl::ReadOnly<Name,			Origin> name;
		dpl::ReadOnly<StorageID,	Origin> storageID;

	private:	// [DATA]
		dpl::ReadOnly<Labeler_t*,	Origin> labeler;

	private:	// [LIFECYCLE]
		CLASS_CTOR Origin(	const Name&		NAME,
							const StorageID	STORAGE_ID,
							Labeler_t&		labeler)
			: name(NAME)
			, storageID(STORAGE_ID)
			, labeler(&labeler)
		{

		}
	};


	// Stores type information of the Entity.
	class	Identity : private dpl::Labelable<char>
	{
	private:	// [SUBTYPES]
		using	Labelable_t	= dpl::Labelable<char>;
		using	Labeler_t	= dpl::Labeler<char>;

	public:		// [SUBTYPES]
		using	StorageID	= Origin::StorageID;

	public:		// [CONSTANTS
		static const StorageID INVALID_STORAGE_ID = dpl::Variant<EntityManager, EntityPack>::INVALID_INDEX;

	public:		// [FRIENDS]
		template<typename>
		friend	class EntityPack_of;

	public:		// [DATA
		dpl::ReadOnly<StorageID, Identity> storageID; // TypeID of the EntityPack that stores this entity.

	public:		// [LIFECYCLE]
		CLASS_CTOR				Identity()
			: storageID(INVALID_STORAGE_ID)
		{

		}

		CLASS_CTOR				Identity(			const Origin&		ORIGIN)
			: storageID(ORIGIN.storageID)
		{
			set_name_internal(ORIGIN.name().type(), ORIGIN.name(), *ORIGIN.labeler());
		}

		CLASS_CTOR				Identity(			const Identity&		OTHER) = delete;
		CLASS_CTOR				Identity(			Identity&&			other) noexcept = default;
		Identity&				operator=(			const Identity&		OTHER) = delete;
		Identity&				operator=(			Identity&&			other) noexcept = default;

	public:		// [FUNCTIONS]
		const std::string&		name() const
		{
			return Labelable_t::get_label();
		}

		void					set_name(			const Name&			NEW_NAME)
		{		
			set_name_internal(NEW_NAME.type(), NEW_NAME, *get_labeler());
		}

		void					set_name(			const Name::Type	NAME_TYPE,
													const std::string&	STR)
		{		
			set_name_internal(NAME_TYPE, STR, *get_labeler());
		}

		template<is_Entity T>
		bool					has_type() const
		{
			return EntityPack_of<T>::ref().typeID() == storageID();
		}

		const std::string&		get_typeName() const;

	private:	// [FUNCTIONS]
		void					set_name_internal(	const Name::Type	NAME_TYPE,
													const std::string&	STR,
													Labeler_t&			labeler)
		{
			if(NAME_TYPE == Name::UNIQUE)	labeler.label(*this, STR);
			else							labeler.label_with_postfix(*this, STR);
		}
	};


	template<typename EntityT>
	using	MaybeIdentified	= std::conditional_t<!has_Base<EntityT>, Identity, Base_of<EntityT>>;
}

// invoke types				<------------------------------ FOR THE USER
namespace dpl
{
	using	InvokeIdentity = std::function<void(const Identity&)>;
		
	using	EntityIndex	= uint32_t;
	using	NumEntities	= uint32_t;

	template<is_Entity T> using	InvokeEntity						= std::function<void(T&)>;	
	template<is_Entity T> using	InvokeConstEntity					= std::function<void(const T&)>;	
	template<is_Entity T> using	InvokeIndexedEntity					= std::function<void(T&, EntityIndex)>;
	template<is_Entity T> using	InvokeConstIndexedEntity			= std::function<void(const T&, EntityIndex)>;
	template<is_Entity T> using	InvokeEntityBuffer					= std::function<void(T*, NumEntities)>;
	template<is_Entity T> using	InvokeConstEntityBuffer				= std::function<void(const T*, NumEntities)>;
	template<is_Entity T> using	InvokeSimilarEntityBuffer			= std::function<void(EntityPackView<T>&)>;
	template<is_Entity T> using InvokeSimilarIndexedEntityBuffer	= std::function<void(dpl::EntityPackView<T>, uint32_t)>;
}

// entity storage			<------------------------------ FOR THE USER
namespace dpl
{
	class	EntityPack : public dpl::Variant<EntityManager, EntityPack>
	{
	public:		// [FRIENDS]
		friend EntityManager;

		template<typename>
		friend class Entity;

		template<typename>
		friend class EntityPack_of;

	public:		// [COMMANDS]
		class	CMD_DestroyHierarchy;

	public:		// [CONSTANTS]
		static const uint32_t INVALID_ENTITY_ID = std::numeric_limits<uint32_t>::max();

	private:	// [LIFECYCLE]
		CLASS_CTOR					EntityPack(						const Binding&			BINDING)
			: Variant(BINDING)
		{

		}

	public:		// [INTERFACE]
		virtual const std::string&	get_entity_typeName() const = 0;

		virtual const Identity*		find_identity(					const std::string&		ENTITY_NAME) const = 0;

		virtual const Identity*		find_and_verify_identity(		const std::string&		ENTITY_NAME,
																	const uint32_t			TYPE_ID) const = 0;

		virtual const Identity&		guess_identity_from_byte(		const char*				ENTITY_MEMBER_BYTE_PTR) const = 0;

		virtual void				for_each_dependent_child(		const std::string&		ENTITY_NAME,
																	const Dependency		DEPENDENCY,
																	const InvokeIdentity&	INVOKE) const = 0;

	private:	// [INTERFACE]
		virtual bool				destroy_around(					const char*				ENTITY_MEMBER_BYTE_PTR) = 0;

		virtual char*				get_entity_as_bytes(			const std::string&		ENTITY_NAME) = 0;

		virtual char*				get_base_entity_as_bytes(		const std::string&		ENTITY_NAME) = 0;

		virtual void				destroy_all_entities() = 0;

		virtual void				save_and_destroy(				const std::string&		ENTITY_NAME,
																	BinaryState&			state) = 0;

		virtual void				create_and_load(				const std::string&		ENTITY_NAME,
																	BinaryState&			state) = 0;

	public:		// [FUNCTIONS]
		template<typename MemberT>
		const Identity&				guess_identity(					const MemberT*			ENTITY_MEMBER) const
		{
			return guess_identity_from_byte(reinterpret_cast<const char*>(ENTITY_MEMBER));
		}

		Identity&					get_identity(					const std::string&		ENTITY_NAME)
		{
			return const_cast<Identity&>(*find_identity(ENTITY_NAME));
		}

		const Identity&				get_identity(					const std::string&		ENTITY_NAME) const
		{
			return *find_identity(ENTITY_NAME);
		}

		template<typename EntityT>
		EntityT*					find_entity_if(					const std::string&		ENTITY_NAME)
		{
			return static_cast<EntityT*>(const_cast<Identity*>(find_and_verify_identity(ENTITY_NAME, EntityPack_of<EntityT>::ref().typeID())));
		}

		template<typename EntityT>
		const EntityT*				find_entity_if(					const std::string&		ENTITY_NAME) const
		{
			return static_cast<const EntityT*>(find_and_verify_identity(ENTITY_NAME, EntityPack_of<EntityT>::ref().typeID()));
		}
	};


	class	EntityManager	: public dpl::Singleton<EntityManager>
							, public dpl::Variation<EntityManager, EntityPack>
							, private dpl::StaticHolder<Identity, EntityManager>
	{
	public:		// [COMMANDS]
		template<typename T>
		class	CommandGroupOf;

		template<typename EntityT, dpl::is_TypeList RELATED_TYPES, template<typename, typename> class RelationCommand>
		class	MultiTypeCommand;

		template<is_Entity T>
		class	CMD_Create;

		template<is_Entity T>
		class	CMD_CreateGroupOf;

		class	CMD_Destroy;

		template<is_Entity ParentT, is_Entity ChildT>
		class	CMD_DestroyChildrenIf;

		template<is_Entity ParentT>
		class	CMD_DestroyAllChildrenOf;

		class	CMD_Rename;

		template<is_Entity ParentT, one_of_child_types_of<ParentT> ChildT>
		class	CMD_Adopt;

		template<is_Entity ParentT, one_of_child_types_of<ParentT> ChildT>
		class	CMD_Orphan;

		template<is_Entity ParentT, one_of_child_types_of<ParentT> ChildT>
		class	CMD_OrphanChildrenIf;

		template<is_FinalEntity T>
		class	CMD_OrphanAllChildrenOf;

		template<is_Entity EntityT, one_of_partner_types_of<EntityT> PartnerT>
		class	CMD_Involve;

		template<is_Entity EntityT, one_of_partner_types_of<EntityT> PartnerT>
		class	CMD_Disinvolve;

		template<is_Entity EntityT>
		class	CMD_DisinvolveAll;

	private:	// [DATA]
		BinaryInvoker m_invoker;

	public:		// [LIFECYCLE]
		CLASS_CTOR				EntityManager(				dpl::Multition&								multition)
			: Singleton(multition)
		{

		}

		CLASS_DTOR				~EntityManager()
		{
			dpl::no_except([&](){	destroy_all_entities();	});
		}

	public:		// [COMMAND FUNCTIONS]
		void					undo_command()
		{
			m_invoker.undo();
		}

		void					redo_command()
		{
			m_invoker.redo();
		}

		void					clear_commands()
		{
			m_invoker.clear();
		}

		template<is_Entity T>
		T&						cmd_create(					const Name									NAME)
		{
			EntityManager::assure_pack_of<T>();
			m_invoker.invoke<CMD_Create<T>>(NAME);
			return EntityPack_of<T>::ref().last();
		}

		template<is_Entity T>
		T&						cmd_create(					const Name::Type							TYPE,
															const std::string&							STR)
		{
			EntityManager::assure_pack_of<T>();
			m_invoker.invoke<CMD_Create<T>>(TYPE, STR);
			return EntityPack_of<T>::ref().last();
		}

		template<is_Entity T>
		T&						cmd_create(					const Name::Type							TYPE,
															const std::string_view						STR)
		{
			EntityManager::assure_pack_of<T>();
			m_invoker.invoke<CMD_Create<T>>(TYPE, STR);
			return EntityPack_of<T>::ref().last();
		}

		template<is_Entity T>
		void					cmd_create_group_of(		const std::string&							GROUP_PREFIX,
															const uint32_t								GROUP_SIZE)
		{
			EntityManager::assure_pack_of<T>();
			m_invoker.invoke<CMD_CreateGroupOf<T>>(GROUP_PREFIX, GROUP_SIZE);
		}

		void					cmd_destroy(				const Identity&								ENTITY);

		template<is_Entity ParentT, is_Entity ChildT>
		void					cmd_destroy_children_if(	const Entity<ParentT>&						PARENT)
		{
			m_invoker.invoke<CMD_DestroyChildrenIf<ParentT, ChildT>>(PARENT);
		}

		template<is_Entity ParentT>
		void					cmd_destroy_all_children_of(const Entity<ParentT>&						PARENT)
		{
			m_invoker.invoke<CMD_DestroyAllChildrenOf<ParentT>>(PARENT);
		}

		void					cmd_destroy_hierarchy(		const Identity&								ENTITY);

		void					cmd_rename(					const Identity&								ENTITY,
															const Name									NEW_NAME);

		void					cmd_rename(					const Identity&								ENTITY,
															const Name::Type							TYPE,
															const std::string&							STR);

		void					cmd_rename(					const Identity&								ENTITY,
															const Name::Type							TYPE,
															const std::string_view						STR);

		template<is_Entity ParentT, one_of_child_types_of<ParentT> ChildT>
		void					cmd_adopt(					const Entity<ParentT>&						PARENT,
															const Entity<ChildT>&						CHILD)
		{
			m_invoker.invoke<CMD_Adopt<ParentT, ChildT>>(PARENT, CHILD);
		}

		template<is_Entity ParentT, one_of_child_types_of<ParentT> ChildT>
		void					cmd_orphan(					const Entity<ChildT>&						CHILD)
		{
			m_invoker.invoke<CMD_Orphan<ParentT, ChildT>>(CHILD);
		}

		template<is_Entity ParentT, one_of_child_types_of<ParentT> ChildT>
		void					cmd_orphan_children_if(		const Entity<ParentT>&						PARENT)
		{
			m_invoker.invoke<CMD_OrphanChildrenIf<ParentT, ChildT>>(PARENT);
		}

		template<is_FinalEntity ParentT>
		void					cmd_orphan_all_children_of(	const Entity<ParentT>&						PARENT)
		{
			m_invoker.invoke<CMD_OrphanAllChildrenOf<ParentT>>(PARENT);
		}

		template<is_Entity EntityT, one_of_partner_types_of<EntityT> PartnerT>
		void					cmd_involve(				const Entity<EntityT>&						ENTITY,
															const Entity<PartnerT>&						PARTNER)
		{
			m_invoker.invoke<CMD_Involve<EntityT, PartnerT>>(ENTITY, PARTNER);
		}

		template<is_Entity EntityT, one_of_partner_types_of<EntityT> PartnerT>
		void					cmd_disinvolve(				const Entity<EntityT>&						ENTITY)
		{
			m_invoker.invoke<CMD_Disinvolve<EntityT, PartnerT>>(ENTITY);
		}

		template<is_Entity EntityT>
		void					cmd_disinvolve_all(			const Entity<EntityT>&						ENTITY)
		{
			m_invoker.invoke<CMD_DisinvolveAll<EntityT>>(ENTITY);
		}

	public:		// [ITERATION]
		template<is_Entity T>
		void					for_each(					const InvokeEntity<T>&						INVOKE)
		{
			EntityPack_of<T>::ref().for_each(INVOKE);
		}

		template<is_Entity T>
		void					for_each(					const InvokeConstEntity<T>&					INVOKE) const
		{
			EntityPack_of<T>::ref().for_each(INVOKE);
		}

		template<is_Entity T>
		void					for_each(					const InvokeIndexedEntity<T>&				INVOKE)
		{
			EntityPack_of<T>::ref().for_each(INVOKE);
		}

		template<is_Entity T>
		void					for_each(					const InvokeConstIndexedEntity<T>&			INVOKE)
		{
			EntityPack_of<T>::ref().for_each(INVOKE);
		}

		template<is_Entity T>
		void					for_each(					const InvokeEntityBuffer<T>&				INVOKE)
		{
			EntityPack_of<T>::ref().for_each(INVOKE);
		}

		template<is_Entity T>
		void					for_each(					const InvokeConstEntityBuffer<T>&			INVOKE) const
		{
			EntityPack_of<T>::ref().for_each(INVOKE);
		}

		template<is_Entity T>
		void					for_each(					const InvokeSimilarEntityBuffer<T>&			INVOKE)
		{
			EntityPack_of<T>::ref().for_each(INVOKE);
		}

	public:		// [PARALLEL ITERATION]
		template<is_Entity T>
		void					for_each_in_parallel(		dpl::ParallelPhase&							phase,
															const InvokeEntity<T>&						INVOKE)
		{
			EntityPack_of<T>::ref().for_each_in_parallel(phase, INVOKE);
		}

		template<is_Entity T>
		void					for_each_in_parallel(		dpl::ParallelPhase&							phase,
															const InvokeConstEntity<T>&					INVOKE) const
		{
			EntityPack_of<T>::ref().for_each_in_parallel(phase, INVOKE);
		}

		template<is_Entity T>
		void					for_each_in_parallel(		dpl::ParallelPhase&							phase,
															const InvokeIndexedEntity<T>&				INVOKE)
		{
			EntityPack_of<T>::ref().for_each_in_parallel(phase, INVOKE);
		}

		template<is_Entity T>
		void					for_each_in_parallel(		dpl::ParallelPhase&							phase,
															const InvokeConstIndexedEntity<T>&			INVOKE)
		{
			EntityPack_of<T>::ref().for_each_in_parallel(phase, INVOKE);
		}

		template<is_Entity T>
		void					for_each_in_parallel(		dpl::ParallelPhase&							phase,
															const InvokeSimilarIndexedEntityBuffer<T>&	INVOKE)
		{
			EntityPack_of<T>::ref().for_each_in_parallel(phase, INVOKE);
		}

	public:		// [FUNCTIONS]
		static const Identity&	false_identity()
		{
			return StaticHolder::data;
		}

		template<is_Entity T>
		bool					create_EntityPack_of()
		{
			return Variation::create_variant<EntityPack_of<T>>();
		}

	private:	// [INTERNAL FUNCTIONS]
		template<is_Entity T>
		void					assure_pack_of()
		{
			if(!EntityPack_of<T>::ptr()) EntityManager::create_EntityPack_of<T>();
		}

		void					destroy_all_entities()
		{
			Variation::for_each_variant([](EntityPack& pack)
			{
				pack.destroy_all_entities();
			});
		}
	};


	inline const std::string&	Identity::get_typeName() const
	{
		EntityPack* pack = EntityManager::ref().find_base_variant(storageID());
		return pack? pack->get_entity_typeName() : "??unknown_entity_type??";
	}
}

// references				(internal, RTTI)
namespace dpl
{
	class	Reference
	{
	public:		// [DATA]
		dpl::ReadOnly<Identity::StorageID,	Reference> storageID;
		dpl::ReadOnly<std::string,			Reference> name;

	public:		// [LIFECYCLE]
		CLASS_CTOR					Reference()
			: storageID(EntityPack::INVALID_INDEX)
		{
		}

		CLASS_CTOR					Reference(				const Identity&			IDENTITY)
			: storageID(IDENTITY.storageID())
			, name(IDENTITY.name())
		{

		}

		CLASS_CTOR					Reference(				BinaryState&			state)
			: Reference()
		{
			load_from_binary(state);
		}

		template<typename EntityT>
		CLASS_CTOR					Reference(				const Entity<EntityT>*	ENTITY)
			: Reference()
		{
			Reference::reset<EntityT>(ENTITY);
		}

	public:		// [OPERATORS]
		bool						operator==(				const Reference&		OTHER) const
		{
			return (this->storageID() == OTHER.storageID()) && (this->name() == OTHER.name());
		}

		bool						operator!=(				const Reference&		OTHER) const
		{
			return !Reference::operator==(OTHER);
		}

	public:		// [FUNCTIONS]
		bool						valid() const
		{
			return (storageID() != EntityPack::INVALID_INDEX) && !name().empty();
		}

		template<typename EntityT>
		void						reset(					const Entity<EntityT>*	ENTITY)
		{
			const Identity& IDENTITY = Reference::get_identity<EntityT>(ENTITY);
			storageID		= IDENTITY.storageID();
			name			= IDENTITY.name();
		}

		template<typename EntityT>
		Entity<EntityT>*			find() const
		{
			if(EntityPack* pack = EntityManager::ref().find_base_variant(storageID()))
			{
				return pack->find_entity_if<EntityT>(name());
			}

			return nullptr;
		}

		template<typename EntityT>
		Entity<EntityT>&			get() const
		{
			return *Reference::find<EntityT>();
		}

		void						load_from_binary(		BinaryState&			state)
		{
			state.load(*storageID);
			state.load(*name);
		}

		void						save_to_binary(			BinaryState&			state) const
		{
			state.save<uint32_t>(storageID());
			state.save(name());
		}

		static void					save_to_binary_opt(		const Identity&			IDENTITY,
															BinaryState&			state)
		{
			state.save<uint32_t>(IDENTITY.storageID());
			state.save(IDENTITY.name());
		}

		template<typename EntityT>
		static void					save_to_binary_opt(		const Entity<EntityT>&	ENTITY,
															BinaryState&			state)
		{
			save_to_binary_opt((const Identity&)ENTITY, state);
		}

		template<typename EntityT>
		static void					save_to_binary_opt(		const Entity<EntityT>*	ENTITY,
															BinaryState&			state)
		{
			Reference::save_to_binary_opt(Reference::get_identity<EntityT>(ENTITY), state);
		}

	private:	// [FUNCTIONS]
		template<typename EntityT>
		static const Identity&		get_identity(			const Entity<EntityT>*	ENTITY)
		{
			return ENTITY? (const Identity&)(*ENTITY) : EntityManager::ref().false_identity();
		}
	};
}

// relation declarations	(internal)
namespace dpl
{
	template<typename ParentT, typename ChildT, RelationType TYPE>
	class	ChildBase;

	template<typename MeT, typename YouT>
	class	PartnerBase;

	template<typename ParentT, typename ChildT, RelationType TYPE>
	class	ParentBase;


	template<typename ChildT, dpl::is_TypeList PARENT_TYPES>
	class	Child;

	template<typename EntityT, dpl::is_TypeList PARTNER_TYPES>
	class	Partner;

	template<typename ParentT, dpl::is_TypeList CHILD_TYPES>
	class	Parent;


	// We use this offset internally to not block the inheritance from dpl::Sequenceable with default ID.
	constexpr uint32_t RELATION_ID_OFFSET = 10000;


	template<typename ParentT, typename ChildT>
	constexpr uint32_t		get_relation_ID()
	{
		using ParentTypeList = ParentList_of<ChildT>;
		return RELATION_ID_OFFSET + ParentTypeList::template index_of<ParentT>();
	}

	template<typename ParentT, typename ChildT>
	constexpr RelationType	get_relation_between()
	{
		return Relation_between<ParentT, ChildT>::TYPE;
	}

	template<typename ParentT, typename ChildT>
	constexpr Dependency	get_dependency_between()
	{
		return Relation_between<ParentT, ChildT>::DEPENDENCY;
	}

	template<typename ParentT, typename ChildT>
	constexpr bool			are_strongly_dependant()
	{
		return dpl::get_dependency_between<ParentT, ChildT>() == STRONG_DEPENDENCY;
	}


	template<typename ParentT, typename ChildT>
	using	ParentBase_t	= ParentBase<ParentT, ChildT, dpl::get_relation_between<ParentT, ChildT>()>;

	template<typename ParentT, typename ChildT>
	using	ChildBase_t		= ChildBase<ParentT, ChildT, dpl::get_relation_between<ParentT, ChildT>()>;
}

// Child interface			(internal)
namespace dpl
{
	template<typename ParentT, typename ChildT>
	class	ChildBase<ParentT, ChildT, RelationType::ONE_TO_ONE> 
		: private dpl::Association<ChildT, ParentT, dpl::get_relation_ID<ParentT, ChildT>()>
	{
	private:	// [SUBTYPES]
		static const uint32_t RELATION_ID	= dpl::get_relation_ID<ParentT, ChildT>();
		using	ParentBaseT					= ParentBase<ParentT, ChildT, RelationType::ONE_TO_ONE>;
		using	MyBaseT						= dpl::Association<ChildT, ParentT, RELATION_ID>;
		using	MyLinkT						= dpl::Association<ParentT, ChildT, RELATION_ID>;

	public:		// [FRIENDS]
		friend	ParentBaseT;
		friend	MyBaseT;
		friend	MyLinkT;

		template<typename, dpl::is_TypeList>
		friend class	Parent;

		template<typename, dpl::is_TypeList>
		friend class	Child;

	protected:	// [LIFECYCLE]
		CLASS_CTOR			ChildBase() = default;
		CLASS_CTOR			ChildBase(			ChildBase&&				other) noexcept = default;
		ChildBase&			operator=(			ChildBase&&				other) noexcept = default;

	private:	// [LIFECYCLE] (deleted)
		CLASS_CTOR			ChildBase(			const ChildBase&		OTHER) = delete;
		ChildBase&			operator=(			const ChildBase&		OTHER) = delete;

	private:	// [FUNCTIONS]
		bool				has_parent() const
		{
			return MyBaseT::is_linked();
		}

		ParentT&			get_parent()
		{
			return *MyBaseT::other();
		}

		const ParentT&		get_parent() const
		{
			return *MyBaseT::other();
		}

		ChildT*				get_prev_sibling()
		{
			return nullptr;
		}

		const ChildT*		get_prev_sibling() const
		{
			return nullptr;
		}

		ChildT*				get_next_sibling()
		{
			return nullptr;
		}

		const ChildT*		get_next_sibling() const
		{
			return nullptr;
		}

		void				save_parent(		BinaryState&			state) const
		{
			Reference::save_to_binary_opt(MyBaseT::other(), state);
		}

		void				load_parent(		BinaryState&			state)
		{
			MyBaseT::unlink(); //<-- Detach from previous parent.
			const Reference REFERENCE(state);
			if(ParentBaseT* parent = REFERENCE.find<ParentT>())
			{
				MyBaseT::link(*parent);
			}
			else // log error
			{
				dpl::Logger::ref().push_error("Fail to import relation. The specified parent could not be found: " + REFERENCE.name());
			}
		}
	};


	template<typename ChildT, typename ParentT>
	class	ChildBase<ParentT, ChildT, RelationType::ONE_TO_MANY> 
		: private dpl::Member<ParentT, ChildT, dpl::get_relation_ID<ParentT, ChildT>()>
	{
	private:	// [SUBTYPES]
		static const uint32_t RELATION_ID	= dpl::get_relation_ID<ParentT, ChildT>();
		using	ParentBaseT					= ParentBase<ParentT, ChildT, RelationType::ONE_TO_MANY>;
		using	MyMemberT					= dpl::Member<ParentT, ChildT, RELATION_ID>;
		using	MyGroupT					= dpl::Group<ParentT, ChildT, RELATION_ID>;

	public:		// [FRIENDS]
		friend	ParentBaseT;
		friend	MyMemberT;
		friend	MyMemberT::MySequence;
		friend	MyMemberT::MyLink;
		friend	MyMemberT::MyGroup;
		friend	MyMemberT::MyBase;
		friend	MyGroupT;

		template<typename, dpl::is_TypeList>
		friend class	Parent;

		template<typename, dpl::is_TypeList>
		friend class	Child;

	protected:	// [LIFECYCLE]
		CLASS_CTOR			ChildBase() = default;
		CLASS_CTOR			ChildBase(			ChildBase&&				other) noexcept = default;
		ChildBase&			operator=(			ChildBase&&				other) noexcept = default;

	private:	// [LIFECYCLE] (deleted)
		CLASS_CTOR			ChildBase(			const ChildBase&		OTHER) = delete;
		ChildBase&			operator=(			const ChildBase&		OTHER) = delete;

	private:	// [FUNCTIONS]
		bool				has_parent() const
		{
			return MyMemberT::is_member();
		}

		ParentT&			get_parent()
		{
			return *MyMemberT::get_group();
		}

		const ParentT&		get_parent() const
		{
			return *MyMemberT::get_group();
		}

		ChildT*				get_prev_sibling()
		{
			return MyMemberT::previous();
		}

		const ChildT*		get_prev_sibling() const
		{
			return MyMemberT::previous();
		}

		ChildT*				get_next_sibling()
		{
			return MyMemberT::next();
		}

		const ChildT*		get_next_sibling() const
		{
			return MyMemberT::next();
		}

		void				save_parent(		BinaryState&			state) const
		{
			Reference::save_to_binary_opt(MyMemberT::get_group(), state);
		}

		void				load_parent(		BinaryState&			state)
		{
			MyMemberT::detach();
			const Reference REFERENCE(state);
			if(ParentBaseT* parent = REFERENCE.find<ParentT>()) 
			{
				parent->add_end_member(*this);
			}
			else // log error
			{
				dpl::Logger::ref().push_error("Fail to import relation. The specified parent could not be found: " + REFERENCE.name());
			}
		}
	};


	template<typename ChildT, typename... ParentTn>
	class	Child<ChildT, dpl::TypeList<ParentTn...>>	: public MaybeIdentified<ChildT>
														, public ChildBase_t<ParentTn, ChildT>...
	{
	public:		// [FRIENDS]
		friend	EntityManager;

		template<typename, dpl::is_TypeList>
		friend	class Partner;

	private:	// [SUBTYPES]
		using	PARENT_TYPES	= dpl::TypeList<ParentTn...>;

		using	MyIdentity		= MaybeIdentified<ChildT>;

		template<dpl::is_one_of<PARENT_TYPES> ParentT>
		using	MyChildBase		= ChildBase_t<ParentT, ChildT>;

	protected:	// [LIFECYCLE]
		CLASS_CTOR			Child(						const Origin&		ORIGIN)
			: MyIdentity(ORIGIN)
		{

		}

		CLASS_CTOR			Child(						Child&&				other) noexcept = default;
		Child&				operator=(					Child&&				other) noexcept = default;

	private:	// [LIFECYCLE]
		CLASS_CTOR			Child(						const Child&		OTHER) = delete;
		Child&				operator=(					const Child&		OTHER) = delete;

	public:		// [FUNCTIONS]
		template<dpl::is_one_of<PARENT_TYPES>	ParentT>
		bool				has_parent() const
		{
			return MyChildBase<ParentT>::has_parent();
		}

		template<dpl::is_one_of<PARENT_TYPES>	ParentT>
		ParentT&			get_parent()
		{
			return MyChildBase<ParentT>::get_parent();
		}

		template<dpl::is_one_of<PARENT_TYPES>	ParentT>
		const ParentT&		get_parent() const
		{
			return MyChildBase<ParentT>::get_parent();
		}

		template<dpl::is_one_of<PARENT_TYPES>	ParentT>
		ChildT*				get_prev_sibling()
		{
			return MyChildBase<ParentT>::get_prev_sibling();
		}

		template<dpl::is_one_of<PARENT_TYPES>	ParentT>
		const ChildT*		get_prev_sibling() const
		{
			return MyChildBase<ParentT>::get_prev_sibling();
		}

		template<dpl::is_one_of<PARENT_TYPES>	ParentT>
		ChildT*				get_next_sibling()
		{
			return MyChildBase<ParentT>::get_next_sibling();
		}

		template<dpl::is_one_of<PARENT_TYPES>	ParentT>
		const ChildT*		get_next_sibling() const
		{
			return MyChildBase<ParentT>::get_next_sibling();
		}

		template<dpl::is_one_of<PARENT_TYPES>	ParentT>
		bool				has_given_parent(			const ParentT&		PARENT) const
		{
			if(!has_parent<ParentT>()) return false;
			return &PARENT == &Child::get_parent();
		}

	protected:	// [FUNCTIONS]
		template<dpl::is_same_as<ChildT>		T = ChildT>
		void				save_parents_of_this(		BinaryState&		state) const
		{
			(MyChildBase<ParentTn>::save_parent(state), ...);
		}

		template<dpl::is_same_as<ChildT>		T = ChildT>
		void				load_parents_of_this(		BinaryState&		state)
		{
			(MyChildBase<ParentTn>::load_parent(state), ...);
		}

	private:	// [FUNCTIONS]
		void				save_relation_hierarchy(	BinaryState&		state) const
		{
			Child::save_parents_of_this(state);
		}

		void				load_relation_hierarchy(	BinaryState&		state)
		{
			Child::load_parents_of_this(state);
		}
	};


	// Specialization for child without parents.
	template<typename ChildT>
	class	Child<ChildT, dpl::TypeList<>> : public MaybeIdentified<ChildT>
	{
	private:	// [SUBTYPES]
		using	MyIdentity	= MaybeIdentified<ChildT>;

	public:		// [FRIENDS]
		template<typename, dpl::is_TypeList>
		friend	class Partner;

	protected:	// [LIFECYCLE]
		CLASS_CTOR			Child(						const Origin&		ORIGIN)
			: MyIdentity(ORIGIN)
		{

		}

		CLASS_CTOR			Child(						Child&&				other) noexcept = default;
		Child&				operator=(					Child&&				other) noexcept = default;

	private:	// [FUNCTIONS]
		void				save_relation_hierarchy(	BinaryState&		state) const
		{
			// dummy function
		}

		void				load_relation_hierarchy(	BinaryState&		state)
		{
			// dummy function
		}
	};
}

// Partner interface		(internal)
namespace dpl
{
	template<typename MeT, typename YouT>
	constexpr uint32_t	get_partner_ID()
	{
		using PartnerTypeList = PartnerList_of<MeT>;
		return dpl::RELATION_ID_OFFSET + ParentList_of<MeT>::SIZE + PartnerTypeList::template index_of<YouT>();
	}


	template<typename MeT, typename YouT>
	class	PartnerBase : private dpl::Association<MeT, YouT, dpl::get_partner_ID<MeT, YouT>()>
	{
	private:	// [SUBTYPES]
		static const uint32_t RELATION_ID	= dpl::get_partner_ID<MeT, YouT>();
		using	MyPartnerT					= PartnerBase<YouT, MeT>;
		using	MyBaseT						= dpl::Association<MeT, YouT, RELATION_ID>;
		using	MyLinkT						= dpl::Association<YouT, MeT, RELATION_ID>;

	public:		// [FRIENDS]
		friend	MyPartnerT;
		friend	MyBaseT;
		friend	MyLinkT;

		template<typename, dpl::is_TypeList>
		friend class	Partner;

	private:	// [LIFECYCLE]
		CLASS_CTOR			PartnerBase() = default;
		CLASS_CTOR			PartnerBase(					PartnerBase&&			other) noexcept = default;
		PartnerBase&		operator=(						PartnerBase&&			other) noexcept = default;

	private:	// [LIFECYCLE] (deleted)
		CLASS_CTOR			PartnerBase(					const PartnerBase&		OTHER) = delete;
		PartnerBase&		operator=(						const PartnerBase&		OTHER) = delete;

	private:	// [FUNCTIONS]
		bool				has_partner() const
		{
			return MyBaseT::is_linked();
		}

		bool				set_partner(					YouT&					partner)
		{
			return MyBaseT::link(partner);
		}

		bool				remove_partner()
		{
			return MyBaseT::unlink();
		}

		bool				remove_partner(					YouT&					partner)
		{
			if(MyBaseT::other() != &partner) return false;
			return Partner::remove_partner();
		}

		YouT&				get_partner()
		{
			return *MyBaseT::other();
		}

		const YouT&			get_partner() const
		{
			return *MyBaseT::other();
		}

		void				save_partner(					BinaryState&			state) const
		{
			Reference::save_to_binary_opt(MyBaseT::other(), state);
		}

		void				load_partner(					BinaryState&			state)
		{
			MyBaseT::unlink();
			const Reference REFERENCE(state);
			if(MyPartnerT* child = REFERENCE.find<YouT>())
			{
				MyBaseT::link(*child);
			}
			else // log error
			{
				dpl::Logger::ref().push_error("Fail to import relation. The specified partner could not be found: " + REFERENCE.name());
			}
		}
	};


	template<typename EntityT, typename... PartnerTs>
	class	Partner<EntityT, dpl::TypeList<PartnerTs...>>	: public Child<EntityT, ParentList_of<EntityT>>
															, public PartnerBase<EntityT, PartnerTs>...
	{
	private:	// [SUBTYPES]
		using	MyChildBase		= Child<EntityT, ParentList_of<EntityT>>;

	public:		// [SUBTYPES]
		using	PARTNER_TYPES	= dpl::TypeList<PartnerTs...>;

	public:		// [FRIENDS]
		friend	EntityManager;

		template<typename, typename>
		friend	class PartnerBase;

		template<typename, dpl::is_TypeList>
		friend	class Parent;

	protected:	// [LIFECYCLE]
		CLASS_CTOR			Partner(					const Origin&		ORIGIN)
			: MyChildBase(ORIGIN)
		{

		}

		CLASS_CTOR			Partner(					Partner&&			other) noexcept = default;
		Partner&			operator=(					Partner&&			other) noexcept = default;

	private:	// [LIFECYCLE] (deleted)
		CLASS_CTOR			Partner(					const Partner&		OTHER) = delete;
		Partner&			operator=(					const Partner&		OTHER) = delete;

	public:		// [FUNCTIONS]
		template<dpl::is_one_of<PARTNER_TYPES> PartnerT>
		bool				has_partner() const
		{
			return PartnerBase<EntityT, PartnerT>::has_partner();
		}

		template<dpl::is_one_of<PARTNER_TYPES> PartnerT>
		PartnerT&			get_partner()
		{
			return PartnerBase<EntityT, PartnerT>::get_partner();
		}

		template<dpl::is_one_of<PARTNER_TYPES> PartnerT>
		const PartnerT&		get_partner() const
		{
			return PartnerBase<EntityT, PartnerT>::get_partner();
		}

	protected:	// [FUNCTIONS]
		template<is_one_of_base_types<PARTNER_TYPES>	PartnerT>
		bool				set_partner(				PartnerT&			partner)
		{
			return PartnerBase<EntityT, Base_in_list<PartnerT, PARTNER_TYPES>>::add_partner(partner);
		}

		template<is_one_of_base_types<PARTNER_TYPES>	PartnerT>
		bool				remove_partner(				PartnerT&			partner)
		{
			return PartnerBase<EntityT, Base_in_list<PartnerT, PARTNER_TYPES>>::remove_partner(partner);
		}

		template<is_one_of_base_types<PARTNER_TYPES>	PartnerT>
		bool				remove_partner()
		{
			return PartnerBase<EntityT, Base_in_list<PartnerT, PARTNER_TYPES>>::remove_partner();
		}

		template<dpl::is_same_as<EntityT>	This = EntityT>
		void				remove_all_partners_of_this()
		{
			(PartnerBase<EntityT, PartnerTs>::remove_partner(), ...);
		}

		template<dpl::is_same_as<EntityT>	This = EntityT>
		void				save_partners_of_this(		BinaryState&		state) const
		{
			(PartnerBase<EntityT, PartnerTs>::save_partner(state), ...);
		}

		template<dpl::is_same_as<EntityT>	This = EntityT>
		void				load_partners_of_this(		BinaryState&		state)
		{
			(PartnerBase<EntityT, PartnerTs>::load_partner(state), ...);
		}

	private:	// [FUNCTIONS]
		void				save_relation_hierarchy(	BinaryState&		state) const
		{
			MyChildBase::save_relation_hierarchy(state);
			Partner::save_partners_of_this(state);
		}

		void				load_relation_hierarchy(	BinaryState&		state)
		{
			MyChildBase::load_relation_hierarchy(state);
			Partner::load_partners_of_this(state);
		}
	};


	// Specialization of entity without partners.
	template<typename EntityT>
	class	Partner<EntityT, dpl::TypeList<>> : public Child<EntityT, ParentList_of<EntityT>>
	{
	private:	// [SUBTYPES]
		using	MyChildBase	= Child<EntityT, ParentList_of<EntityT>>;

	public:		// [FRIENDS]
		template<typename, dpl::is_TypeList>
		friend	class Parent;

	protected:	// [LIFECYCLE]
		CLASS_CTOR			Partner(					const Origin&		ORIGIN)
			: MyChildBase(ORIGIN)
		{

		}

		CLASS_CTOR			Partner(					Partner&&			other) noexcept = default;
		Partner&			operator=(					Partner&&			other) noexcept = default;

	private:	// [FUNCTIONS]
		void				save_relation_hierarchy(	BinaryState&		state) const
		{
			MyChildBase::save_relation_hierarchy(state);
		}

		void				load_relation_hierarchy(	BinaryState&		state)
		{
			MyChildBase::load_relation_hierarchy(state);
		}
	};
}

// Parent interface			(internal)
namespace dpl
{
	template<typename ParentT, typename ChildT>
	class	ParentBase<ParentT, ChildT, RelationType::ONE_TO_ONE> 
		: private dpl::Association<ParentT, ChildT, dpl::get_relation_ID<ParentT, ChildT>()>
	{
	private: // subtypes
		static const uint32_t RELATION_ID	= dpl::get_relation_ID<ParentT, ChildT>();
		using	ChildBaseT					= ChildBase<ParentT, ChildT, RelationType::ONE_TO_ONE>;
		using	MyBaseT						= dpl::Association<ParentT, ChildT, RELATION_ID>;
		using	MyLinkT						= dpl::Association<ChildT, ParentT, RELATION_ID>;

	public: // subtypes
		using	InvokeChild					= std::function<void(ChildT&)>;
		using	InvokeConstChild			= std::function<void(const ChildT&)>;
		using	InvokeChildUntil			= std::function<bool(ChildT&)>;
		using	InvokeConstChildUntil		= std::function<bool(const ChildT&)>;

	public: // friends
		friend	ChildBaseT;
		friend	MyBaseT;
		friend	MyLinkT;

		template<typename, dpl::is_TypeList>
		friend class	Parent;

		template<typename, dpl::is_TypeList>
		friend class	Child;

	private: // lifecycle
		CLASS_CTOR			ParentBase() = default;
		CLASS_CTOR			ParentBase(						ParentBase&&					other) noexcept = default;
		ParentBase&			operator=(						ParentBase&&					other) noexcept = default;

	private: // lifecycle (deleted)
		CLASS_CTOR			ParentBase(						const ParentBase&				OTHER) = delete;
		ParentBase&			operator=(						const ParentBase&				OTHER) = delete;

	private: // functions
		bool				has_child() const
		{
			return MyBaseT::is_linked();
		}

		bool				can_have_another_child() const
		{
			return !has_child();
		}

		uint32_t			numChildren() const
		{
			return has_child()? 1 : 0;
		}

		bool				add_child(						ChildT&							child)
		{
			return MyBaseT::link(child);
		}

		bool				remove_child(					ChildT&							child)
		{
			if(MyBaseT::other() != &child) return false;
			return MyBaseT::unlink();
		}

		bool				remove_all_children()
		{
			return MyBaseT::unlink();
		}

		void				destroy_all_children()
		{
			if(this->has_child())
			{
				Entity<ChildT>& entity = *first_child();
								entity.selfdestruct();
			}
		}

		ChildT&				get_child()
		{
			return *MyBaseT::other();
		}

		const ChildT&		get_child() const
		{
			return *MyBaseT::other();
		}

		ChildT*				first_child()
		{
			return MyBaseT::other();
		}

		const ChildT*		first_child() const
		{
			return MyBaseT::other();
		}

		ChildT*				last_child()
		{
			return MyBaseT::other();
		}

		const ChildT*		last_child() const
		{
			return MyBaseT::other();
		}

		ChildT*				previous_child(					MyLinkT&						child)
		{
			return nullptr;
		}

		const ChildT*		previous_child(					const MyLinkT&					CHILD) const
		{
			return nullptr;
		}

		ChildT*				next_child(						MyLinkT&						child)
		{
			return nullptr;
		}

		const ChildT*		next_child(						const MyLinkT&					CHILD) const
		{
			return nullptr;
		}

		uint32_t			for_each_child(					const InvokeChild&				INVOKE_CHILD)
		{
			if(!has_child()) return 0;
			INVOKE_CHILD(get_child());
			return 1;
		}

		uint32_t			for_each_child(					const InvokeConstChild&			INVOKE_CHILD) const
		{
			if(!has_child()) return 0;
			INVOKE_CHILD(get_child());
			return 1;
		}

		uint32_t			for_each_child_until(			const InvokeChildUntil&			INVOKE_CHILD)
		{
			return ParentBase::for_each_child([&](ChildT& child){ INVOKE_CHILD(child); });
		}

		uint32_t			for_each_child_until(			const InvokeConstChildUntil&	INVOKE_CHILD) const
		{
			return ParentBase::for_each_child([&](const ChildT& CHILD){ INVOKE_CHILD(CHILD); });
		}

		void				save_children(					BinaryState&					state) const
		{
			Reference::save_to_binary_opt(first_child(), state);
		}

		void				load_children(					BinaryState&					state)
		{
			MyBaseT::unlink();
			const Reference REFERENCE(state);
			if(ChildBaseT* child = REFERENCE.find<ChildT>())
			{
				MyBaseT::link(*child);
			}
			else // log error
			{
				dpl::Logger::ref().push_error("Fail to import relation. The specified child could not be found: " + REFERENCE.name());
			}
		}
	};


	template<typename ParentT, typename ChildT>
	class	ParentBase<ParentT, ChildT, RelationType::ONE_TO_MANY> 
		: private dpl::Group<ParentT, ChildT, dpl::get_relation_ID<ParentT, ChildT>()>
	{
	private: // subtypes
		static const uint32_t RELATION_ID	= dpl::get_relation_ID<ParentT, ChildT>();
		using	ChildBaseT					= ChildBase<ParentT, ChildT, RelationType::ONE_TO_MANY>;
		using	MyMemberT					= dpl::Member<ParentT, ChildT, RELATION_ID>;
		using	MyGroupT					= dpl::Group<ParentT, ChildT, RELATION_ID>;

	public: // subtypes
		using	InvokeChild					= std::function<void(ChildT&)>;
		using	InvokeConstChild			= std::function<void(const ChildT&)>;
		using	InvokeChildUntil			= std::function<bool(ChildT&)>;
		using	InvokeConstChildUntil		= std::function<bool(const ChildT&)>;

	public: // friends
		friend	ChildBaseT;
		friend	MyMemberT;
		friend	MyGroupT;

		template<typename, dpl::is_TypeList>
		friend class	Parent;

		template<typename, dpl::is_TypeList>
		friend class	Child;

	private: // lifecycle
		CLASS_CTOR			ParentBase() = default;
		CLASS_CTOR			ParentBase(					ParentBase&&					other) noexcept = default;
		ParentBase&			operator=(					ParentBase&&					other) noexcept = default;

	private: // lifecycle (deleted)
		CLASS_CTOR			ParentBase(					const ParentBase&				OTHER) = delete;
		ParentBase&			operator=(					const ParentBase&				OTHER) = delete;

	private: // functions
		bool				has_child() const
		{
			return MyGroupT::size() > 0;
		}

		bool				can_have_another_child() const
		{
			return true;
		}

		uint32_t			numChildren() const
		{
			return MyGroupT::size();
		}
			
		bool				add_child(					ChildT&							child)
		{
			/*
			if (ChildFilter<ParentT, ChildT>()(static_cast<const ParentT&>(*this), child))
			{
				// TODO: add member here
			}
			*/
			return MyGroupT::add_end_member(child);
		}

		bool				remove_child(				ChildT&							child)
		{
			return MyGroupT::remove_member(child);
		}

		bool				remove_all_children()
		{
			return MyGroupT::remove_all_members();
		}

		void				destroy_all_children()
		{
			ParentBase::for_each_child([](ChildT& child) 
			{
				Entity<ChildT>& entity = child;
								entity.selfdestruct();
			});
		}

		ChildT&				get_child()
		{
			return *MyGroupT::first();
		}

		const ChildT&		get_child() const
		{
			return *MyGroupT::first();
		}

		ChildT*				first_child()
		{
			return MyGroupT::first();
		}

		const ChildT*		first_child() const
		{
			return MyGroupT::first();
		}

		ChildT*				last_child()
		{
			return MyGroupT::last();
		}

		const ChildT*		last_child() const
		{
			return MyGroupT::last();
		}

		ChildT*				previous_child(				MyMemberT&						child)
		{
			if(!child.is_member_of(*this)) return nullptr;
			return child.previous();
		}
			
		const ChildT*		previous_child(				const MyMemberT&				CHILD) const
		{
			if(!CHILD.is_member_of(*this)) return nullptr;
			return CHILD.previous();
		}

		ChildT*				next_child(					MyMemberT&						child)
		{
			if(!child.is_member_of(*this)) return nullptr;
			return child.next();
		}

		const ChildT*		next_child(					const MyMemberT&				CHILD) const
		{
			if(!CHILD.is_member_of(*this)) return nullptr;
			return CHILD.next();
		}

		uint32_t			for_each_child(				const InvokeChild&				INVOKE_CHILD)
		{
			return MyGroupT::for_each_member(INVOKE_CHILD);
		}

		uint32_t			for_each_child(				const InvokeConstChild&			INVOKE_CHILD) const
		{
			return MyGroupT::for_each_member(INVOKE_CHILD);
		}

		uint32_t			for_each_child_until(		const InvokeChildUntil&			INVOKE_CHILD)
		{
			return MyGroupT::for_each_member_until(INVOKE_CHILD);
		}

		uint32_t			for_each_child_until(		const InvokeConstChildUntil&	INVOKE_CHILD) const
		{
			return MyGroupT::for_each_member_until(INVOKE_CHILD);
		}

		void				detach_children()
		{
			while(ChildBaseT* child = first_child())
			{
				child->detach();
			}
		}

		void				save_children(				BinaryState&					state) const
		{
			state.save<uint32_t>(numChildren());
			ParentBase::for_each_child([&](const ChildT& CHILD)
			{
				Reference::save_to_binary_opt(CHILD, state);
			});
		}

		void				load_children(				BinaryState&					state)
		{
			detach_children();
			Reference reference;
			const uint32_t	NUM_CHILDREN = state.load<uint32_t>();
			for(uint32_t index = 0; index < NUM_CHILDREN; ++index)
			{
				reference.load_from_binary(state);

				if(ChildBaseT* child = reference.find<ChildT>())
				{
					MyGroupT::add_end_member(*child);
				}
				else //log error
				{
					dpl::Logger::ref().push_error("Fail to import relation. The specified child could not be found: " + reference.name());
				}
			}
		}
	};


	template<typename ParentT, typename... ChildTn>
	class	Parent<ParentT, dpl::TypeList<ChildTn...>>	: public Partner<ParentT, PartnerList_of<ParentT>>
														, public ParentBase_t<ParentT, ChildTn>...
	{
	private:	// [SUBTYPES]
		using	MyPartnerBase	= Partner<ParentT, PartnerList_of<ParentT>>;

	public:		// [SUBTYPES]
		using	CHILD_TYPES			= dpl::TypeList<ChildTn...>;

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		using	Invoke				= std::function<void(ChildT&)>;

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		using	InvokeConst			= std::function<void(const ChildT&)>;

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		using	InvokeUntil			= std::function<bool(ChildT&)>;

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		using	InvokeConstUntil	= std::function<bool(const ChildT&)>;

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		using	ParentBase_of		= ParentBase_t<ParentT, ChildT>;

	public:		// [FRIENDS]
		friend	EntityManager;
		friend	EntityPack_of<ParentT>;

		template<typename, typename, RelationType>
		friend	class ChildBase;

	protected:	// [LIFECYCLE]
		CLASS_CTOR				Parent(						const Origin&					ORIGIN)
			: MyPartnerBase(ORIGIN)
		{

		}

		CLASS_CTOR				Parent(						Parent&&						other) noexcept = default;
		Parent&					operator=(					Parent&&						other) noexcept = default;

	private:	// [LIFECYCLE] (deleted)
		CLASS_CTOR				Parent(						const Parent&					OTHER) = delete;
		Parent&					operator=(					const Parent&					OTHER) = delete;

	public:		// [FUNCTIONS]
		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		bool					has_child() const
		{
			return ParentBase_of<ChildT>::has_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		bool					can_have_another_child() const
		{
			return ParentBase_of<ChildT>::can_have_another_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		uint32_t				numChildren() const
		{
			return ParentBase_of<ChildT>::numChildren();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		ChildT&					get_child()
		{
			return ParentBase_of<ChildT>::get_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		const ChildT&			get_child() const
		{
			return ParentBase_of<ChildT>::get_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		ChildT*					first_child()
		{
			return ParentBase_of<ChildT>::first_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		const ChildT*			first_child() const
		{
			return ParentBase_of<ChildT>::first_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		ChildT*					last_child()
		{
			return ParentBase_of<ChildT>::last_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		const ChildT*			last_child() const
		{
			return ParentBase_of<ChildT>::last_child();
		}

		template<is_one_of_base_types<CHILD_TYPES>	ChildT>
		ChildT*					previous_child(				ChildT&							child)
		{
			return ParentBase_of<Base_in_list<ChildT, CHILD_TYPES>>::previous_child(child);
		}

		template<is_one_of_base_types<CHILD_TYPES>	ChildT>
		const ChildT*			previous_child(				const ChildT&					CHILD) const
		{
			return ParentBase_of<Base_in_list<ChildT, CHILD_TYPES>>::previous_child(CHILD);
		}

		template<is_one_of_base_types<CHILD_TYPES>	ChildT>
		ChildT*					next_child(					ChildT&							child)
		{
			return ParentBase_of<Base_in_list<ChildT, CHILD_TYPES>>::next_child(child);
		}

		template<is_one_of_base_types<CHILD_TYPES>	ChildT>
		const ChildT*			next_child(					const ChildT&					CHILD) const
		{
			return ParentBase_of<Base_in_list<ChildT, CHILD_TYPES>>::next_child(CHILD);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		uint32_t				for_each_child(				const Invoke<ChildT>&			INVOKE_CHILD)
		{
			return ParentBase_of<ChildT>::for_each_child(INVOKE_CHILD);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		uint32_t				for_each_child(				const InvokeConst<ChildT>&		INVOKE_CHILD) const
		{
			return ParentBase_of<ChildT>::for_each_child(INVOKE_CHILD);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		uint32_t				for_each_child_until(		const InvokeUntil<ChildT>&		INVOKE_CHILD)
		{
			return ParentBase_of<ChildT>::for_each_child_until(INVOKE_CHILD);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		uint32_t				for_each_child_until(		const InvokeConstUntil<ChildT>&	INVOKE_CHILD) const
		{
			return ParentBase_of<ChildT>::for_each_child_until(INVOKE_CHILD);
		}

	protected:	// [FUNCTIONS]
		template<is_one_of_base_types<CHILD_TYPES>	ChildT>
		bool					add_child(					ChildT&							child)
		{
			return ParentBase_of<Base_in_list<ChildT, CHILD_TYPES>>::add_child(child);
		}

		template<is_one_of_base_types<CHILD_TYPES>	ChildT>
		bool					remove_child(				ChildT&							child)
		{
			return ParentBase_of<Base_in_list<ChildT, CHILD_TYPES>>::remove_child(child);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		bool					remove_children_of_type()
		{
			return ParentBase_of<ChildT>::remove_all_children();
		}

		template<dpl::is_same_as<ParentT>		This = ParentT>
		void					remove_all_children_of_this()
		{
			(ParentBase_of<ChildTn>::remove_all_children(), ...);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		void					destroy_children_of_type()
		{
			ParentBase_of<ChildT>::destroy_all_children();
		}

		template<dpl::is_same_as<ParentT>		This = ParentT>
		void					save_children_of_this(		BinaryState&					state) const
		{
			(ParentBase_of<ChildTn>::save_children(state), ...);
		}

		template<dpl::is_same_as<ParentT>		This = ParentT>
		void					load_children_of_this(		BinaryState&					state)
		{
			(ParentBase_of<ChildTn>::load_children(state), ...);
		}

	private:	// [FUNCTIONS]
		void					save_relation_hierarchy(	BinaryState&					state) const
		{
			MyPartnerBase::save_relation_hierarchy(state);
			Parent::save_children_of_this(state);
		}

		void					load_relation_hierarchy(	BinaryState&					state)
		{
			MyPartnerBase::load_relation_hierarchy(state);
			Parent::load_children_of_this(state);
		}
	};


	// Specialization of parent without children.
	template<typename ParentT>
	class	Parent<ParentT, dpl::TypeList<>> : public Partner<ParentT, PartnerList_of<ParentT>>
	{
	private:	// [SUBTYPES]
		using	MyPartnerBase	= Partner<ParentT, PartnerList_of<ParentT>>;

	public:		// [FRIENDS]
		friend	EntityManager;
		friend	EntityPack_of<ParentT>;

	protected:	// [LIFECYCLE]
		CLASS_CTOR				Parent(						const Origin&				ORIGIN)
			: MyPartnerBase(ORIGIN)
		{

		}

		CLASS_CTOR				Parent(						Parent&&					other) noexcept = default;
		Parent&					operator=(					Parent&&					other) noexcept = default;

	private:	// [FUNCTIONS]
		void					save_relation_hierarchy(	BinaryState&				state) const
		{
			MyPartnerBase::save_relation_hierarchy(state);
		}

		void					load_relation_hierarchy(	BinaryState&				state)
		{
			MyPartnerBase::load_relation_hierarchy(state);
		}
	};
}

// maybe composite			(internal)
namespace dpl
{
	template<typename EntityT>
	concept is_Composite = (AllComponentTypes_of<EntityT>::SIZE > 0);


	template<typename EntityT, is_ComponentTypeList COMPONENT_TYPES>
	class	MaybeComposite;


	template<typename EntityT>
	class	MaybeComposite<EntityT, dpl::TypeList<>> : public Parent<EntityT, ChildList_of<EntityT>>
	{
	public: // subtypes
		using	MyRelations	= Parent<EntityT, ChildList_of<EntityT>>;

	protected: // inherited members
		using	MyRelations::MyRelations;

	protected: // lifecycle
		CLASS_CTOR		MaybeComposite(	const MaybeComposite&	OTHER) = delete;
		CLASS_CTOR		MaybeComposite(	MaybeComposite&&		other) noexcept = default;
		MaybeComposite&	operator=(		const MaybeComposite&	OTHER) = delete;
		MaybeComposite&	operator=(		MaybeComposite&&		other) noexcept = default;
	};


	template<typename EntityT, is_Component... Ts>
	class	MaybeComposite<EntityT, dpl::TypeList<Ts...>> : public Parent<EntityT, ChildList_of<EntityT>>
	{
	public: // subtypes
		using	MyRelations			= Parent<EntityT, ChildList_of<EntityT>>;

	public: // subtypes
		using	COMPONENT_TYPES		= AllComponentTypes_of<EntityT>;
		using	Table				= ComponentTable<EntityT, COMPONENT_TYPES>;
		using	Column				= typename Table::Column;
		using	ConstColumn			= typename Table::ConstColumn;
		using	MyPack				= EntityPack_of<EntityT>;

	protected: // inherited members
		using	MyRelations::MyRelations;

	protected: // lifecycle
		CLASS_CTOR		MaybeComposite(					const MaybeComposite&	OTHER) = delete;
		CLASS_CTOR		MaybeComposite(					MaybeComposite&&		other) noexcept = default;
		MaybeComposite&	operator=(						const MaybeComposite&	OTHER) = delete;
		MaybeComposite&	operator=(						MaybeComposite&&		other) noexcept = default;

	public: // component access
		template<dpl::is_one_of<COMPONENT_TYPES> T>
		T&				get_component()
		{
			MyPack& pack = MyPack::ref();
			return *pack.row<T>().at(get_index(pack));
		}

		template<dpl::is_one_of<COMPONENT_TYPES> T>
		const T&		get_component() const
		{
			const MyPack& PACK = MyPack::ref();
			return *PACK.row<T>().at(get_index(PACK));
		}

		Column			get_all_components()
		{
			MyPack& pack = MyPack::ref();
			return pack.column(get_index(pack));
		}

		ConstColumn		get_all_components() const
		{
			const MyPack& PACK = MyPack::ref();
			return PACK.column(get_index(PACK));
		}

		void			save_components_to_binary(		BinaryState&			state) const
		{
			std::apply
			(
				[&state](const Ts*... components)
				{
					(EntityT::save_component(*components, state), ...);

				}, get_all_components()
			);
		}

		void			load_components_from_binary(	BinaryState&			state)
		{
			std::apply
			(
				[&state](Ts*... components)
				{
					(EntityT::load_component(*components, state), ...);

				}, get_all_components()
			);
		}

	private: // functions
		const uint32_t	get_index(						const MyPack&			PACK) const
		{
			if(PACK.typeID() != MyRelations::storageID()) throw dpl::GeneralException(this, __LINE__, "Components must be retrieved through final entity class.");
			return PACK.index_of(static_cast<const EntityT*>(this));
		}

		// Override to save state of the specific component.
		template<dpl::is_one_of<COMPONENT_TYPES> T>
		static void		save_component(					const T&				COMPONENT, 
														BinaryState&			state)
		{
			state.save(COMPONENT);
		}

		// Override to load state of the specific component.
		template<dpl::is_one_of<COMPONENT_TYPES> T>
		static void		load_component(					T&						component, 
														BinaryState&			state)
		{
			state.load(component);
		}
	};
}

// entity implementation	<------------------------------ FOR THE USER
namespace dpl
{
	// Specialize to describe your entity.
	/*
	* BaseType - type of the entity that this entity derives from (same type indicate no inheritance, non-entity type yelds UB)
	* ParentTypes - types of entities assigned as a "parent node"
	* ChildTypes - types of entities assigned as a "child node"
	* PartnerTypes - types of entities paired to this one (no special relation)
	* ComponentTypes - List of data types assigned to this entity
	* NOTES: 
	*	- your specialization must contain BaseType and the same type list names, any other typedef, or data will be ignored
	*	- all types specified by the base entity are used under the hood by all entity types that derive from it
	*	- all types must be unique (double check types in the base class description)
	*/
	template<typename EntityT>
	struct	Description_of
	{
		using BaseType			= EntityT;
		using ParentTypes		= dpl::TypeList<>;
		using ChildTypes		= dpl::TypeList<>;
		using PartnerTypes		= dpl::TypeList<>;
		using ComponentTypes	= dpl::TypeList<>;
	};


	// Specialize as derived from Relation (look@ RelationType).
	template<typename ParentT, typename ChildT>
	struct	Relation_between : public Relation<ONE_TO_MANY, WEAK_DEPENDENCY> {};


	/*
	// Specialize to filter out unwanted children.
	template<typename ParentT, typename ChildT>
	struct	ChildFilter
	{
		bool operator()(const ParentT& PARENT, const ChildT& CHILD) const
		{
			return true;
		}
	};
	*/


	/*
		[BASICS]:
		- Each user EntityT must be derived from Entity<EntityT>.
		- The user may specialize Description_of<EntityT> to modify internal structure of the Entity<EntityT>.

		[IDENTITY]:
		- Each entity is associated with a unique name that can be used to distinguish it from others, or to query it in the EntityPack_of<EntityT>.
		- In addition to its name, each entity has a storageID that indicates the entity pack in which it is stored (its origin).

		[RELATIONS]:
		- Specialization of Relation_between<ParentT, ChildT> further modifies internal structure and behavior of the Entity<EntityT>.
		- The derived entity inherits relations defined by its base entity.
		- The order of linked children is undefined.

		[COMPOSITION]:
		- A separate, contiguous buffer is allocated to store each type of component.
		- Linking the buffer that stores a component type to the Stream object is possible if the component type satisfies the requirements for streamable data.
		- The component types specified in the entity description are combined with those inherited from the Base.
		- To meet the requirement, non-trivially destructible components must have operator>>(std::ostream&) and operator<<(std::istream&) implemented.
	*/
	template<typename EntityT>
	class	Entity : public MaybeComposite<EntityT, AllComponentTypes_of<EntityT>>
	{
	public:		// [FRIENDS]
		template<typename>
		friend class Entity;

		friend	EntityPack_of<EntityT>;

	private:	// [SUBTYPES]
		using	MyComposition = MaybeComposite<EntityT, AllComponentTypes_of<EntityT>>;

	protected:	// [INHERITED]
		using	MyComposition::MyComposition;

	protected:	// [LIFECYCLE]	
		CLASS_CTOR				Entity(		Entity&&		other) noexcept = default;
		Entity&					operator=(	Entity&&		other) noexcept = default;

	private:	// [LIFECYCLE] (deleted)
		CLASS_CTOR				Entity(		const Entity&	OTHER) = delete;
		Entity&					operator=(	const Entity&	OTHER) = delete;

	public:		// [FUNCTIONS]
		bool					has_known_storage() const
		{
			return MyComposition::storageID() == EntityPack_of<EntityT>::ref().typeID();
		}

		// Use with caution! (TODO: hide from the user?)
		void					selfdestruct()
		{
			EntityPack* packPtr = nullptr;
			if (this->has_known_storage())
			{
				EntityPack_of<EntityT>& pack = EntityPack_of<EntityT>::ref();
				if (pack.destroy(static_cast<const EntityT&>(*this))) return;
				packPtr = &pack;
			}
			else
			{
				packPtr = EntityManager::ref().find_base_variant(MyComposition::storageID());
				if (packPtr)
				{
					if (packPtr->destroy_around(reinterpret_cast<const char*>(this))) return;
				}
				else
				{
					dpl::Logger::ref().push_error("Fail to destroy Type[??]::Name[%s] -> Could not find pack with given ID[%d]", MyComposition::name().c_str(), MyComposition::storageID());
					return;
				}
			}

			dpl::Logger::ref().push_error("Fail to destroy Type[%s]::Name[%s]", packPtr->get_entity_typeName().c_str(), MyComposition::name().c_str());
		}

	private:	// [FUNCTIONS]
		// Override to save state of the EntityT members.
		void					save_state(	BinaryState&	state) const {}

		// Override to load state of the EntityT members.
		void					load_state(	BinaryState&	state) {}

		void					save(		BinaryState&	state) const
		{
			if constexpr (has_Base<EntityT>) Entity<Base_of<EntityT>>::save(state);
			static_cast<const EntityT&>(*this).save_state(state);
		}

		void					load(		BinaryState&	state)
		{
			if constexpr (has_Base<EntityT>) Entity<Base_of<EntityT>>::load(state);
			static_cast<EntityT&>(*this).load_state(state);
		}
	};


#define DEFINE_SIMPLE_ENTITY(EntityT) class EntityT : public dpl::Entity<EntityT>{public: EntityT(const dpl::Origin& ORIGIN) : Entity(ORIGIN){}}
}

// storage implementation	(internal)
namespace dpl
{
	// CRT Pattern
	template<typename EntityT>
	class	EntityStorageNode	: private Member<EntityStorageNode<Base_of<EntityT>>, EntityStorageNode<Base_of<EntityT>>>
								, private Group<EntityStorageNode<EntityT>, EntityStorageNode<EntityT>>
	{
	private:	// [SUBTYPES]
		using	MyMemberT	= Member<EntityStorageNode<Base_of<EntityT>>, EntityStorageNode<Base_of<EntityT>>>;
		using	MyGroupT	= Group<EntityStorageNode<EntityT>, EntityStorageNode<EntityT>>;

	public:		// [FRIENDS]
		friend	MyMemberT;
		friend	MyMemberT::MySequence;
		friend	MyMemberT::MyLink;
		friend	MyMemberT::MyGroup;
		friend	MyMemberT::MyBase;
		friend	MyGroupT;
		
		template<typename>
		friend class EntityStorageNode;

		template<typename>
		friend class EntityPack_of;

	private:	// [LIFECYCLE]
		CLASS_CTOR		EntityStorageNode()
		{
			if constexpr (has_Base<EntityT>)
			{
				auto&	group = EntityPack_of<Base_of<EntityT>>::ref();
						group.add_end_member(*this);
			}
		}

	private:	// [FUNCTIONS]
		template<typename BaseEntityT>
		void			call_recursively_for(const std::function<void(EntityPackView<BaseEntityT>&)>& INVOKE)
		{
			INVOKE(EntityPackView<BaseEntityT>(static_cast<EntityPack_of<EntityT>&>(*this)));

			MyGroupT::for_each([&](EntityStorageNode<EntityT>& node)
			{
				node.call_recursively_for<BaseEntityT>(INVOKE);
			});
		}
	};


	template<typename EntityT>
	using	MaybeComponentTable	= std::conditional_t<is_Composite<EntityT>, ComponentTable<EntityT, AllComponentTypes_of<EntityT>>, Monostate_t<EntityT, 2>>;


	/*
		[DATA]:			EntityT
		[STORAGE]:		contiguous
		[ORDER]:		undefined
		[INSERTION]:	fast
		[ERASURE]:		fast
	*/
	template<typename EntityT>
	class	EntityPack_of	final	: public EntityPack
									, public dpl::Singleton<EntityPack_of<EntityT>>
									, public MaybeComponentTable<EntityT>
									, public EntityStorageNode<EntityT>
	{
	private:	// [SUBTYPES]
		using	MySingletonBase		= dpl::Singleton<EntityPack_of<EntityT>>;
		using	MyComponentTable	= MaybeComponentTable<EntityT>;
		using	MyNodeBase			= EntityStorageNode<EntityT>;

	public:		// [FRIENDS]
		friend	EntityManager;
		friend  Entity<EntityT>;
		friend	dpl::Variation<EntityManager, EntityPack>;

		template<typename>
		friend class EntityPack_of;

	public:		// [DATA]
		dpl::ReadOnly<uint32_t, EntityPack_of>	typeID;

	private:	// [DATA]
		dpl::Labeler<char>						m_labeler;
		std::vector<EntityT>					m_entities;

	public:		// [LIFECYCLE]
		CLASS_CTOR					EntityPack_of(				const Binding&										BINDING)
			: EntityPack(BINDING)
			, MySingletonBase(static_cast<EntityManager*>(BINDING.owner())->owner())
			, typeID(EntityPack::get_typeID<EntityPack_of<EntityT>>())
		{
			
		}

	public:		// [BASIC]
		void						reserve_additional_space(	const uint32_t										AMOUNT)
		{
			const uint32_t NEW_CAPACITY = m_entities.size() + AMOUNT;
			m_labeler.reserve(NEW_CAPACITY);
			m_entities.reserve(NEW_CAPACITY);
			// NOTE: Component buffers are self regulated.
		}

		EntityT&					create(						const Name&											ENTITY_NAME)
		{
			if constexpr (is_Composite<EntityT>) MyComponentTable::add_column();
			return m_entities.emplace_back(Origin(ENTITY_NAME, typeID(), m_labeler));
		}

		EntityT&					create(						const Name::Type&									NAME_TYPE,
																const std::string&									NAME)
		{
			return create(Name(NAME_TYPE, NAME));
		}

		bool						destroy_at(					uint64_t											index)
		{
			if(index >= m_entities.size()) return false;
			std::invoke([&]<typename... ChildTs>(dpl::TypeList<ChildTs...> DUMMY)
			{
				(..., std::invoke([&](Tag<ChildTs> DUMMY)
				{
					if constexpr (dpl::are_strongly_dependant<EntityT, ChildTs>())
					{
						while(ChildTs* child = m_entities[index].last_child<ChildTs>())
						{
							if constexpr (std::is_same_v<EntityT, ChildTs>)
							{
								if(index+1 == m_entities.size())
								{
									// Parent will be swapped with the child (fast erase), we have to correct the index.
									index = dpl::get_element_index(m_entities, child);
								}
							}

							static_cast<Entity<ChildTs>*>(child)->selfdestruct();
						}
					}

				}, Tag<ChildTs>()));

			}, AllChildTypes_of<EntityT>());
			
			dpl::fast_remove(m_entities, m_entities.begin() + index);
			if constexpr (is_Composite<EntityT>) MyComponentTable::remove_column((uint32_t)index);
			return true;
		}

		bool						destroy(					const EntityT&										ENTITY)
		{
			return destroy_at(dpl::get_element_index(m_entities, ENTITY));
		}

		bool						destroy(					const std::string&									NAME)
		{
			const auto* ENTITY = find(NAME);
			if(!ENTITY) return false;
			return EntityPack_of::destroy(*ENTITY);
		}

	public:		// [QUERY]
		uint32_t					size() const
		{
			return (uint32_t)m_entities.size();
		}

		uint32_t					index_of(					const EntityT*										ENTITY) const
		{
			const uint64_t INDEX64 = dpl::get_element_index(m_entities, ENTITY);
			return (INDEX64 > EntityPack::INVALID_INDEX) ? EntityPack::INVALID_INDEX : (uint32_t)INDEX64;
		}

		bool						contains(					const uint32_t										ENTITY_INDEX) const
		{
			return ENTITY_INDEX < size();
		}

		bool						contains(					const EntityT*										ENTITY) const
		{
			return EntityPack_of::contains(index_of(ENTITY));
		}

		EntityT*					find(						const std::string&									ENTITY_NAME)
		{
			EntityT* entity = static_cast<EntityT*>(m_labeler.find_entry(ENTITY_NAME));
			return EntityPack_of::contains(entity)? entity : nullptr;
		}

		const EntityT*				find(						const std::string&									ENTITY_NAME) const
		{
			const EntityT* ENTITY = static_cast<const EntityT*>(m_labeler.find_entry(ENTITY_NAME));
			return EntityPack_of::contains(ENTITY)? ENTITY : nullptr;
		}

		EntityT*					find(						const uint32_t										ENTITY_INDEX)
		{
			return m_entities.data() + ENTITY_INDEX;
		}
			
		const EntityT*				find(						const uint32_t										ENTITY_INDEX) const
		{
			return m_entities.data() + ENTITY_INDEX;
		}

		EntityT&					get(						const std::string&									ENTITY_NAME)
		{
			return *find(ENTITY_NAME);
		}

		const EntityT&				get(						const std::string&									ENTITY_NAME) const
		{
			return *find(ENTITY_NAME);
		}

		EntityT&					get(						const uint32_t										ENTITY_INDEX)
		{
			return m_entities[ENTITY_INDEX];
		}

		const EntityT&				get(						const uint32_t										ENTITY_INDEX) const
		{
			return m_entities[ENTITY_INDEX];
		}

		EntityT&					last()
		{
			return m_entities.back();
		}

		const EntityT&				last() const
		{
			return m_entities.back();
		}

		uint32_t					guess_ID_from_byte(			const char*											ENTITY_MEMBER_PTR) const
		{
			static const uint64_t	STRIDE			= sizeof(EntityT);
			const char*				BEGIN			= reinterpret_cast<const char*>(m_entities.data());
			const uint64_t			TOTAL_NUM_BYTES = STRIDE * m_entities.size();
			const uint64_t			BYTE_OFFSET		= ENTITY_MEMBER_PTR - BEGIN;
			const char*				END				= BEGIN + TOTAL_NUM_BYTES;

			if(ENTITY_MEMBER_PTR < BEGIN || ENTITY_MEMBER_PTR > END) return EntityPack::INVALID_ENTITY_ID;
			return (uint32_t)(BYTE_OFFSET / STRIDE);
		}

		bool						is_inherited_ID(			const uint32_t										TYPE_ID) const
		{
			if(typeID() == TYPE_ID) return true;
			if constexpr (has_Base<EntityT>)
			{
				EntityPack_of<Base_of<EntityT>>& basePack = EntityPack_of<Base_of<EntityT>>::ref();
				return basePack.is_inherited_ID(TYPE_ID);
			}	
			return false;
		}

	public:		// [ITERATION]
		void						for_each(					const InvokeEntity<EntityT>&						INVOKE)
		{
			std::for_each(m_entities.begin(), m_entities.end(), INVOKE);
		}

		void						for_each(					const InvokeConstEntity<EntityT>&					INVOKE) const
		{
			std::for_each(m_entities.begin(), m_entities.end(), INVOKE);
		}

		void						for_each(					const InvokeIndexedEntity<EntityT>&					INVOKE)
		{
			const uint32_t SIZE = size();
			for(uint32_t index = 0; index < SIZE; ++index)
			{
				INVOKE(m_entities[index], index);
			}
		}

		void						for_each(					const InvokeConstIndexedEntity<EntityT>&			INVOKE)
		{
			const uint32_t SIZE = size();
			for(uint32_t index = 0; index < SIZE; ++index)
			{
				INVOKE(m_entities[index], index);
			}
		}

		void						for_each(					const InvokeEntityBuffer<EntityT>&					INVOKE)
		{
			INVOKE(m_entities.data(), size());
		}

		void						for_each(					const InvokeConstEntityBuffer<EntityT>&				INVOKE) const
		{
			INVOKE(m_entities.data(), size());
		}

		void						for_each(					const InvokeSimilarEntityBuffer<EntityT>&			INVOKE)
		{
			MyNodeBase::call_recursively_for<EntityT>(INVOKE);
		}

	public:		// [PARALLEL ITERATION]
		void						for_each_in_parallel(		dpl::ParallelPhase&									phase,
																const InvokeEntity<EntityT>&						INVOKE)
		{
			dpl::IndexRange<>(0, size()).for_each_split(phase.numJobs(), [&](const auto RANGE_OF_ENTITIES)
			{
				phase.add_task(RANGE_OF_ENTITIES.size(), [&, RANGE_OF_ENTITIES]()
				{
					RANGE_OF_ENTITIES.for_each([&](const uint32_t INDEX)
					{
						INVOKE(m_entities[INDEX]);
					});
				});
			});
		
			phase.start();
		}

		void						for_each_in_parallel(		dpl::ParallelPhase&									phase,
																const InvokeConstEntity<EntityT>&					INVOKE) const
		{
			dpl::IndexRange<>(0, size()).for_each_split(phase.numJobs(), [&](const auto RANGE_OF_ENTITIES)
			{
				phase.add_task(RANGE_OF_ENTITIES.size(), [&, RANGE_OF_ENTITIES]()
				{
					RANGE_OF_ENTITIES.for_each([&](const uint32_t INDEX)
					{
						INVOKE(m_entities[INDEX]);
					});
				});
			});
		
			phase.start();
		}

		void						for_each_in_parallel(		dpl::ParallelPhase&									phase,
																const InvokeIndexedEntity<EntityT>&					INVOKE)
		{
			dpl::IndexRange<>(0, size()).for_each_split(phase.numJobs(), [&](const auto RANGE_OF_ENTITIES)
			{
				phase.add_task(RANGE_OF_ENTITIES.size(), [&, RANGE_OF_ENTITIES]()
				{
					RANGE_OF_ENTITIES.for_each([&](const uint32_t INDEX)
					{
						INVOKE(m_entities[INDEX], INDEX);
					});
				});
			});
		
			phase.start();
		}

		void						for_each_in_parallel(		dpl::ParallelPhase&									phase,
																const InvokeConstIndexedEntity<EntityT>&			INVOKE)
		{
			dpl::IndexRange<>(0, size()).for_each_split(phase.numJobs(), [&](const auto RANGE_OF_ENTITIES)
			{
				phase.add_task(RANGE_OF_ENTITIES.size(), [&, RANGE_OF_ENTITIES]()
				{
					RANGE_OF_ENTITIES.for_each([&](const uint32_t INDEX)
					{
						INVOKE(m_entities[INDEX], INDEX);
					});
				});
			});
		
			phase.start();
		}

		void						for_each_in_parallel(		dpl::ParallelPhase&									phase,
																const InvokeSimilarIndexedEntityBuffer<EntityT>&	INVOKE)
		{
			EntityPack_of::for_each([&](dpl::EntityPackView<EntityT>& view)
			{
				dpl::IndexRange<>(0, view.numEntities()).for_each_split(phase.numJobs(), [&](const auto RANGE_OF_ENTITIES)
				{
					phase.add_task(RANGE_OF_ENTITIES.size(), [&, RANGE_OF_ENTITIES]()
					{
						RANGE_OF_ENTITIES.for_each([&](const uint32_t INDEX)
						{
							INVOKE(view, INDEX);
						});
					});
				});
			});
		
			phase.start();
		}
	public:		// [IO]
		void						save_entity(				const Entity<EntityT>&								ENTITY,
																BinaryState&										state) const
		{
			if(!EntityPack_of::contains(static_cast<const EntityT*>(&ENTITY))) 
				throw dpl::GeneralException(this, __LINE__, "Fail to save. Invalid entity: %s", ENTITY.name().c_str());

			ENTITY.save(state);
			if constexpr(is_Composite<EntityT>) ENTITY.save_components_to_binary(state);
			ENTITY.save_relation_hierarchy(state);
		}

		const EntityT&				save_entity(				const std::string&									ENTITY_NAME,
																BinaryState&										state) const
		{
			const EntityT& ENTITY = get(ENTITY_NAME);
			EntityPack_of::save_entity(ENTITY, state);
			return ENTITY;
		}

		void						load_entity(				Entity<EntityT>&									entity,
																BinaryState&										state)
		{
			if(!EntityPack_of::contains(static_cast<EntityT*>(&entity)))
				throw dpl::GeneralException(this, __LINE__, "Fail to load. Invalid entity: %s", entity.name().c_str());

			entity.load(state);
			if constexpr(is_Composite<EntityT>) entity.load_components_from_binary(state);
			entity.load_relation_hierarchy(state);
		}

		EntityT&					load_entity(				const std::string&									ENTITY_NAME,
																BinaryState&										state)
		{
			EntityT& entity = get(ENTITY_NAME);
			EntityPack_of::load_entity(entity, state);
			return entity;
		}

	public:		// [IMPLEMENTATION]
		virtual const std::string&	get_entity_typeName() const final override
		{
			static const std::string TYPE_NAME = dpl::undecorate_type_name<EntityT>();
			return TYPE_NAME;
		}

		virtual const Identity*		find_identity(				const std::string&									ENTITY_NAME) const final override
		{
			return find(ENTITY_NAME);
		}

		virtual const Identity*		find_and_verify_identity(	const std::string&									ENTITY_NAME,
																const uint32_t										TYPE_ID) const final override
		{
			if(!is_inherited_ID(TYPE_ID)) return nullptr;
			return find(ENTITY_NAME);
		}

		virtual const Identity&		guess_identity_from_byte(	const char*											ENTITY_MEMBER_BYTE_PTR) const final override
		{
			const uint32_t ENTITY_ID = guess_ID_from_byte(ENTITY_MEMBER_BYTE_PTR);
			return (ENTITY_ID != EntityPack::INVALID_ENTITY_ID)? get(ENTITY_ID) : EntityManager::ref().false_identity();
		}

		virtual void				for_each_dependent_child(	const std::string&									ENTITY_NAME,
																const Dependency									DEPENDENCY,
																const InvokeIdentity&								INVOKE) const final override
		{
			const EntityT& ENTITY = get(ENTITY_NAME);

			std::invoke([&]<typename... ChildTs>(dpl::TypeList<ChildTs...> DUMMY)
			{
				(..., std::invoke([&](Tag<ChildTs> DUMMY)
				{
					if(dpl::get_dependency_between<EntityT, ChildTs>() == DEPENDENCY)
					{
						ENTITY.for_each_child<ChildTs>([&](const ChildTs& CHILD)
						{
							INVOKE(CHILD);
						});
					}

				}, Tag<ChildTs>()));

			}, AllChildTypes_of<EntityT>());
		}

	private:	// [IMPLEMENTATION]
		virtual bool				destroy_around(				const char*											ENTITY_MEMBER_BYTE_PTR) final override
		{
			const uint32_t ENTITY_ID = guess_ID_from_byte(ENTITY_MEMBER_BYTE_PTR);
			return (ENTITY_ID != EntityPack::INVALID_ENTITY_ID)? destroy_at(ENTITY_ID) : false;
		}

		virtual char*				get_entity_as_bytes(		const std::string&									ENTITY_NAME) final override
		{
			Entity<EntityT>* entity = find(ENTITY_NAME);
			return entity? reinterpret_cast<char*>(entity) : nullptr;
		}

		virtual char*				get_base_entity_as_bytes(	const std::string&									ENTITY_NAME) final override
		{
			RootBase_of<EntityT>* entity = find(ENTITY_NAME);
			return entity? reinterpret_cast<char*>(entity) : nullptr;
		}

		virtual void				destroy_all_entities() final override
		{
			m_entities.clear();
		}

		virtual void				save_and_destroy(			const std::string&									ENTITY_NAME,
																BinaryState&										state) final override
		{
			EntityPack_of::destroy(save_entity(ENTITY_NAME, state));
		}

		virtual void				create_and_load(			const std::string&									ENTITY_NAME,
																BinaryState&										state) final override
		{
			EntityPack_of::load_entity(create(Name::UNIQUE, ENTITY_NAME), state);
		}
	};


	template<typename EntityT>
	class	EntityPackView
	{
	private:	// [SUBTYPES]
		using	ComponentTypes	= AllComponentTypes_of<EntityT>;
		using	ComponentArrays	= typename ComponentTypes::PtrPack;

	public:		// [FRIENDS]
		template<typename>
		friend class EntityStorageNode;

		template<typename>
		friend class EntityPack_of;

	private:	// [DATA]
		uint8_t*							rawEntityBuffer;
		uint64_t							stride;
		ComponentArrays						componentArrays;

	public:		// [DATA]
		ReadOnly<uint32_t, EntityPackView>	numEntities;

	private:		// [LIFECYCLE]
		template<typename DerivedEntityT>
		CLASS_CTOR		EntityPackView(			EntityPack_of<DerivedEntityT>&	pack)
			: rawEntityBuffer(reinterpret_cast<uint8_t*>(pack.find(0)) + dpl::base_offset<EntityT, DerivedEntityT>())
			, stride(sizeof(DerivedEntityT))
			, numEntities(pack.size())
		{
			if constexpr (ComponentTypes::SIZE > 0)
			{
				auto set_address = [&]<typename T>(T*& address)
				{
					address = pack.row<T>().modify();
				};

				std::invoke([&]<typename... ComponentTs>(std::tuple<ComponentTs*...>& components)
				{
					(..., set_address(std::get<ComponentTs*>(components)));

				}, componentArrays);
			}
		}

	public:		// [FUNCTIONS]
		EntityT&		entity_at(				const uint32_t					INDEX)
		{
			throw_if_invalid_index(INDEX);
			return *reinterpret_cast<EntityT*>(rawEntityBuffer + get_offset(INDEX));
		}

		const EntityT&	entity_at(				const uint32_t					INDEX) const
		{
			throw_if_invalid_index(INDEX);
			return *reinterpret_cast<const EntityT*>(rawEntityBuffer + get_offset(INDEX));
		}

		template<dpl::is_one_of<ComponentTypes> T>
		T&				component_at(			const uint32_t					INDEX)
		{
			throw_if_invalid_index(INDEX);
			return std::get<T*>(componentArrays)[INDEX];
		}

		template<dpl::is_one_of<ComponentTypes> T>
		const T&		component_at(			const uint32_t					INDEX) const
		{
			throw_if_invalid_index(INDEX);
			return std::get<T*>(componentArrays)[INDEX];
		}

	private:	// [FUNCTIONS]
		uint64_t		get_offset(				const uint32_t					INDEX) const
		{
			return INDEX * stride;
		}

		void			throw_if_invalid_index(	const uint32_t					INDEX) const
		{
#ifdef _DEBUG
			if(INDEX >= numEntities())
				throw GeneralException(this, __LINE__, "Index out of range");
#endif // _DEBUG
		}
	};
}

// commands					(internal)
namespace dpl
{
	template<typename T>
	class	EntityManager::CommandGroupOf : public dpl::BinaryCommand
	{
	private:	// [DATA]
		std::vector<T>	m_commands;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CommandGroupOf(		const Initializer&		INIT)
			: BinaryCommand(INIT)
		{

		}

	public:		// [LIFECYCLE]
		template<typename... CTOR>
		void			add_command(		const Initializer&		INIT,
											CTOR&&...				args)
		{
			if(was_executed()) throw InvalidCommand(this, __LINE__, "Invalid use");
			m_commands.emplace_back(INIT, std::forward<CTOR>(args)...);
		}

	private:	// [IMPLEMENTATION]
		virtual void	on_execute(			BinaryState&			state) final override
		{
			for(auto it = m_commands.begin(); it != m_commands.end(); ++it)
			{
				BinaryCommand&	command = *it;
								command.execute(state);
			}
		}

		virtual void	on_unexecute(		BinaryState&			state) final override
		{
			for(auto it = m_commands.rbegin(); it != m_commands.rend(); ++it)
			{
				BinaryCommand&	command = *it;
								command.unexecute(state);
			}
		}
	};


	template<typename EntityT, dpl::is_TypeList RELATED_TYPES, template<typename, typename> class RelationCommand>
	class	EntityManager::MultiTypeCommand : public dpl::BinaryCommand
	{
	private:	// [SUBTYPES]
		template<is_Entity RelatedT>
		using	Subcommand	= RelationCommand<EntityT, RelatedT>;

		using	AllCommandTypes	= RELATED_TYPES::template encapsulate_in<Subcommand>;
		using	CommandTuple	= typename AllCommandTypes::DataPack_r;

	private:	// [DATA]
		CommandTuple m_cmdTuple;

	private:	// [LIFECYCLE]
		template<	typename... CommandTs,
					typename... Args>
		CLASS_CTOR		MultiTypeCommand(	const dpl::TypeList<CommandTs...>	DUMMY,
											const Initializer&					INIT,
											std::tuple<Args&...>				args)
			: BinaryCommand(INIT)
			, m_cmdTuple(std::piecewise_construct, (sizeof(CommandTs), args)...)
		{

		}

	public:		// [LIFECYCLE]
		template<typename... CTOR>
		CLASS_CTOR		MultiTypeCommand(	const Initializer&					INIT,
											CTOR&&...							args)
			: MultiTypeCommand(AllCommandTypes(), INIT, std::tie(args...))
		{

		}

	private:	// [IMPLEMENTATION]
		virtual void	on_execute(			BinaryState&						state) final override
		{
			std::invoke([&state]<typename... CommandTs>(std::tuple<CommandTs...>&	commands)
			{
				(..., ((BinaryCommand&)std::get<CommandTs>(commands)).execute(state));

			}, m_cmdTuple);
		}

		virtual void	on_unexecute(		BinaryState&						state) final override
		{
			std::invoke([&state]<typename... CommandTs>(std::tuple<CommandTs...>& commands)
			{
				(((BinaryCommand&)std::get<CommandTs>(commands)).unexecute(state), ...);

			}, m_cmdTuple);
		}
	};


	template<is_Entity T>
	class	EntityManager::CMD_Create : public dpl::BinaryCommand
	{
	private:	// [DATA]
		Name m_name;
		bool m_initialized;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_Create(		const Initializer&		INIT,
										const Name				NAME)
			: BinaryCommand(INIT)
			, m_name(NAME)
			, m_initialized(false)
		{

		}

		CLASS_CTOR		CMD_Create(		const Initializer&		INIT,
										const Name::Type		TYPE,
										const std::string&		STR)
			: CMD_Create(INIT, Name(TYPE, STR))
		{

		}

		CLASS_CTOR		CMD_Create(		const Initializer&		INIT,
										const Name::Type		TYPE,
										const std::string_view	STR)
			: CMD_Create(INIT, Name(TYPE, STR))
		{

		}

	private:	// [IMPLEMENTATION]
		virtual void	on_execute(		BinaryState&			state) final override
		{
			EntityPack_of<T>&	pack = EntityPack_of<T>::ref();
			if(m_initialized)
			{
				pack.create_and_load(m_name, state);
			}
			else // first creation of the entity
			{
				const Identity&	ENTITY	= pack.create(m_name);
				pack.save_entity(ENTITY, state); //<-- We have to initialize binary space for the entity.
				m_initialized = true;

				if(m_name.is_generic())
					m_name.change(Name::UNIQUE, ENTITY.name());
			}
		}

		virtual void	on_unexecute(	BinaryState&			state) final override
		{
			EntityPack_of<T>&	pack = EntityPack_of<T>::ref();
								pack.save_and_destroy(m_name, state);
		}
	};


	template<is_Entity T>
	class	EntityManager::CMD_CreateGroupOf : public EntityManager::CommandGroupOf<EntityManager::CMD_Create<T>>
	{
	public:		// [SUBTYPES]
		using	MyCommand	= EntityManager::CMD_Create<T>;
		using	MyCommands	= EntityManager::CommandGroupOf<MyCommand>;
		using	Initializer = typename EntityManager::CommandGroupOf<EntityManager::CMD_Create<T>>::Initializer;

	private:	// [DATA]
		std::string m_prefix;
		uint32_t	m_size;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_CreateGroupOf(	const Initializer&	INIT,
											const std::string&	GROUP_PREFIX,
											const uint32_t		GROUP_SIZE)
			: MyCommands(INIT)
			, m_prefix(GROUP_PREFIX)
			, m_size(GROUP_SIZE)
		{
			if(GROUP_PREFIX.size() < 3)
				throw InvalidCommand("Group prefix must have at least 3 characters.");

			if(GROUP_SIZE == 0)
				throw InvalidCommand("Group size must be non-zero.");
		}

	private:	// [IMPLEMENTATION]
		virtual void	on_first_execution(	BinaryState&		state) final override
		{
			for(uint32_t index = 0; index < m_size; ++index)
			{
				MyCommands::add_command(Initializer(state), Name::GENERIC, m_prefix);
			}
		}
	};


	class	EntityManager::CMD_Destroy : public dpl::BinaryCommand
	{
	public:		// [DATA]
		ReadOnly<Reference, CMD_Destroy> reference;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_Destroy(	const Initializer&	INIT,
										const Identity&		IDENTITY)
			: BinaryCommand(INIT)
			, reference(IDENTITY)
		{
			
		}

	private:	// [IMPLEMENTATION]
		virtual void	on_execute(		BinaryState&		state) final override
		{
			EntityPack& pack = EntityManager::ref().get_base_variant(reference().storageID());
			pack.save_and_destroy(reference().name(), state);
		}

		virtual void	on_unexecute(	BinaryState&		state) final override
		{
			EntityPack& pack = EntityManager::ref().get_base_variant(reference().storageID());
			pack.create_and_load(reference().name(), state);
		}
	};


	template<is_Entity ParentT, is_Entity ChildT>
	class	EntityManager::CMD_DestroyChildrenIf : public EntityManager::CommandGroupOf<EntityManager::CMD_Destroy>
	{
	public:		// [SUBTYPES]
		using MyCommand		= EntityManager::CMD_Destroy;
		using MyCommands	= EntityManager::CommandGroupOf<MyCommand>;

	private:	// [DATA]
		Reference m_ref;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_DestroyChildrenIf(	const Initializer&		INIT,
												const Entity<ParentT>&	PARENT)
			: MyCommands(INIT, PARENT.numChildren<ChildT>())
			, m_ref(PARENT)
		{
			if(!PARENT.numChildren<ChildT>())
				throw InvalidCommand(__LINE__, "No children of the given type: %s", EntityPack_of<ChildT>::ref().get_entity_typeName().c_str());
		}

	private:	// [IMPLEMENTATION]
		virtual void	on_first_execution(		BinaryState&			state) final override
		{
			EntityPack&			pack	= EntityManager::ref().get_base_variant(m_ref.storageID());
			Entity<ParentT>&	entity	= static_cast<Entity<ParentT>&>(pack.get_identity(m_ref.name()));

			entity.for_each_child<ChildT>([&](ChildT& child)
			{
				MyCommands::add_command(Initializer(state), child);
			});
		}
	};

	
	template<is_Entity ParentT>
	class	EntityManager::CMD_DestroyAllChildrenOf : public EntityManager::MultiTypeCommand<ParentT, AllChildTypes_of<ParentT>, EntityManager::CMD_DestroyChildrenIf>
	{
	private:	// [SUBTYPES]
		using	MyBase		= EntityManager::MultiTypeCommand<ParentT, AllChildTypes_of<ParentT>, EntityManager::CMD_DestroyChildrenIf>;
		using	Initializer = typename MyBase::Initializer;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_DestroyAllChildrenOf(	const Initializer&		INIT,
													const Entity<ParentT>&	ENTITY)
			: MyBase(INIT, ENTITY)
		{
			
		}
	};


	class	EntityManager::CMD_Rename : public dpl::BinaryCommand
	{
	private:	// [DATA]
		Reference	m_ref;
		Name		m_newName;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_Rename(		const Initializer&		INIT,
										const Identity&			ENTITY,
										const Name				NEW_NAME)
			: BinaryCommand(INIT)
			, m_ref(ENTITY)
			, m_newName(NEW_NAME)
		{

		}

		CLASS_CTOR		CMD_Rename(		const Initializer&		INIT,
										const Identity&			ENTITY,
										const Name::Type		TYPE,
										const std::string&		STR)
			: CMD_Rename(INIT, ENTITY, Name(TYPE, STR))
		{

		}

		CLASS_CTOR		CMD_Rename(		const Initializer&		INIT,
										const Identity&			ENTITY,
										const Name::Type		TYPE,
										const std::string_view	STR)
			: CMD_Rename(INIT, ENTITY, Name(TYPE, STR))
		{

		}

	private:	// [IMPLEMENTATION]
		virtual void	on_execute(		BinaryState&			state) final override
		{
			EntityPack& pack		= EntityManager::ref().get_base_variant(m_ref.storageID());
			Identity&	identity	= pack.get_identity(m_ref.name());
						identity.set_name(m_newName);

			if(m_newName.is_generic())
			{
				m_newName.change(Name::UNIQUE, identity.name());
			}
		}

		virtual void	on_unexecute(	BinaryState &			state) final override
		{
			EntityPack& pack		= EntityManager::ref().get_base_variant(m_ref.storageID());
			Identity&	identity	= pack.get_identity(m_newName);
						identity.set_name(Name::UNIQUE, m_ref.name());
		}
	};


	template<is_Entity ParentT, one_of_child_types_of<ParentT> ChildT>
	class	EntityManager::CMD_Adopt : public dpl::BinaryCommand
	{
	private:	// [DATA]
		Reference m_parentRef;
		Reference m_childRef;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_Adopt(		const Initializer&		INIT,
										const Entity<ParentT>&	PARENT,
										const Entity<ChildT>&	CHILD)
			: BinaryCommand(INIT)
			, m_parentRef(PARENT)
			, m_childRef(CHILD)
		{
			if(!PARENT.can_have_another_child<ChildT>()) 
				throw InvalidCommand("%s has maximum number of children.", PARENT.name().c_str());

			if(CHILD.has_parent<ParentT>())
				throw InvalidCommand("%s already has a parent. Child must first be orphaned.", CHILD.name().c_str());
		}

	private:	// [IMPLEMENTATION]
		virtual void	on_execute(		BinaryState&			state) final override
		{
			m_parentRef.get<ParentT>().add_child(m_childRef.get<ChildT>());
		}

		virtual void	on_unexecute(	BinaryState&			state) final override
		{
			m_parentRef.get<ParentT>().remove_child(m_childRef.get<ChildT>());
		}
	};


	template<is_Entity ParentT, one_of_child_types_of<ParentT> ChildT>
	class	EntityManager::CMD_Orphan : public dpl::BinaryCommand
	{
	private:	// [DATA]
		Reference m_parentRef;
		Reference m_childRef;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_Orphan(		const Initializer&		INIT,
										const Entity<ChildT>&	CHILD)
			: BinaryCommand(INIT)
			, m_childRef(CHILD)
		{
			if(!CHILD.has_parent<ParentT>()) 
				throw InvalidCommand("%s does not have a parent of the given type", CHILD.name().c_str());

			m_parentRef.reset(&CHILD.get_parent<ParentT>());
		}

	private:	// [IMPLEMENTATION]
		virtual void	on_execute(		BinaryState&			state) final override
		{
			m_parentRef.get<ParentT>().remove_child(m_childRef.get<ChildT>());
		}

		virtual void	on_unexecute(	BinaryState&			state) final override
		{
			m_parentRef.get<ParentT>().add_child(m_childRef.get<ChildT>());
		}
	};


	template<is_Entity ParentT, one_of_child_types_of<ParentT> ChildT>
	class	EntityManager::CMD_OrphanChildrenIf : public EntityManager::CommandGroupOf<EntityManager::CMD_Orphan<ParentT, ChildT>>
	{
	public:		// [SUBTYPES]
		using	MyCommand	= EntityManager::CMD_Orphan<ParentT, ChildT>;
		using	MyCommands	= EntityManager::CommandGroupOf<MyCommand>;
		using	Initializer = typename EntityManager::CommandGroupOf<EntityManager::CMD_Orphan<ParentT, ChildT>>::Initializer;

	private:	// [DATA]
		Reference m_ref;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_OrphanChildrenIf(	const Initializer&		INIT,
												const Entity<ParentT>&	PARENT)
			: MyCommands(INIT)
			, m_ref(PARENT)
		{
			if(!PARENT.numChildren<ChildT>())
				throw InvalidCommand(__LINE__, "No children of the given type: ", EntityPack_of<ChildT>::ref().get_entity_typeName().c_str());
		}

	private:	// [IMPLEMENTATION]
		virtual void	on_first_execution(		BinaryState&			state) final override
		{
			EntityPack&			pack	= EntityManager::ref().get_base_variant(m_ref.storageID());
			Entity<ParentT>&	entity	= static_cast<Entity<ParentT>&>(pack.get_identity(m_ref.name()));

			entity.for_each_child<ChildT>([&](ChildT& child)
			{
				MyCommands::add_command(Initializer(state), child);
			});
		}
	};


	template<is_FinalEntity ParentT>
	class	EntityManager::CMD_OrphanAllChildrenOf : public EntityManager::MultiTypeCommand<ParentT, AllChildTypes_of<ParentT>, EntityManager::CMD_OrphanChildrenIf>
	{
	private:	// [SUBTYPES]
		using	MyBase		= EntityManager::MultiTypeCommand<ParentT, AllChildTypes_of<ParentT>, EntityManager::CMD_OrphanChildrenIf>;
		using	Initializer = typename MyBase::Initializer;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_OrphanAllChildrenOf(	const Initializer&		INIT,
													const Entity<ParentT>&	ENTITY)
			: MyBase(INIT, ENTITY)
		{

		}
	};


	template<is_Entity EntityT, one_of_partner_types_of<EntityT> PartnerT>
	class	EntityManager::CMD_Involve : public dpl::BinaryCommand
	{
	private:	// [DATA]
		Reference m_entityRef;
		Reference m_partnerRef;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_Involve(		const Initializer&		INIT,
											const Entity<EntityT>&	ENTITY,
											const Entity<PartnerT>&	PARTNER)
			: BinaryCommand(INIT)
			, m_entityRef(ENTITY)
			, m_partnerRef(PARTNER)
		{
			CMD_Involve::validate_partner<PartnerT>(ENTITY);
			CMD_Involve::validate_partner<EntityT>(PARTNER);
		}

	private:	// [IMPLEMENTATION]
		template<typename OtherT, typename T>
		void			validate_partner(	const Entity<T>&		ENTITY) const
		{
			if(ENTITY.has_partner<OtherT>()) 
				throw InvalidCommand("%s is already involved with %s", ENTITY.name().c_str(), ENTITY.get_partner<OtherT>().name().c_str());
		}

		virtual void	on_execute(			BinaryState&			state) final override
		{
			m_entityRef.get<EntityT>().set_partner(m_partnerRef.get<PartnerT>());
		}

		virtual void	on_unexecute(		BinaryState&			state) final override
		{
			m_entityRef.get<EntityT>().remove_partner(m_partnerRef.get<PartnerT>());
		}
	};


	template<is_Entity EntityT, one_of_partner_types_of<EntityT> PartnerT>
	class	EntityManager::CMD_Disinvolve : public dpl::BinaryCommand
	{
	private:	// [DATA]
		Reference m_entityRef;
		Reference m_partnerRef;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_Disinvolve(	const Initializer&		INIT,
										const Entity<EntityT>&	ENTITY)
			: BinaryCommand(INIT)
			, m_entityRef(ENTITY)
		{
			if(!ENTITY.has_partner<PartnerT>()) 
				throw InvalidCommand("%s is not involved with %s", ENTITY.name().c_str(), EntityPack_of<PartnerT>::ref().get_entity_typeName().c_str());

			m_partnerRef.reset(&ENTITY.get_partner<PartnerT>());
		}

	private:	// [IMPLEMENTATION]
		virtual void	on_execute(		BinaryState&			state) final override
		{
			m_entityRef.get<EntityT>().remove_partner(m_partnerRef.get<PartnerT>());
		}

		virtual void	on_unexecute(	BinaryState&			state) final override
		{
			m_entityRef.get<EntityT>().set_partner(m_partnerRef.get<PartnerT>());
		}
	};


	template<is_Entity EntityT>
	class	EntityManager::CMD_DisinvolveAll : public EntityManager::MultiTypeCommand<EntityT, AllPartnerTypes_of<EntityT>, EntityManager::CMD_Disinvolve>
	{
	private:	// [SUBTYPES]
		using	MyBase		= EntityManager::MultiTypeCommand<EntityT, AllPartnerTypes_of<EntityT>, EntityManager::CMD_Disinvolve>;
		using	Initializer = typename MyBase::Initializer;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_DisinvolveAll(	const Initializer&		INIT,
											const Entity<EntityT>&	ENTITY)
			: MyBase(INIT, ENTITY)
		{

		}
	};


	class	EntityPack::CMD_DestroyHierarchy : public BinaryCommand
	{
	private:	// [DATA]
		EntityManager::CMD_Destroy							m_root;
		EntityManager::CommandGroupOf<CMD_DestroyHierarchy>	m_branches;

	public:		// [LIFECYCLE]
		CLASS_CTOR		CMD_DestroyHierarchy(	const Initializer&	INIT,
												const Identity&		ROOT_ENTITY)
			: BinaryCommand(INIT)
			, m_root(INIT, ROOT_ENTITY)
			, m_branches(INIT)
		{
			
		}

	private:	// [IMPLEMENTATION]
		virtual void	on_first_execution(		BinaryState&		state) final override
		{
			EntityPack& pack = EntityManager::ref().get_base_variant(m_root.reference().storageID());
			pack.for_each_dependent_child(m_root.reference().name(), STRONG_DEPENDENCY, [&](const Identity& CHILD_IDENTITY)
			{
				m_branches.add_command(BinaryCommand::Initializer(state), CHILD_IDENTITY);
			});
		}

		virtual void	on_execute(				BinaryState&		state) final override
		{
			m_root.execute(state);
			m_branches.execute(state);
		}

		virtual void	on_unexecute(			BinaryState&		state) final override
		{
			m_branches.unexecute(state);
			m_root.unexecute(state);
		}
	};


	inline void	EntityManager::cmd_destroy_hierarchy(	const Identity&					ENTITY)
	{
		m_invoker.invoke<EntityPack::CMD_DestroyHierarchy>(ENTITY);
	}

	inline void	EntityManager::cmd_destroy(				const Identity&					ENTITY)
	{
		m_invoker.invoke<CMD_Destroy>(ENTITY);
	}

	inline void	EntityManager::cmd_rename(				const Identity&					ENTITY,
														const Name						NEW_NAME)
	{
		m_invoker.invoke<CMD_Rename>(ENTITY, NEW_NAME);
	}

	inline void	EntityManager::cmd_rename(				const Identity&					ENTITY,
														const Name::Type				TYPE,
														const std::string&				STR)
	{
		m_invoker.invoke<CMD_Rename>(ENTITY, TYPE, STR);
	}

	inline void	EntityManager::cmd_rename(				const Identity&					ENTITY,
														const Name::Type				TYPE,
														const std::string_view			STR)
	{
		m_invoker.invoke<CMD_Rename>(ENTITY, TYPE, STR);
	}
}

#pragma pack(pop)