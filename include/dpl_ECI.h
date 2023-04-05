#pragma once


#include <algorithm>
#include <sstream>
#include "dpl_TypeTraits.h"
#include "dpl_NamedType.h"
#include "dpl_Membership.h"
#include "dpl_Variation.h"
#include "dpl_Labelable.h"
#include "dpl_StaticHolder.h"
#include "dpl_Singleton.h"
#include "dpl_std_addons.h"
#include "dpl_Logger.h"
#include "dpl_Stream.h"


/*
	[TODO::IDENTITY]
	- each entity pack should have its own labeler

	[TODO::INSTANCING]:
	- save/load
	- attach/detach

	[TODO::COMMANDS]:
	- ???
*/

#pragma pack(push, 4)

// forward declarations		(internal)
namespace dpl
{
	class	Name;
	class	Origin;
	class	Identity;

	class	InstanceRow;
	class	InstanceTable;

	template<typename EntityT>
	class	Entity;

	class	EntityPack;

	template<typename EntityT>
	class	EntityPack_of;

	class	EntityManager;
}

// relations				(internal)
namespace dpl
{
	enum	RelationType
	{
		ONE_TO_ONE, // One parent can only have one child of the given type.
		ONE_TO_MANY // One parent can have many children of given type.
	};

	template<typename ParentT, typename ChildT, RelationType TYPE>
	class	ParentBase;

	template<typename ParentT, typename ChildT, RelationType TYPE>
	class	ChildBase;

	template<typename ParentT, dpl::is_TypeList CHILD_TYPES>
	class	Parent;

	template<typename ChildT, dpl::is_TypeList PARENT_TYPES>
	class	Child;

	template<RelationType TYPE>
	struct	Relation
	{
		static const RelationType type = TYPE;
	};
}

// Entity description		<------------------------------ FOR THE USER
namespace dpl
{
	// Specialize as derived from std::true_type to enable inheritance.
	template<typename EntityT>
	struct	Inheritance_of : public std::false_type{};

	// Specialize as derived from std::type_identity ('type' must be derived from Entity<type>).
	template<typename EntityT>
	struct	Base_of : public std::type_identity<EntityT>{}; //<-- Same type indicate no inheritance.

	// Specialize to make EntityT instantiable. Ignored if Inheritance_of<T> is true.
	template<typename EntityT>
	struct	Instance_of;

	// Specialize as derived from Relation (look@ RelationType).
	template<typename ChildT, typename ParentT>
	struct	Relation_between : public Relation<ONE_TO_MANY>{};

	// Specialize as derived from dpl::TypeList.
	template<typename EntityT>
	struct	ParentList_of : public dpl::TypeList<>{};

	// Specialize as derived from dpl::TypeList.
	template<typename EntityT>
	struct	ChildList_of : public dpl::TypeList<>{};

	// Specialize as derived from dpl::TypeList.
	template<typename EntityT>
	struct	ComponentList_of : public dpl::TypeList<>{};
}

// restrictions				(internal)
namespace dpl
{
	template<typename EntityT>
	concept is_Entity					= std::is_base_of_v<Entity<EntityT>, EntityT>; //dpl::is_base_of_template<Entity, EntityT>;

	template<typename EntityT>
	struct	IsEntity : public std::conditional_t<is_Entity<EntityT>, std::true_type, std::false_type>{};

	template<typename ENTITY_TYPES>
	concept	is_EntityTypeList			= dpl::is_TypeList<ENTITY_TYPES> 
										&& ENTITY_TYPES::ALL_UNIQUE 
										&& ENTITY_TYPES::template all<IsEntity>();

	template<uint32_t N, typename T>
	concept is_divisible_by				= sizeof(T)%N == 0;

	template<typename EntityT>
	concept is_TransferableInstance_of	= is_divisible_by<4, Instance_of<EntityT>>
										&& std::is_trivially_destructible_v<Instance_of<EntityT>>;

	template<typename EntityT>
	concept is_Inheritable				= Inheritance_of<EntityT>::value;

	template<typename EntityT>
	concept has_Base					= !std::is_same_v<typename Base_of<EntityT>::type, EntityT>
										&& is_Inheritable<typename Base_of<EntityT>::type>;
}

// queries					(internal)
namespace dpl
{
	template<typename EntityT>
	struct	RootBaseQuery
	{
	private:	using Base	= std::conditional_t<has_Base<EntityT>, typename Base_of<EntityT>::type, void>;
	public:		using Type	= std::conditional_t<has_Base<Base>, typename RootBaseQuery<Base>::Type, EntityT>;
	};

	template<>
	struct	RootBaseQuery<void>
	{
	public:		using Type	= void;
	};

	// Returns the entity type that serves as the root of the inheritance diagram to which EntityT belongs.
	template<typename EntityT>
	using	RootBase_of		= typename RootBaseQuery<EntityT>::Type;

	template<typename ParentT, typename ChildT>
	constexpr uint32_t		get_relation_ID()
	{
		constexpr uint32_t ID_OFFSET = 10000; //<-- We use this offset to not block the inheritance from dpl::Sequenceable with default ID.
		using ParentTypeList = typename ParentList_of<ChildT>::type;
		return ID_OFFSET + ParentTypeList::template index_of<ParentT>();
	}

	template<typename ParentT, typename ChildT>
	constexpr RelationType	get_relation_between()
	{
		return Relation_between<ParentT, ChildT>::type;
	}
}

// identity & origin		(internal, Entity base)
namespace dpl
{
	// Stores naming convention of the entity.
	class	Name
	{
	public: // subtypes
		enum	Type
		{
			UNIQUE, // Entity will be named with exact name given by the user.
			GENERIC // Entity name will be appended with generic postfix.
		};

	private: // data
		Type		m_type;
		std::string	m_str;

	public: // lifecycle
		CLASS_CTOR		Name(		const Type			TYPE,
									const std::string&	STR)
			: m_type(TYPE)
			, m_str(STR)
		{

		}

	public: // operators
		operator const std::string&() const
		{
			return m_str;
		}

	public: // functions
		inline void		change(		const Type			TYPE,
									const std::string&	STR)
		{
			m_type	= TYPE;
			m_str	= STR;
		}

		inline bool		has_type(	const Type			TYPE) const
		{
			return m_type == TYPE;
		}

		inline bool		is_unique() const
		{
			return has_type(UNIQUE);
		}

		inline bool		is_generic() const
		{
			return has_type(GENERIC);
		}
	};


	// Wrapps name convention of the entity and typeID of the buffer that stores it.
	class	Origin
	{
	public: // friends
		friend Identity;

		template<typename>
		friend class Entity;

		template<typename>
		friend class EntityPack_of;

	private: // subtypes
		using	Labelable_t	= dpl::Labelable<char>;
		using	Labeler_t	= dpl::Labeler<char>;

	public: // subtypes
		using	StorageID	= uint32_t;

	public: // data
		dpl::ReadOnly<Name,			Origin> name;
		dpl::ReadOnly<StorageID,	Origin> storageID;

	private: // data
		dpl::ReadOnly<Labeler_t*,	Origin> labeler;

	private: // lifecycle
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
	private: // subtypes
		using	Labelable_t	= dpl::Labelable<char>;

	public: // subtypes
		using	StorageID	= Origin::StorageID;

	public: // constants
		static const StorageID INVALID_STORAGE_ID = dpl::Variant<EntityManager, EntityPack>::INVALID_INDEX;

	public: // friends
		template<typename>
		friend	class EntityPack_of;

	public: // data
		dpl::ReadOnly<StorageID, Identity> storageID; // TypeID of the EntityPack that stores this entity.

	public: // lifecycle
		CLASS_CTOR					Identity()
			: storageID(INVALID_STORAGE_ID)
		{

		}

		CLASS_CTOR					Identity(		const Origin&		ORIGIN)
			: storageID(ORIGIN.storageID)
		{
			if(ORIGIN.name().is_unique())	ORIGIN.labeler()->label(*this, ORIGIN.name());
			else							ORIGIN.labeler()->label_with_postfix(*this, ORIGIN.name());
		}

		CLASS_CTOR					Identity(		const Identity&		OTHER) = delete;
		CLASS_CTOR					Identity(		Identity&&			other) noexcept = default;
		Identity&					operator=(		const Identity&		OTHER) = delete;
		Identity&					operator=(		Identity&&			other) noexcept = default;

	public: // functions
		inline const std::string&	name() const
		{
			return Labelable_t::get_label();
		}

		template<is_Entity T>
		inline bool					has_type() const
		{
			return EntityPack_of<T>::ref().typeID() == storageID();
		}
	};
}

// 'pack' interfaces		(internal)
namespace dpl
{
	class	EntityPack : public dpl::Variant<EntityManager, EntityPack>
	{
	public: // friends
		template<typename>
		friend class EntityPack_of;

	public:// constants
		static const uint32_t INVALID_ENTITY_ID = std::numeric_limits<uint32_t>::max();

	private: // lifecycle
		CLASS_CTOR					EntityPack(					const Binding&		BINDING)
			: Variant(BINDING)
		{

		}

	public: // interface
		virtual const Identity*		find_identity(				const std::string&	ENTITY_NAME) const = 0;

		virtual InstanceRow*		find_instance_row(			const std::string&	ENTITY_NAME) = 0;

		virtual const Identity&		guess_identity_from_byte(	const char*			ENTITY_MEMBER_BYTE_PTR) const = 0;

		virtual char*				get_entity_as_bytes(		const std::string&	ENTITY_NAME) = 0;

		virtual char*				get_base_entity_as_bytes(	const std::string&	ENTITY_NAME) = 0;

		virtual bool				is_family_line(				const uint32_t		TYPE_ID) const = 0;

	public: // functions
		template<typename MemberT>
		inline const Identity&		guess_identity(				const MemberT*		ENTITY_MEMBER) const
		{
			return guess_identity_from_byte(reinterpret_cast<const char*>(ENTITY_MEMBER));
		}
	};

	// Stores entity packs.
	class	EntityManager	: public dpl::Singleton<EntityManager>
							, public dpl::Variation<EntityManager, EntityPack>
							, private dpl::StaticHolder<Identity, EntityManager>
	{
	public: // lifecycle
		CLASS_CTOR						EntityManager(		dpl::Multition&				multition)
			: Singleton(multition)
		{

		}

	public: // functions
		static inline const Identity&	false_identity()
		{
			return StaticHolder::data;
		}

		template<is_Entity T>
		inline bool						create_EntityPack_of()
		{
			return Variation::create_variant<EntityPack_of<T>>();
		}
	};
}

// binary state				(internal)
namespace dpl
{
	struct	BinaryState
	{
		std::stringstream	binary;

		inline void reset_in()
		{
			binary.seekg(0);
			binary.clear();
		}

		inline void reset_out()
		{
			binary.str({}); //<-- Remove old state.
			binary.seekp(0);
			binary.clear();
		}
	};
}

// references				(internal, RTTI)
namespace dpl
{
	class	Reference
	{
	public: // data
		dpl::ReadOnly<Identity::StorageID,	Reference> storageID;
		dpl::ReadOnly<std::string,			Reference> name;

	public: // lifecycle
		CLASS_CTOR					Reference()
			: storageID(EntityPack::INVALID_INDEX)
		{
		}

		template<typename EntityT>
		CLASS_CTOR					Reference(				const Entity<EntityT>*	ENTITY)
			: Reference()
		{
			Reference::reset<EntityT>(ENTITY);
		}

	public: // operators
		inline bool					operator==(				const Reference&		OTHER) const
		{
			return (this->storageID() == OTHER.storageID()) && (this->name() == OTHER.name());
		}

		inline bool					operator!=(				const Reference&		OTHER) const
		{
			return !Reference::operator==(OTHER);
		}

	public: // functions
		inline bool					valid() const
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

		InstanceRow*				acquire_instance_row() const;

		template<typename EntityT>
		Entity<EntityT>*			acquire() const
		{
			if(EntityPack* pack = EntityManager::ref().find_base_variant(storageID()))
			{
				if(const Identity* IDENTITY = pack->find_identity(name()))
				{
					if(IDENTITY->has_type<EntityT>())
					{
						const auto* ENTITY = static_cast<const Entity<EntityT>*>(IDENTITY);
						return const_cast<Entity<EntityT>*>(ENTITY);
					}
				}
			}

			return nullptr;
		}

		inline void					load_from_binary(		std::istream&			stream)
		{
			dpl::import_t(stream, *storageID);
			dpl::import_dynamic_container(stream, *name);
		}

		inline void					save_to_binary(			std::ostream&			stream) const
		{
			dpl::export_t(stream, storageID());
			dpl::export_container(stream, name());
		}

		static void					save_to_binary_opt(		const Identity&			IDENTITY,
															std::ostream&			stream)
		{
			dpl::export_t(stream, IDENTITY.storageID());
			dpl::export_container(stream, IDENTITY.name());
		}

		template<typename EntityT>
		static void					save_to_binary_opt(		const Entity<EntityT>&	ENTITY,
															std::ostream&			stream)
		{
			save_to_binary_opt((const Identity&)ENTITY, stream);
		}

		template<typename EntityT>
		static void					save_to_binary_opt(		const Entity<EntityT>*	ENTITY,
															std::ostream&			stream)
		{
			save_to_binary_opt(Reference::get_identity<EntityT>(ENTITY), stream);
		}

	private: // functions
		template<typename EntityT>
		inline const Identity&		get_identity(			const Entity<EntityT>*	ENTITY) const
		{
			return ENTITY? (const Identity&)(*ENTITY) : EntityManager::ref().false_identity();
		}
	};
}

// child base				(internal)
namespace dpl
{
	template<typename ParentT, typename ChildT>
	class	ChildBase<ParentT, ChildT, RelationType::ONE_TO_ONE> 
		: private dpl::Association<ChildT, ParentT, dpl::get_relation_ID<ParentT, ChildT>()>
	{
	private: // subtypes
		static const uint32_t RELATION_ID	= dpl::get_relation_ID<ParentT, ChildT>();
		using	ParentBaseT					= ParentBase<ParentT, ChildT, RelationType::ONE_TO_ONE>;
		using	MyBaseT						= dpl::Association<ChildT, ParentT, RELATION_ID>;
		using	MyLinkT						= dpl::Association<ParentT, ChildT, RELATION_ID>;

	public: // friends
		friend	ParentBaseT;
		friend	MyBaseT;
		friend	MyLinkT;

		template<typename, dpl::is_TypeList>
		friend class	Parent;

		template<typename, dpl::is_TypeList>
		friend class	Child;

	protected: // lifecycle
		CLASS_CTOR				ChildBase() = default;
		CLASS_CTOR				ChildBase(						ChildBase&&				other) noexcept = default;
		ChildBase&				operator=(						ChildBase&&				other) noexcept = default;

		ChildBase&				operator=(						dpl::Swap<ChildBase>	other)
		{
			MyBaseT::operator=(dpl::Swap<MyBaseT>(*other));
			return *this;
		}

	private: // lifecycle (deleted)
		CLASS_CTOR				ChildBase(						const ChildBase&		OTHER) = delete;
		ChildBase&				operator=(						const ChildBase&		OTHER) = delete;

	private: // functions
		inline bool				has_parent() const
		{
			return MyBaseT::is_linked();
		}

		inline ParentT&			get_parent()
		{
			return *MyBaseT::other();
		}

		inline const ParentT&	get_parent() const
		{
			return *MyBaseT::other();
		}

		void					import_relation_from_binary(	std::istream&			stream)
		{
			MyBaseT::unlink(); //<-- Detach from previous parent.
			const Reference REFERENCE(stream);
			if(ParentBaseT* parent = REFERENCE.acquire<ParentT>())
			{
				MyBaseT::link(*parent);
			}
			else // log error
			{
				dpl::Logger::ref().push_error("Fail to import relation. The specified parent could not be found: " + REFERENCE.name());
			}
		}

		inline void				export_relation_to_binary(		std::ostream&			stream) const
		{
			Reference::save_to_binary_opt(MyBaseT::other(), stream);
		}
	};


	template<typename ChildT, typename ParentT>
	class	ChildBase<ParentT, ChildT, RelationType::ONE_TO_MANY> 
		: private dpl::Member<ParentT, ChildT, dpl::get_relation_ID<ParentT, ChildT>()>
	{
	private: // subtypes
		static const uint32_t RELATION_ID	= dpl::get_relation_ID<ParentT, ChildT>();
		using	ParentBaseT					= ParentBase<ParentT, ChildT, RelationType::ONE_TO_MANY>;
		using	MyMemberT					= dpl::Member<ParentT, ChildT, RELATION_ID>;
		using	MyGroupT					= dpl::Group<ParentT, ChildT, RELATION_ID>;

	public: // friends
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

	protected: // lifecycle
		CLASS_CTOR				ChildBase() = default;
		CLASS_CTOR				ChildBase(						ChildBase&&				other) noexcept = default;
		ChildBase&				operator=(						ChildBase&&				other) noexcept = default;

		ChildBase&				operator=(						dpl::Swap<ChildBase>	other)
		{
			MyMemberT::operator=(dpl::Swap<MyMemberT>(*other));
			return *this;
		}

	private: // lifecycle (deleted)
		CLASS_CTOR				ChildBase(						const ChildBase&		OTHER) = delete;
		ChildBase&				operator=(						const ChildBase&		OTHER) = delete;

	private: // functions
		inline bool				has_parent() const
		{
			return MyMemberT::is_member();
		}

		inline ParentT&			get_parent()
		{
			return *MyMemberT::get_group();
		}

		inline const ParentT&	get_parent() const
		{
			return *MyMemberT::get_group();
		}

		inline void				import_relation_from_binary(	std::istream&			stream)
		{
			MyMemberT::detach();
			const Reference REFERENCE(stream);
			if(MyGroupT* parent = REFERENCE.acquire<ParentT>()) 
			{
				parent->attach_back(*this);
			}
			else // log error
			{
				dpl::Logger::ref().push_error("Fail to import relation. The specified parent could not be found: " + REFERENCE.name());
			}
		}

		inline void				export_relation_to_binary(		std::ostream&			stream) const
		{
			Reference::save_to_binary_opt(MyMemberT::get_group(), stream);
		}
	};
}

// parent base				(internal)
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

	public: // friends
		friend	ChildBaseT;
		friend	MyBaseT;
		friend	MyLinkT;

		template<typename, dpl::is_TypeList>
		friend class	Parent;

		template<typename, dpl::is_TypeList>
		friend class	Child;

	private: // lifecycle
		CLASS_CTOR				ParentBase() = default;
		CLASS_CTOR				ParentBase(						ParentBase&&				other) noexcept = default;
		ParentBase&				operator=(						ParentBase&&				other) noexcept =default;

		ParentBase&				operator=(						dpl::Swap<ParentBase>		other)
		{
			MyBaseT::operator=(dpl::Swap<MyBaseT>(*other));
			return *this;
		}

	private: // functions
		inline bool				has_child() const
		{
			return MyBaseT::is_linked();
		}

		inline bool				can_have_another_child() const
		{
			return !has_child();
		}

		inline uint32_t			numChildren() const
		{
			return has_child()? 1 : 0;
		}

		inline bool				add_child(						ChildT&						child)
		{
			return MyBaseT::link(child);
		}

		inline bool				remove_child(					ChildT&						child)
		{
			if(MyBaseT::other() != &child) return false;
			return MyBaseT::unlink();
		}

		inline bool				remove_all_children()
		{
			return MyBaseT::unlink();
		}

		inline ChildT&			get_child()
		{
			return *MyBaseT::other();
		}

		inline const ChildT&	get_child() const
		{
			return *MyBaseT::other();
		}

		inline ChildT*			first_child()
		{
			return MyBaseT::other();
		}

		inline const ChildT*	first_child() const
		{
			return MyBaseT::other();
		}

		inline ChildT*			last_child()
		{
			return MyBaseT::other();
		}

		inline const ChildT*	last_child() const
		{
			return MyBaseT::other();
		}

		inline ChildT*			previous_child(					MyLinkT&						child)
		{
			return nullptr;
		}

		inline const ChildT*	previous_child(					const MyLinkT&					CHILD) const
		{
			return nullptr;
		}

		inline ChildT*			next_child(						MyLinkT&						child)
		{
			return nullptr;
		}

		inline const ChildT*	next_child(						const MyLinkT&					CHILD) const
		{
			return nullptr;
		}

		inline uint32_t			for_each_child(					const InvokeChild&				INVOKE_CHILD)
		{
			if(!has_child()) return 0;
			INVOKE_CHILD(get_child());
			return 1;
		}

		inline uint32_t			for_each_child(					const InvokeConstChild&			INVOKE_CHILD) const
		{
			if(!has_child()) return 0;
			INVOKE_CHILD(get_child());
			return 1;
		}

		void					import_relation_from_binary(	std::istream&					stream)
		{
			MyBaseT::unlink();
			const Reference REFERENCE(stream);
			if(ChildBaseT* child = REFERENCE.acquire<ChildT>())
			{
				MyBaseT::link(*child);
			}
			else // log error
			{
				dpl::Logger::ref().push_error("Fail to import relation. The specified child could not be found: " + REFERENCE.name());
			}
		}

		inline void				export_relation_to_binary(		std::ostream&					stream) const
		{
			Reference::save_to_binary_opt(first_child(), stream);
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

	public: // friends
		friend	ChildBaseT;
		friend	MyMemberT;
		friend	MyGroupT;

		template<typename, dpl::is_TypeList>
		friend class	Parent;

		template<typename, dpl::is_TypeList>
		friend class	Child;

	private: // lifecycle
		CLASS_CTOR				ParentBase() = default;

		CLASS_CTOR				ParentBase(						ParentBase&&				other) noexcept = default;

		ParentBase&				operator=(						ParentBase&&				other) noexcept = default;

		ParentBase&				operator=(						dpl::Swap<ParentBase>		other)
		{
			MyGroupT::operator=(dpl::Swap<MyGroupT>(*other));
			return *this;
		}

	private: // functions
		inline bool				has_child() const
		{
			return MyGroupT::size() > 0;
		}

		inline bool				can_have_another_child() const
		{
			return true;
		}

		inline uint32_t			numChildren() const
		{
			return MyGroupT::size();
		}

		inline bool				add_child(						ChildT&						child)
		{
			// TEST type restrictions;
			return MyGroupT::add_end_member(child);
		}

		inline bool				remove_child(					ChildT&						child)
		{
			return MyGroupT::remove_member(child);
		}

		inline bool				remove_all_children()
		{
			return MyGroupT::remove_all_members();
		}

		inline ChildT&			get_child()
		{
			return *MyGroupT::first();
		}

		inline const ChildT&	get_child() const
		{
			return *MyGroupT::first();
		}

		inline ChildT*			first_child()
		{
			return MyGroupT::first();
		}

		inline const ChildT*	first_child() const
		{
			return MyGroupT::first();
		}

		inline ChildT*			last_child()
		{
			return MyGroupT::last();
		}

		inline const ChildT*	last_child() const
		{
			return MyGroupT::last();
		}

		inline ChildT*			previous_child(					MyMemberT&					child)
		{
			if(!child.is_member_of(*this)) return nullptr;
			return child.previous();
		}
			
		inline const ChildT*	previous_child(					const MyMemberT&			CHILD) const
		{
			if(!CHILD.is_member_of(*this)) return nullptr;
			return CHILD.previous();
		}

		inline ChildT*			next_child(						MyMemberT&					child)
		{
			if(!child.is_member_of(*this)) return nullptr;
			return child.next();
		}

		inline const ChildT*	next_child(						const MyMemberT&			CHILD) const
		{
			if(!CHILD.is_member_of(*this)) return nullptr;
			return CHILD.next();
		}

		inline uint32_t			for_each_child(					const InvokeChild&			INVOKE_CHILD)
		{
			return MyGroupT::for_each_member(INVOKE_CHILD);
		}

		inline uint32_t			for_each_child(					const InvokeConstChild&		INVOKE_CHILD) const
		{
			return MyGroupT::for_each_member(INVOKE_CHILD);
		}

		inline void				detach_children()
		{
			while(ChildBaseT* child = first_child())
			{
				MyMemberT&	linkedChild = *child;
							linkedChild.detach();
			}
		}

		void					import_relation_from_binary(	std::istream&				stream)
		{
			detach_children();
			Reference reference;
			const uint32_t	NUM_CHILDREN = dpl::import_t<uint32_t>(stream);
			for(uint32_t index = 0; index < NUM_CHILDREN; ++index)
			{
				reference.load_from_binary(stream);

				if(ChildBaseT* child = reference.acquire<ChildT>())
				{
					MyGroupT::attach_back(*child);
				}
				else //log error
				{
					dpl::Logger::ref().push_error("Fail to import relation. The specified child could not be found: " + reference.name());
				}
			}
		}

		void					export_relation_to_binary(		std::ostream&				stream) const
		{
			dpl::export_t(stream, numChildren());
			ParentBase::for_each_child([&](const ChildT& CHILD)
			{
				Reference::save_to_binary_opt(CHILD, stream);
			});
		}
	};
}

// Child interface			(internal)
namespace dpl
{
	template<typename ChildT, typename... ParentTn>
	class	Child<ChildT, dpl::TypeList<ParentTn...>> : public ChildBase<ParentTn, ChildT, dpl::get_relation_between<ParentTn, ChildT>()>...
	{
	private: // subtypes
		using	PARENT_TYPES	= dpl::TypeList<ParentTn...>;

		template<dpl::is_one_of<PARENT_TYPES> ParentT>
		using	BaseT			= ChildBase<ParentT, ChildT, dpl::get_relation_between<ParentT, ChildT>()>;

	public: // lifecycle
		CLASS_CTOR				Child() = default;
		CLASS_CTOR				Child(										Child&&				other) noexcept = default;
		Child&					operator=(									Child&&				other) noexcept = default;

		Child&					operator= (									dpl::Swap<Child>	other)
		{
			(BaseT<ParentTn>::operator=(dpl::Swap<BaseT<ParentTn>>(*other)), ...);
			return *this;
		}

	private: // lifecycle
		CLASS_CTOR				Child(										const Child&		OTHER) = delete;
		Child&					operator=(									const Child&		OTHER) = delete;

	public: // functions
		template<dpl::is_one_of<PARENT_TYPES> ParentT>
		inline bool				has_parent() const
		{
			return BaseT<ParentT>::has_parent();
		}

		template<dpl::is_one_of<PARENT_TYPES> ParentT>
		inline ParentT&			get_parent()
		{
			return BaseT<ParentT>::get_parent();
		}

		template<dpl::is_one_of<PARENT_TYPES> ParentT>
		inline const ParentT&	get_parent() const
		{
			return BaseT<ParentT>::get_parent();
		}

		inline void				import_relations_with_parents_from_binary(	std::istream&		stream)
		{
			(BaseT<ParentTn>::import_relation_from_binary(stream), ...);
		}

		inline void				export_relations_with_parents_to_binary(	std::ostream&		stream) const
		{
			(BaseT<ParentTn>::export_relation_to_binary(stream), ...);
		}
	};


	// Specialization for child without parents.
	template<typename ChildT>
	class	Child<ChildT, dpl::TypeList<>>{};
}

// Parent interface			(internal)
namespace dpl
{
	template<typename ParentT, typename... ChildTn>
	class	Parent<ParentT, dpl::TypeList<ChildTn...>> : public ParentBase<ParentT, ChildTn, dpl::get_relation_between<ParentT, ChildTn>()>...
	{
	public: // subtypes
		using	CHILD_TYPES		= dpl::TypeList<ChildTn...>;

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		using	Invoke			= std::function<void(ChildT&)>;

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		using	InvokeConst		= std::function<void(const ChildT&)>;

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		using	ParentBase_of	= ParentBase<ParentT, ChildT, dpl::get_relation_between<ParentT, ChildT>()>;

	public: // friends
		template<typename, typename, RelationType>
		friend	class ChildBase;

	protected: // lifecycle
		CLASS_CTOR				Parent() = default;
		CLASS_CTOR				Parent(										Parent&&					other) noexcept = default;
		Parent&					operator=(									Parent&&					other) noexcept = default;

		Parent&					operator=(									dpl::Swap<Parent>			other)
		{
			(ParentBase_of<ChildTn>::operator=(dpl::Swap<ChildTn>(*other)), ...);
			return *this;
		}

	private: // lifecycle
		CLASS_CTOR				Parent(										const Parent&				OTHER) = delete;
		Parent&					operator=(									const Parent&				OTHER) = delete;

	public: // functions
		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline bool				has_child() const
		{
			return ParentBase_of<ChildT>::has_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline bool				can_have_another_child() const
		{
			return ParentBase_of<ChildT>::can_have_another_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline uint32_t			numChildren() const
		{
			return ParentBase_of<ChildT>::numChildren();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline bool				add_child(									ChildT&						child)
		{
			return ParentBase_of<ChildT>::add_child(child);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline bool				remove_child(								ChildT&						child)
		{
			return ParentBase_of<ChildT>::remove_child(child);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline bool				remove_children_of_type()
		{
			return ParentBase_of<ChildT>::remove_all_children();
		}

		inline void				remove_all_children()
		{
			(ParentBase_of<ChildTn>::remove_all_children(), ...);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline ChildT&			get_child()
		{
			return ParentBase_of<ChildT>::get_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline const ChildT&	get_child() const
		{
			return ParentBase_of<ChildT>::get_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline ChildT*			first_child()
		{
			return ParentBase_of<ChildT>::first_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline const ChildT*	first_child() const
		{
			return ParentBase_of<ChildT>::first_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline ChildT*			last_child()
		{
			return ParentBase_of<ChildT>::last_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline const ChildT*	last_child() const
		{
			return ParentBase_of<ChildT>::last_child();
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline ChildT*			previous_child(								ChildT&						child)
		{
			return ParentBase_of<ChildT>::previous_child(child);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline const ChildT*	previous_child(								const ChildT&				CHILD) const
		{
			return ParentBase_of<ChildT>::previous_child(CHILD);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline ChildT*			next_child(									ChildT&						child)
		{
			return ParentBase_of<ChildT>::next_child(child);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline const ChildT*	next_child(									const ChildT&				CHILD) const
		{
			return ParentBase_of<ChildT>::next_child(CHILD);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline uint32_t			for_each_child(								const Invoke<ChildT>&		INVOKE_CHILD)
		{
			return ParentBase_of<ChildT>::for_each_child(INVOKE_CHILD);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline uint32_t			for_each_child(								const InvokeConst<ChildT>&	INVOKE_CHILD) const
		{
			return ParentBase_of<ChildT>::for_each_child(INVOKE_CHILD);
		}

		inline void				import_relations_with_children_from_binary(	std::istream&				stream)
		{
			(ParentBase_of<ChildTn>::import_relation_from_binary(stream), ...);
		}

		inline void				export_relations_with_children_to_binary(	std::ostream&				stream) const
		{
			(ParentBase_of<ChildTn>::export_relation_to_binary(stream), ...);
		}
	};


	// Specialization of parent without children.
	template<typename ParentT>
	class	Parent<ParentT, dpl::TypeList<>>{};
}

// maybe related			(internal, Entity base)
namespace dpl
{
	template<typename EntityT>
	class	Related	: public Child<EntityT, typename ParentList_of<EntityT>::type>
					, public Parent<EntityT, typename ChildList_of<EntityT>::type>
	{
	private: // subtypes
		using	MyChildBase		= Child<EntityT, typename ParentList_of<EntityT>::type>;
		using	MyParentBase	= Parent<EntityT, typename ChildList_of<EntityT>::type>;

	public: // friends
		template<typename>
		friend	class EntityPack_of;

	public: // lifecycle
		CLASS_CTOR			Related() = default;

		CLASS_CTOR			Related(							Related&&			other) noexcept = default;
		Related&			operator=(							Related&&			other) noexcept = default;

	private: // lifecycle(deleted)
		CLASS_CTOR			Related(							const Related&		OTHER) = delete;
		Related&			operator=(							const Related&		OTHER) = delete;

	public: // functions
		inline void			import_all_relations_from_binary(	std::istream&		stream)
		{
			MyChildBase::import_relations_with_parents_from_binary(stream);
			MyParentBase::import_relations_with_children_from_binary(stream);
		}

		inline void			export_all_relations_to_binary(		std::ostream&		stream) const
		{
			MyChildBase::export_relations_with_parents_to_binary(stream);
			MyParentBase::export_relations_with_children_to_binary(stream);
		}
	};


	struct	NotRelated{};


	template<typename EntityT>
	using	MaybeRelated = std::conditional_t<!has_Base<EntityT>, Related<EntityT>, NotRelated>;
}

// instance table			(internal) 
namespace dpl
{
	template<typename EntityT>
	class	InstanceRow_of;


	class	InstanceRow : private dpl::Member<InstanceTable, InstanceRow> // [TODO]: Change to Item
	{
	private: // subtypes
		using	MyBase		= dpl::Member<InstanceTable, InstanceRow>;
		using	MyGroup		= dpl::Group<InstanceTable, InstanceRow>;

	public: // friends
		friend	MyBase;
		friend	MyBase::MyLink;
		friend	MyBase::MyBase;
		friend	MyBase::MySequence;
		friend	MyGroup;
		friend	InstanceTable;

		template<typename>
		friend class InstanceRow_of;

	private: // lifecycle
		CLASS_CTOR					InstanceRow() = default;
		CLASS_CTOR					InstanceRow(						const InstanceRow&	OTHER) = delete;
		CLASS_CTOR					InstanceRow(						InstanceRow&&		other) noexcept = default;
		InstanceRow&				operator=(							const InstanceRow&	OTHER) = delete;
		InstanceRow&				operator=(							InstanceRow&&		other) noexcept = default;

	public: // interface
		virtual uint32_t			query_numInstances() const = 0;

	private: // internal interface	
		virtual void				push_instances(						const uint32_t		NUM_INSTANCES) = 0;

		virtual void				pop_instances(						const uint32_t		NUM_INSTANCES) = 0;

		virtual void				swap_instances(						const uint32_t		FIRST_INDEX,
																		const uint32_t		SECOND_INDEX) = 0;

		virtual void				make_last_instance(					const uint32_t		INSTANCE_INDEX) = 0;

		virtual void				save_identity_prefix(				std::ostream&		stream) const = 0;

		virtual void				save_all_instances_to_binary(		std::ostream&		stream) const = 0;

		virtual void				save_tail_instances_to_binary(		std::ostream&		stream,
																		const uint32_t		NUM_INSTANCES) const = 0;

		virtual void				load_all_instances_from_binary(		std::istream&		stream) = 0;

		virtual void				load_tail_instances_from_binary(	std::istream&		stream) = 0;
	};


	class	InstanceTable	: public InstanceRow // <-- Top row (self reference)
							, private dpl::Group<InstanceTable, InstanceRow> // [TODO]: Change to Collection
	{
	private: // subtypes
		using	MyBase		= dpl::Group<InstanceTable, InstanceRow>;
		using	MyMember	= dpl::Member<InstanceTable, InstanceRow>;
		using	InvokePack	= std::function<void(InstanceRow&)>;

	public: // friends
		friend	MyBase;
		friend	MyMember;

		template<typename>
		friend class InstanceTable_of;

	protected: // lifecycle
		CLASS_CTOR			InstanceTable()
		{
			MyBase::add_front_member(*this); //<-- Adds itself as a first row.
		}
		CLASS_CTOR			InstanceTable(						const InstanceTable&	OTHER) = delete;
		CLASS_CTOR			InstanceTable(						InstanceTable&&			other) noexcept = default;
		InstanceTable&		operator=(							const InstanceTable&	OTHER) = delete;
		InstanceTable&		operator=(							InstanceTable&&			other) noexcept = default;

	private: // functions
		inline void			add_row_of_instances(				const uint32_t			NUM_INSTANCES)
		{
			MyBase::for_each([&](InstanceRow& pack)
			{
				pack.push_instances(NUM_INSTANCES);
			});
		}

		inline void			pop_shared_instances(				const uint32_t			NUM_INSTANCES)
		{
			MyBase::for_each([&](InstanceRow& pack)
			{
				pack.pop_instances(NUM_INSTANCES);
			});
		}

		inline void			pop_all_shared_instances()
		{
			pop_shared_instances(std::numeric_limits<uint32_t>::max());
		}

		inline void			swap_shared_instances(				const uint32_t			FIRST_INDEX,
																const uint32_t			SECOND_INDEX)
		{
			MyBase::for_each([&](InstanceRow& pack)
			{
				pack.swap_instances(FIRST_INDEX, SECOND_INDEX);
			});
		}

		inline void			make_last_shared_instance(			const uint32_t			INSTANCE_INDEX)
		{
			MyBase::for_each([&](InstanceRow& pack)
			{
				pack.make_last_instance(INSTANCE_INDEX);
			});
		}

		inline void			move_shared_instances_to_binary(	std::ostream&			stream,
																const uint32_t			NUM_INSTANCES)
		{
			MyBase::for_each([&](InstanceRow& pack)
			{
				pack.save_identity_prefix(stream);
				pack.save_tail_instances_to_binary(stream, NUM_INSTANCES);
				pack.pop_instances(NUM_INSTANCES);
			});
			Reference().save_to_binary(stream); //<-- Marks end of the list.
		}

		inline void			move_shared_instance_to_binary(		std::ostream&			stream,
																const uint32_t			INSTANCE_INDEX)
		{
			MyBase::for_each([&](InstanceRow& pack)
			{
				pack.save_identity_prefix(stream);
				pack.make_last_instance(INSTANCE_INDEX);
				pack.save_tail_instances_to_binary(stream, 1);
				pack.pop_instances(1);
			});
			Reference().save_to_binary(stream); //<-- Marks end of the list.
		}

		inline void			move_shared_instances_from_binary(	std::istream&			stream)
		{
			for_each_loaded_reference(stream, [&](InstanceRow& pack) 
			{
				pack.load_all_instances_from_binary(stream);
			});
		}

		inline void			move_shared_instance_from_binary(	std::istream&			stream,
																const uint32_t			INSTANCE_INDEX)
		{
			for_each_loaded_reference(stream, [&](InstanceRow& pack) 
			{
				pack.load_tail_instances_from_binary(stream);
				pack.make_last_instance(INSTANCE_INDEX);
			});
		}

	private: // helpers
		void				for_each_loaded_reference(			std::istream&			stream,
																const InvokePack&		INVOKE)
		{
			Reference reference;
			while(true)
			{
				reference.load_from_binary(stream);
				if(!reference.valid()) break;
				if(auto* pack = reference.acquire_instance_row())
				{
					INVOKE(*pack);
				}
				else
				{
					dpl::Logger::ref().push_error("Fail to load instances due to unknown reference: [%d, %s]", reference.storageID(), reference.name().c_str());
					pop_all_shared_instances(); //<-- Remove everything to preserve layout.
				}
			}
		}
	};


	template<typename EntityT>
	concept	is_Instantiable				= dpl::is_type_complete<Instance_of<EntityT>>
										&& !is_Inheritable<EntityT>;

	template<typename EntityT>
	concept	has_transferable_instances	= is_Instantiable<EntityT>
										&& is_divisible_by<4, Instance_of<EntityT>>
										&& std::is_trivially_destructible_v<Instance_of<EntityT>>;

	template<typename EntityT>
	class	InstanceRow_of		: public InstanceRow
	{
	public: // constants
		static constexpr bool IS_TRANSFERABLE = has_transferable_instances<EntityT>;

	private: // subtypes
		using	Instance_t		= Instance_of<EntityT>;
		using	InstanceStorage	= std::conditional_t<	IS_TRANSFERABLE,
														dpl::StreamChunk<Instance_t>, 
														dpl::DynamicArray<Instance_t>>;

	public: // subtypes
		using	NumInstances	= uint32_t;
		using	Invoke			= std::function<void(Instance_t&)>;
		using	InvokeConst		= std::function<void(const Instance_t&)>;
		using	InvokeAll		= std::function<void(Instance_t*, NumInstances)>;
		using	InvokeAllConst	= std::function<void(const Instance_t*, NumInstances)>;

	private: // data
		InstanceStorage m_instances;

	protected: // lifecycle
		CLASS_CTOR					InstanceRow_of() = default;
		CLASS_CTOR					InstanceRow_of(						const InstanceRow_of&	OTHER) = delete;
		CLASS_CTOR					InstanceRow_of(						InstanceRow_of&&		other) noexcept = default;
		InstanceRow_of&				operator=(							const InstanceRow_of&	OTHER) = delete;
		InstanceRow_of&				operator=(							InstanceRow_of&&		other) noexcept = default;

	public: // functions
		inline Instance_t*			modify_instances()
		{
			return m_instances.modify();
		}

		inline const Instance_t*	read_instances() const
		{
			return m_instances.read();
		}

		inline void					invoke_instances(					const InvokeAll&		INVOKE)
		{
			if constexpr(IS_TRANSFERABLE)	INVOKE(m_instances.modify(), m_instances.size());
			else							INVOKE(m_instances.data(), m_instances.size());
		}

		inline void					invoke_instances(					const InvokeAllConst&	INVOKE) const
		{
			
			if constexpr(IS_TRANSFERABLE)	INVOKE(m_instances.read(), m_instances.size());
			else							INVOKE(m_instances.data(), m_instances.size());
		}

		inline void					for_each_instance(					const Invoke&			INVOKE)
		{
			invoke_instances([&](Instance_t* instances, uint32_t NUM_INSTANCES)
			{
				for(uint32_t index = 0; index < NUM_INSTANCES; ++index)
				{
					INVOKE(instances[index]);
				}
			});
		}

		inline void					for_each_instance(					const InvokeConst&		INVOKE) const
		{
			invoke_instances([&](const Instance_t* INSTANCES, uint32_t NUM_INSTANCES)
			{
				for(uint32_t index = 0; index < NUM_INSTANCES; ++index)
				{
					INVOKE(INSTANCES[index]);
				}
			});
		}

	private: // interface implementation
		virtual uint32_t			query_numInstances() const final override
		{
			return m_instances.size();
		}

		virtual void				push_instances(						const uint32_t			NUM_INSTANCES) final override
		{
			m_instances.enlarge(NUM_INSTANCES);
		}

		virtual void				pop_instances(						const uint32_t			NUM_INSTANCES) final override
		{
			m_instances.reduce_if_possible(NUM_INSTANCES);
		}

		virtual void				swap_instances(						const uint32_t			FIRST_INDEX,
																		const uint32_t			SECOND_INDEX) final override
		{
			m_instances.swap_elements(FIRST_INDEX, SECOND_INDEX);
		}

		virtual void				make_last_instance(					const uint32_t			INSTANCE_INDEX) final override
		{
			m_instances.make_last(INSTANCE_INDEX);
		}

		virtual void				save_identity_prefix(				std::ostream&			stream) const final override
		{
			Reference::save_to_binary_opt(static_cast<const Entity<EntityT>&>(*this), stream);
		}

		virtual void				save_all_instances_to_binary(		std::ostream&			stream) const final override
		{
			m_instances.export_to(stream);
		}

		virtual void				save_tail_instances_to_binary(		std::ostream&			stream,
																		const uint32_t			NUM_INSTANCES) const final override
		{
			m_instances.export_tail_to(stream, NUM_INSTANCES);
		}

		virtual void				load_all_instances_from_binary(		std::istream&			stream) final override
		{
			m_instances.import_from(stream);
		}

		virtual void				load_tail_instances_from_binary(	std::istream&			stream) final override
		{
			m_instances.import_tail_from(stream);
		}
	};
}

// maybe instantiable		(internal, Entity base)
namespace dpl
{
	template<typename EntityT>
	using	Instantiable = InstanceRow_of<EntityT>; // [TODO]: or InstanceTable_of<EntityT>;


	template<typename EntityT> //<-- Removes C4584 WARNING
	struct	NotInstantiable{};


	template<typename EntityT>
	using	MaybeInstantiable = std::conditional_t<is_Instantiable<EntityT>, Instantiable<EntityT>, NotInstantiable<EntityT>>;
}

// component storage		(internal)
namespace dpl
{
	template<typename ComponentT>
	concept is_Component				= std::is_default_constructible_v<ComponentT>
										&& std::is_copy_assignable_v<ComponentT>;

	template<typename ComponentT>
	concept is_TransferableComponent	= is_Component<ComponentT>
										&& is_divisible_by<4, ComponentT>
										&& std::is_trivially_destructible_v<ComponentT>;

	template<typename ComponentT>
	struct	IsComponent
	{
		static const bool value = is_Component<ComponentT>;
	};

	template<typename COMPONENT_TYPES>
	concept	is_ComponentTypeList		= dpl::is_TypeList<COMPONENT_TYPES> 
										&& COMPONENT_TYPES::ALL_UNIQUE 
										&& COMPONENT_TYPES::template all<IsComponent>();

	template<typename CompositeT, is_ComponentTypeList COMPONENT_TYPES>
	class	ComponentTable;


	template<typename EntityT>
	struct	ComponentQuery
	{
		using Base						= std::conditional_t<has_Base<EntityT>, typename Base_of<EntityT>::type, void>;
		using OwnComponentTypes			= typename ComponentList_of<EntityT>::type;
		using InheritedComponentTypes	= typename ComponentQuery<Base>::AllComponentTypes;

		using AllComponentTypes			= dpl::merge_t<	OwnComponentTypes, 
														InheritedComponentTypes>;
	};

	template<>
	struct	ComponentQuery<void>
	{
		using AllComponentTypes = dpl::TypeList<>;
	};

	template<typename EntityT>
	using	AllComponentTypes_of	= typename ComponentQuery<EntityT>::AllComponentTypes;

	template<typename EntityT>
	concept is_Composite			= (AllComponentTypes_of<EntityT>::SIZE > 0);

	template<typename CompositeT, typename... ComponentTn>
	class	ComponentTable<CompositeT, dpl::TypeList<ComponentTn...>>
	{
	public: // subtypes
		using	COMPONENT_TYPES = dpl::TypeList<ComponentTn...>;
		using	Column			= std::tuple<ComponentTn*...>;
		using	ConstColumn		= std::tuple<const ComponentTn*...>;

		template<is_Component T>
		using	ColumnStorage	= std::conditional_t<	is_TransferableComponent<T>,
														dpl::StreamChunk<T>,
														dpl::DynamicArray<T>>;

		template<is_Component T>
		class	Row	: private ColumnStorage<T>
		{
		private: // subtypes
			using	MyStorageBase	= ColumnStorage<T>;

		public: // subtypes
			using	Invocation		= typename ColumnStorage<T>::Invocation;
			using	ConstInvocation	= typename ColumnStorage<T>::ConstInvocation;

		public: // constants
			static constexpr bool IS_TRANSFERABLE = std::is_same_v<MyStorageBase, dpl::StreamChunk<T>>;

		public: // friends
			friend	MyStorageBase;
			friend	ComponentTable;

		public: // exposed functions
			using	MyStorageBase::size;
			using	MyStorageBase::index_of;

		public: // lifecycle
			CLASS_CTOR			Row() = default;
			CLASS_CTOR			Row(				Row&&					other) noexcept = default;
			Row&				operator=(			Row&&					other) noexcept = default;

		private: // lifecycle (deleted)
			CLASS_CTOR			Row(				const Row&				OTHER) = delete;
			Row&				operator=(			const Row&				OTHER) = delete;

		public: // functions
			inline uint32_t		offset() const
			{
				if constexpr(IS_TRANSFERABLE)	return MyStorageBase::offset;
				else							return 0;
			}

			inline T*			modify()
			{
				if constexpr(IS_TRANSFERABLE)	return MyStorageBase::modify();
				else							return MyStorageBase::data();
			}

			inline const T*		read() const
			{
				if constexpr(IS_TRANSFERABLE)	return MyStorageBase::read();
				else							return MyStorageBase::data();
			}

			inline void			modify_each(		const Invocation&		INVOKE)
			{
				if constexpr(IS_TRANSFERABLE)	return MyStorageBase::modify_each(INVOKE);
				else							return MyStorageBase::for_each(INVOKE);
			}

			inline void			read_each(			const ConstInvocation&	INVOKE) const
			{
				if constexpr(IS_TRANSFERABLE)	return MyStorageBase::read_each(INVOKE);
				else							return MyStorageBase::for_each(INVOKE);
			}

			inline T*			at(					const uint32_t			COLUMN_INDEX)
			{
				return modify() + COLUMN_INDEX;
			}

			inline const T*		at(					const uint32_t			COLUMN_INDEX) const
			{
				return read() + COLUMN_INDEX;
			}

			inline uint32_t		index_of(			const T*				COMPONENT_ADDRESS) const
			{
				return MyStorageBase::index_of(COMPONENT_ADDRESS);
			}

		private: // internal functions
			inline T*			enlarge(			const uint32_t			NUM_COLUMNS)
			{
				return MyStorageBase::enlarge(NUM_COLUMNS);
			}

			inline void			destroy_at(			const uint32_t			COLUMN_INDEX)
			{
				MyStorageBase::fast_erase(COLUMN_INDEX);
			}
		};

		using	Rows			= std::tuple<Row<ComponentTn>...>;

	private: // data
		Rows m_rows; //<-- Each row has the same size.

	protected: // lifecycle
		CLASS_CTOR						ComponentTable() = default;

	public: // row functions
		static constexpr uint32_t		numRows()
		{
			return COMPONENT_TYPES::SIZE;
		}

		template<dpl::is_one_of<COMPONENT_TYPES> T>
		inline Row<T>&					row()
		{
			return std::get<Row<T>>(m_rows);
		}

		template<dpl::is_one_of<COMPONENT_TYPES> T>
		inline const Row<T>&			row() const
		{
			return std::get<Row<T>>(m_rows);
		}

	public: // column functions
		inline uint32_t					numColumns() const
		{
			using FirstComponentType = COMPONENT_TYPES::template At<0>;
			return Row<FirstComponentType>::size();
		}

		inline Column					column(			const uint32_t		COLUMN_INDEX)
		{
			return std::make_tuple(ComponentTable::row<ComponentTn>().at(COLUMN_INDEX)...);
		}

		inline ConstColumn				column(			const uint32_t		COLUMN_INDEX) const
		{
			return std::make_tuple(ComponentTable::row<ComponentTn>().at(COLUMN_INDEX)...);
		}

		template<dpl::is_one_of<COMPONENT_TYPES> T>
		inline uint32_t					column_index(	const T*			COMPONENT_ADDRESS) const
		{
			return ComponentTable::row<T>().index_of(COMPONENT_ADDRESS);
		}

	protected: // column functions
		inline Column					add_columns(	const uint32_t		NUM_COLUMNS)
		{
			return std::make_tuple(ComponentTable::row<ComponentTn>().enlarge(NUM_COLUMNS)...);
		}

		inline Column					add_column()
		{
			return ComponentTable::add_columns(1);
		}
			
		inline void						remove_column(	const uint32_t		COLUMN_INDEX)
		{
			(ComponentTable::row<ComponentTn>().destroy_at(COLUMN_INDEX), ...);
		}
	};

	template<typename T>
	using	MaybeComponentTable		= std::conditional_t<is_Composite<T>, ComponentTable<T, AllComponentTypes_of<T>>, std::monostate>;
}

// maybe composite			(internal)
namespace dpl
{
	template<typename EntityT>
	using	MaybeIdentified = std::conditional_t<!has_Base<EntityT>, Identity, typename Base_of<EntityT>::type>;

	template<typename EntityT, is_ComponentTypeList COMPONENT_TYPES>
	class	MaybeComposite;


	template<typename EntityT>
	class	MaybeComposite<EntityT, dpl::TypeList<>> : private	MaybeIdentified<EntityT>
	{
	public: // friends
		friend	MaybeRelated<EntityT>;

		template<typename>
		friend class EntityPack_of;

	private: // subtypes
		using	MyIdentity = MaybeIdentified<EntityT>;
		
	public: // inherited members
		using	MyIdentity::MyIdentity;
		using	MyIdentity::name;
		using	MyIdentity::storageID;

	public: // lifecycle
		CLASS_CTOR		MaybeComposite(		const MaybeComposite&	OTHER) = delete;
		CLASS_CTOR		MaybeComposite(		MaybeComposite&&		other) noexcept = default;
		MaybeComposite&	operator=(			const MaybeComposite&	OTHER) = delete;
		MaybeComposite&	operator=(			MaybeComposite&&		other) noexcept = default;
	};


	template<typename EntityT, is_Component... Ts>
	class	MaybeComposite<EntityT, dpl::TypeList<Ts...>> : private	MaybeIdentified<EntityT>
	{
	public: // friends
		friend	MaybeRelated<EntityT>;

		template<typename>
		friend class EntityPack_of;

	private: // subtypes
		using	MyIdentity			= MaybeIdentified<EntityT>;

	public: // subtypes
		using	COMPONENT_TYPES		= AllComponentTypes_of<EntityT>;
		using	Table				= ComponentTable<EntityT, COMPONENT_TYPES>;
		using	Column				= typename Table::Column;
		using	ConstColumn			= typename Table::ConstColumn;
		using	MyPack				= EntityPack_of<EntityT>;

	public: // inherited members
		using	MyIdentity::MyIdentity;
		using	MyIdentity::name;
		using	MyIdentity::storageID;

	public: // lifecycle
		CLASS_CTOR		MaybeComposite(		const MaybeComposite&	OTHER) = delete;
		CLASS_CTOR		MaybeComposite(		MaybeComposite&&		other) noexcept = default;
		MaybeComposite&	operator=(			const MaybeComposite&	OTHER) = delete;
		MaybeComposite&	operator=(			MaybeComposite&&		other) noexcept = default;

	public: // functions
		template<typename T>
		inline bool				has_type() const
		{
			return EntityPack_of<T>::ref().typeID() == MyIdentity::storageID();
		}

	public: // component access
		template<dpl::is_one_of<COMPONENT_TYPES> T>
		inline T&				get_component()
		{
			EntityPack_of<EntityT>& pack = EntityPack_of<EntityT>::ref();
			return *pack.row<T>().at(get_index(pack));
		}

		template<dpl::is_one_of<COMPONENT_TYPES> T>
		inline const T&			get_component() const
		{
			const EntityPack_of<EntityT>& PACK = EntityPack_of<EntityT>::ref();
			return *PACK.row<T>().at(get_index(PACK));
		}

		inline Column			get_all_components()
		{
			EntityPack_of<EntityT>& pack = EntityPack_of<EntityT>::ref();
			return pack.column(get_index(pack));
		}

		inline ConstColumn		get_all_components() const
		{
			const EntityPack_of<EntityT>& PACK = EntityPack_of<EntityT>::ref();
			return PACK.column(get_index(PACK));
		}

		inline void				import_components_from_binary(	std::istream&	stream)
		{
			std::apply
			(
				[&stream](Ts*... components)
				{
					(dpl::import_t(stream, *components), ...);

				}, get_all_components()
			);
		}

		inline void				export_components_to_binary(	std::ostream&	stream) const
		{
			std::apply
			(
				[&stream](const Ts*... components)
				{
					(dpl::export_t(stream, *components), ...);

				}, get_all_components()
			);
		}

	private: // functions
		inline const uint32_t	get_index(						const MyPack&	PACK) const
		{
			return PACK.index_of(static_cast<EntityT*>(this));
		}
	};
}

// entity implementation	<------------------------------ FOR THE USER
namespace dpl
{
	/*
		[IDENTITY]:
		- Each entity is associated with a unique name that can be used to distinguish it from others, or to query it in the EntityPack_of<EntityT>..
		- In addition to its name, each entity has a storageID that indicates the entity pack in which it is stored (its origin).

		[RELATIONS]:
		- The root entity is the only one authorized to provide a description of its parents and children.
		- The derived entity inherits the relations defined by its base entity.
		- The order of linked children is undefined.	

		[INSTANCING]:
		- Specialize Instance_of<EntityT> to indicate that this entity type is instantiable.
		- To ensure consistency in the number of instances across multiple entities, they can be linked together in a group.

		[COMPOSITION]:
		- A separate, contiguous buffer is allocated to store each type of component.
		- Linking the buffer that stores a component type to the StreamController object is possible if the component type satisfies the requirements for transferable data.
		- The component types specified in the entity description are combined with those inherited from the Base.
		- To meet the requirement, non-trivially destructible components must have operator>>(std::ostream&) and operator<<(std::istream&) implemented.
	*/
	template<typename EntityT>
	class	Entity	: private	dpl::NamedType<EntityT>
					, public	MaybeComposite<EntityT, AllComponentTypes_of<EntityT>>
					, public	MaybeRelated<EntityT>
					, public	MaybeInstantiable<EntityT>
	{
	private: // subtypes
		using	MyTypeName		= dpl::NamedType<EntityT>;
		using	MyComposition	= MaybeComposite<EntityT, AllComponentTypes_of<EntityT>>;

	public: // inherited members
		using	MyComposition::MyComposition;

	public: // lifecycle
		CLASS_CTOR					Entity(		const Entity&	OTHER) = delete;
		CLASS_CTOR					Entity(		Entity&&		other) noexcept = default;
		Entity&						operator=(	const Entity&	OTHER) = delete;
		Entity&						operator=(	Entity&&		other) noexcept = default;

	public: // functions
		static const std::string&	get_typeName()
		{
			return MyTypeName::typeName();
		}
	};
}

// entity storage			<------------------------------ FOR THE USER
namespace dpl
{
	inline InstanceRow*	Reference::acquire_instance_row() const
	{
		EntityPack* pack = EntityManager::ref().find_base_variant(storageID());
		return pack? pack->find_instance_row(name()) : nullptr;
	}


	/*
		[DATA]:			EntityT
		[STORAGE]:		contiguous
		[ORDER]:		undefined
		[INSERTION]:	fast
		[ERASURE]:		fast
	*/
	template<typename EntityT>
	class	EntityPack_of	: public EntityPack
							, public dpl::Singleton<EntityPack_of<EntityT>>
							, public MaybeComponentTable<EntityT>
	{
	private: // subtypes
		using	MySingletonBase		= dpl::Singleton<EntityPack_of<EntityT>>;
		using	MyComponentTable	= MaybeComponentTable<EntityT>;

	public: // subtypes
		using	EntityIndex			= uint32_t;
		using	NumEntities			= uint32_t;
		using	Invoke				= std::function<void(EntityT&)>;
		using	InvokeAll			= std::function<void(EntityT*, NumEntities)>;
		using	InvokeConst			= std::function<void(const EntityT&)>;
		using	InvokeAllConst		= std::function<void(const EntityT*, NumEntities)>;
		using	InvokeIndexed		= std::function<void(EntityT&, EntityIndex)>;
		using	InvokeIndexedConst	= std::function<void(const EntityT&, EntityIndex)>;

	public: // friends
		friend	EntityManager;
		friend  Entity<EntityT>;
		friend	dpl::Variation<EntityManager, EntityPack>;

	public: // data
		dpl::ReadOnly<uint32_t, EntityPack_of> typeID;

	private: // data
		dpl::Labeler<char>		m_labeler;
		std::vector<EntityT>	m_entities;

	public: // lifecycle
		CLASS_CTOR					EntityPack_of(				const Binding&					BINDING)
			: EntityPack(BINDING)
			, MySingletonBase(static_cast<EntityManager*>(BINDING.owner())->owner())
			, typeID(EntityPack::get_typeID<EntityPack_of<EntityT>>())
		{

		}

	public: // basic functions
		inline void					reserve_names(				const uint32_t					NUM_NAMES)
		{
			m_labeler.reserve(NUM_NAMES);
		}

		inline EntityT&				create(						const Name&						ENTITY_NAME)
		{
			if constexpr (is_Composite<EntityT>) MyComponentTable::add_row();
			return m_entities.emplace_back(Origin(ENTITY_NAME, typeID(), m_labeler));
		}

		inline EntityT&				create(						const Name::Type&				NAME_TYPE,
																const std::string&				NAME)
		{
			return create(Name(NAME_TYPE, NAME));
		}

		bool						destroy(					const EntityT&					ENTITY)
		{
			const uint64_t ENTITY_INDEX = dpl::get_element_index(m_entities, ENTITY);
			if(ENTITY_INDEX >= m_entities.size()) return false;
			dpl::fast_remove(m_entities, m_entities.begin() + ENTITY_INDEX);
			if constexpr (is_Composite<EntityT>) MyComponentTable::remove_row((uint32_t)ENTITY_INDEX);
			return true;
		}

		inline bool					destroy(					const std::string&				NAME)
		{
			const auto* ENTITY = find(NAME);
			if(!ENTITY) return false;
			return destroy(*ENTITY);
		}

	public: // query functions
		inline uint32_t				size() const
		{
			return (uint32_t)m_entities.size();
		}

		inline uint32_t				index_of(					const EntityT*					ENTITY) const
		{
			const uint64_t INDEX64 = dpl::get_element_index(m_entities, ENTITY);
			return (INDEX64 > EntityPack::INVALID_INDEX) ? EntityPack::INVALID_INDEX : (uint32_t)INDEX64;
		}

		inline bool					contains(					const uint32_t					ENTITY_INDEX) const
		{
			return ENTITY_INDEX < size();
		}

		inline bool					contains(					const EntityT*					ENTITY) const
		{
			return EntityPack_of::contains(index_of(ENTITY));
		}

		inline EntityT*				find(						const std::string&				ENTITY_NAME)
		{
			EntityT* entity = static_cast<EntityT*>(m_labeler.find_entry(ENTITY_NAME));
			return EntityPack_of::contains(entity)? entity : nullptr;
		}

		inline const EntityT*		find(						const std::string&				ENTITY_NAME) const
		{
			const EntityT* ENTITY = static_cast<const EntityT*>(m_labeler.find_entry(ENTITY_NAME));
			return EntityPack_of::contains(ENTITY)? ENTITY : nullptr;
		}

		inline EntityT&				get(						const std::string&				ENTITY_NAME)
		{
			return *find(ENTITY_NAME);
		}

		inline const EntityT&		get(						const std::string&				ENTITY_NAME) const
		{
			return *find(ENTITY_NAME);
		}

		inline EntityT&				get(						const uint32_t					ENTITY_INDEX)
		{
			return m_entities[ENTITY_INDEX];
		}

		inline const EntityT&		get(						const uint32_t					ENTITY_INDEX) const
		{
			return m_entities[ENTITY_INDEX];
		}

		static inline bool			is_family_ID(				const uint32_t					TYPE_ID)
		{
			if(EntityPack::get_typeID<EntityPack_of<EntityT>>() == TYPE_ID) return true;
			if constexpr(has_Base<EntityT>)
			{
				return EntityPack_of<typename Base_of<EntityT>::type>::is_family_ID(TYPE_ID);
			}
			return false;
		}

		uint32_t					guess_ID_from_byte(			const char*						ENTITY_MEMBER_PTR) const
		{
			static const uint64_t	STRIDE			= sizeof(EntityT);
			const char*				BEGIN			= reinterpret_cast<const char*>(m_entities.data());
			const uint64_t			TOTAL_NUM_BYTES = STRIDE * m_entities.size();
			const uint64_t			BYTE_OFFSET		= ENTITY_MEMBER_PTR - BEGIN;
			const char*				END				= BEGIN + TOTAL_NUM_BYTES;

			if(ENTITY_MEMBER_PTR < BEGIN || ENTITY_MEMBER_PTR > END) return EntityPack::INVALID_ENTITY_ID;
			return (uint32_t)(BYTE_OFFSET / STRIDE);
		}

	public: // iteration functions
		inline void					for_each(					const Invoke&					INVOKE)
		{
			std::for_each(m_entities.begin(), m_entities.end(), INVOKE);
		}

		inline void					for_each(					const InvokeAll&				INVOKE)
		{
			INVOKE(m_entities.data(), size());
		}

		inline void					for_each(					const InvokeConst&				INVOKE) const
		{
			std::for_each(m_entities.begin(), m_entities.end(), INVOKE);
		}

		inline void					for_each(					const InvokeAllConst&			INVOKE) const
		{
			INVOKE(m_entities.data(), size());
		}

		inline void					for_each(					const InvokeIndexed&			INVOKE)
		{
			const uint32_t SIZE = size();
			for(uint32_t index = 0; index < SIZE; ++index)
			{
				INVOKE(m_entities[index], index);
			}
		}

		inline void					for_each(					const InvokeIndexedConst&		INVOKE)
		{
			const uint32_t SIZE = size();
			for(uint32_t index = 0; index < SIZE; ++index)
			{
				INVOKE(m_entities[index], index);
			}
		}

	private: // interface implementation
		virtual const Identity*		find_identity(				const std::string&				ENTITY_NAME) const final override
		{
			return find(ENTITY_NAME);
		}

		virtual const Identity&		guess_identity_from_byte(	const char*						ENTITY_MEMBER_PTR) const final override
		{
			const uint32_t ENTITY_ID = guess_ID_from_byte(ENTITY_MEMBER_PTR);
			return (ENTITY_ID != EntityPack::INVALID_ENTITY_ID)? get(ENTITY_ID) : EntityManager::ref().false_identity();
		}

		virtual InstanceRow*		find_instance_row(			const std::string&				ENTITY_NAME) final override
		{
			if constexpr (std::is_base_of_v<InstanceRow, EntityT>) return find(ENTITY_NAME);
			else return nullptr;
		}

		virtual char*				get_entity_as_bytes(		const std::string&				ENTITY_NAME) final override
		{
			Entity<EntityT>* entity = find(ENTITY_NAME);
			return entity? reinterpret_cast<char*>(entity) : nullptr;
		}

		virtual char*				get_base_entity_as_bytes(	const std::string&				ENTITY_NAME) final override
		{
			RootBase_of<EntityT>* entity = find(ENTITY_NAME);
			return entity? reinterpret_cast<char*>(entity) : nullptr;
		}

		virtual bool				is_family_line(				const uint32_t					TYPE_ID) const final override
		{
			return is_family_ID(TYPE_ID);
		}
	};
}

// TODO: commands

#pragma pack(pop)