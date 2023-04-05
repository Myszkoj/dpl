#pragma once


#include <optional>
#include <dpl_Chain.h>
#include <dpl_Variation.h>
#include <dpl_StaticHolder.h>
#include <dpl_std_addons.h>
#include <dpl_Binary.h>
#include <dpl_NamedType.h>
#include <dpl_Labelable.h>
#include "complex_Components.h"
#include "complex_Instances.h"
#include "complex_StateMachine.h"

#pragma pack(push, 4)

// declarations
namespace complex
{
	template<typename ObjT>
	struct	Traits;

	template<typename ObjT>
	class	Object;

	template<typename ObjT>
	concept is_Object			= std::is_base_of_v<Object<ObjT>, ObjT>;

	template<typename ObjT>
	struct	IsObject
	{
		static const bool value = is_Object<ObjT>;
	};

	class	ObjectPack;

	template<typename T>
	class	ObjectPack_of;

	class	ObjectManager;

	template<typename OBJECT_TYPES>
	concept	is_ObjectTypeList	= dpl::is_TypeList<OBJECT_TYPES> && OBJECT_TYPES::ALL_UNIQUE && OBJECT_TYPES::template all<IsObject>;
}

// ObjectPack implementation
namespace complex
{
	/*
		Base class for object packs.
	*/
	class	ObjectPack : public dpl::Variant<ObjectManager, ObjectPack>
	{
	protected: // lifecycle
		CLASS_CTOR					ObjectPack(					const Binding&		BINDING)
			: Variant(BINDING)
		{

		}

	public: // interface
		virtual const std::string&	guess_name_from_byte(		const char*			OBJECT_MEMBER_BYTE_PTR) const = 0;

		virtual InstanceRow*		get_object_as_instance_pack(const std::string&	OBJECT_NAME) = 0;

		virtual char*				get_object_as_bytes(		const std::string&	OBJECT_NAME) = 0;

	public: // functions
		template<typename MemberT>
		inline const std::string&	guess_object_name(			const MemberT*		OBJECT_MEMBER) const
		{
			return guess_name_from_byte(reinterpret_cast<const char*>(OBJECT_MEMBER));
		}
	};


	/*template<typename ObjT>
	inline uint32_t get_object_typeID()
	{
		return ObjectPack::get_typeID<ObjectPack_of<ObjT>>();
	}*/
}

// relations
namespace complex
{
	enum	Relation
	{
		ONE_TO_ONE, // One child attached to one parent.
		MANY_TO_ONE // Many children attached to one parent.
	};

	template<Relation RELATION, typename... Tn>
	struct	Bond
	{
		using Alternative = dpl::TypeList<Tn...>;

		static constexpr Relation	relation()
		{
			return RELATION;
		}

		template<typename T>
		static constexpr bool		includes()
		{
			return Alternative::template has_type<T>();
		}
	};

	template<typename Test, template<Relation, typename...> class Ref>
	struct	IsBond : public std::false_type {};

	template<template<Relation, typename...> class Ref, Relation RELATION, typename... Args>
	struct	IsBond<Ref<RELATION, Args...>, Ref>: std::true_type {};

	template<typename BondT>
	concept is_Bond				= IsBond<BondT, Bond>::value;

	template<typename ChildT, is_Bond BondT>
	class	Parent;

	template<typename ChildT, is_Bond BondT>
	class	Child;

	template<typename ChildT, dpl::is_TypeList BOND_TYPES>
	class	ChildWithManyParents;

	template<typename ParentT, dpl::is_TypeList CHILD_TYPES>
	class	ParentWithManyChildren;

	/*
		Specialize to change internal structure of the object.
	*/
	template<typename ObjT>
	struct	Traits
	{
		using ParentTypes		= dpl::TypeList<>;
		using ChildTypes		= dpl::TypeList<>;
		using ComponentTypes	= dpl::TypeList<>;
	};

	template<typename ObjT>
	using	ParentTypes_of		= typename Traits<ObjT>::ParentTypes;

	template<typename T>
	using	ChildTypes_of		= typename Traits<T>::ChildTypes;

	template<typename T>
	using	ComponentTypes_of	= typename Traits<T>::ComponentTypes;


	template<typename BondT, typename ChildT>
	concept is_Bond_of			= ParentTypes_of<ChildT>::template has_type<BondT>();


	template<typename ChildT, is_Bond T>
	constexpr uint32_t	get_bondID()
	{
		return ParentTypes_of<ChildT>::template index_of<T>();
	}

	/*
		Returns true if object can be an offspring of another object.
	*/
	template<typename T>
	constexpr bool		is_offspring()
	{
		return ParentTypes_of<T>::SIZE > 0;
	}

	/*
		Returns true if object can spawn children.
	*/
	template<typename T>
	constexpr bool		is_fertile()
	{
		return ChildTypes_of<T>::SIZE > 0;
	}

	template<typename T>
	constexpr bool		is_instantiable()
	{
		return dpl::is_type_complete<Instance_of<T>>;
	}

	/*
		Returns true if object have additional components associated with it.
	*/
	template<typename T>
	constexpr bool		is_composite()
	{
		return ComponentTypes_of<T>::SIZE > 0;
	}

	template<typename ParentT, typename ChildT>
	struct	RelatedPair
	{
	private: // subtypes
		template<typename BondT>
		struct	HasParentType
		{
			static const bool value = BondT::template includes<ParentT>();
		};

		using	ParentTypes			= ParentTypes_of<ChildT>;
		using	BondsWithParentT	= ParentTypes::template Subtypes<HasParentType>;

		static_assert(ParentTypes::template any<HasParentType>(), "ParentT is not bonded with ChildT.");
		static_assert(BondsWithParentT::SIZE < 2, "ParentT can only be bonded once per ChildT.");

	public:
		using BondT			= BondsWithParentT::template At<0>;

		static const Relation	RELATION	= BondT::relation();
		static const uint32_t	BOND_ID		= complex::get_bondID<ChildT, BondT>();

		using ChildBaseT	= Child<ChildT, BondT>;
		using ParentBaseT	= Parent<ChildT, BondT>;
	};
}

// handles
namespace complex
{
	template<typename ChildT, is_Bond_of<ChildT> BondT>
	class	ParentHandle
	{
	public: // subtypes
		using	BaseT	= Parent<ChildT, BondT>;

		class	TypeWrapper
		{
		public: // constants
			static const uint16_t INVALID_INDEX16 = std::numeric_limits<uint16_t>::max();

		public: // data
			dpl::ReadOnly<uint16_t, TypeWrapper> ID;
			dpl::ReadOnly<uint16_t,	TypeWrapper> baseOffset;

		private: // lifecycle
			CLASS_CTOR	TypeWrapper(	const uint16_t			TYPE_ID,
										const uint16_t			BASE_OFFSET)
				: ID(TYPE_ID)
				, baseOffset(BASE_OFFSET)
			{

			}

		public: // lifecycle
			CLASS_CTOR	TypeWrapper()
				: ID(TypeWrapper::INVALID_INDEX16)
				, baseOffset(TypeWrapper::INVALID_INDEX16)
			{

			}

			template<is_Object ParentT>
			CLASS_CTOR	TypeWrapper(	const Object<ParentT>&	OBJECT)
				: TypeWrapper(	(uint16_t)OBJECT.typeID(),// (uint16_t)complex::get_object_typeID<ParentT>(), 
								(uint16_t)dpl::base_offset<BaseT, ParentT>())
			{

			}

		public: // operators
			inline bool operator==(		const TypeWrapper&		OTHER) const
			{
				return (this->ID() == OTHER.ID()); // baseOffset will always be the same
			}

			inline bool operator!=(		const TypeWrapper&		OTHER) const
			{
				return !TypeWrapper::operator==(OTHER);
			}

		public: // functions
			inline void swap(			TypeWrapper&			other)
			{
				ID.swap(other.ID);
				baseOffset.swap(other.baseOffset);
			}

			template<is_Object ParentT>
			inline void	reset(			const Object<ParentT>&	OBJECT)
			{
				ID			= OBJECT.typeID(); //complex::get_object_typeID<ParentT>();
				baseOffset	= (uint16_t)dpl::base_offset<BaseT, ParentT>();
			}

			template<is_Object ParentT>
			inline bool is_same_as() const
			{
				return ID() == ObjectPack::get_typeID<ObjectPack_of<ParentT>>(); //complex::get_object_typeID<ParentT>();
			}

			inline bool	valid() const
			{
				return ID() != TypeWrapper::INVALID_INDEX16;
			}

			inline void invalidate()
			{
				ID			= TypeWrapper::INVALID_INDEX16;
				baseOffset	= TypeWrapper::INVALID_INDEX16;
			}

		public: // debug exceptions
			template<is_Object ParentT>
			inline void throw_if_different_typeID() const
			{
#ifdef _DEBUG
				if(TypeWrapper::is_same_as<ParentT>()) return;
				throw dpl::GeneralException(__FILE__, __LINE__, std::string(typeid(ParentT).name()) + " is not valid parent type.");
#endif // _DEBUG
			}
		};

	public: // data
		dpl::ReadOnly<TypeWrapper,	ParentHandle> type;
		dpl::ReadOnly<std::string,	ParentHandle> name;

	public: // lifecycle
		CLASS_CTOR			ParentHandle() = default;

		CLASS_CTOR			ParentHandle(	const TypeWrapper		PARENT_TYPE,
											const std::string&		PARENT_NAME)
			: type(PARENT_TYPE)
			, name(PARENT_NAME)
		{

		}

		template<is_Object ParentT>
		CLASS_CTOR			ParentHandle(	const Object<ParentT>&	OBJECT)
			: ParentHandle(TypeWrapper(OBJECT), OBJECT.name())
		{

		}

	public: // operators
		inline bool			operator==(		const ParentHandle&		OTHER) const
		{
			return (this->type() == OTHER.type()) && (this->name() == OTHER.name());
		}

		inline bool			operator!=(		const ParentHandle&		OTHER) const
		{
			return !ParentHandle::operator==(OTHER);
		}

	public: // functions
		inline bool			valid() const
		{
			return type().valid() && !name().empty();
		}

		template<is_Object ParentT>
		inline void			reset(			const Object<ParentT>&	OBJECT)
		{
			type->reset(OBJECT);
			name = OBJECT.name();
		}

		inline BaseT*		acquire_parent( ObjectPack&				pack) const
		{
			// [NOTE]: We can create method that returns address of the Object<ObjT> instead of ObjT, that way we could still use the baseOffset.
			return reinterpret_cast<BaseT*>(pack.get_object_as_bytes(name()) + type().baseOffset());
		}
	};
}

// parent interface
namespace complex
{
	template<typename ChildT, typename... ParentTn>
	class	Parent<ChildT, Bond<ONE_TO_ONE, ParentTn...>> 
		: private dpl::Association<Parent<ChildT, Bond<ONE_TO_ONE, ParentTn...>>, ChildT, complex::get_bondID<ChildT, Bond<ONE_TO_ONE, ParentTn...>>()>
	{
	private: // subtypes
		using	BondT				= Bond<ONE_TO_ONE, ParentTn...>;

		static const uint32_t BOND_ID = complex::get_bondID<ChildT, BondT>();

		using	MyType				= Parent<ChildT, BondT>;
		using	MyBaseT				= dpl::Association<MyType, ChildT, BOND_ID>;
		using	MyLinkT				= dpl::Association<ChildT, MyType, BOND_ID>;

	public: // subtypes
		using	InvokeChild			= std::function<void(ChildT&)>;
		using	InvokeConstChild	= std::function<void(const ChildT&)>;

	public: // friends
		friend	MyLinkT;
		friend	Child<ChildT, BondT>;

		template<typename, dpl::is_TypeList>
		friend class	ParentWithManyChildren;

	private: // lifecycle
		CLASS_CTOR				Parent() = default;

		CLASS_CTOR				Parent(			Parent&&				other) noexcept
			: MyBaseT(std::move(other))
		{

		}

		Parent&					operator=(		Parent&&				other) noexcept
		{
			MyBaseT::operator=(std::move(other));
			return *this;
		}

		Parent&					operator=(		dpl::Swap<Parent>		other)
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

		inline ChildT*			previous_child(	MyLinkT&				child)
		{
			return nullptr;
		}

		inline const ChildT*	previous_child(	const MyLinkT&			CHILD) const
		{
			return nullptr;
		}

		inline ChildT*			next_child(		MyLinkT&				child)
		{
			return nullptr;
		}

		inline const ChildT*	next_child(		const MyLinkT&			CHILD) const
		{
			return nullptr;
		}

		inline void				for_each_child(	const InvokeChild&		INVOKE_CHILD)
		{
			if(has_child()) INVOKE_CHILD(get_child());
		}

		inline void				for_each_child(	const InvokeConstChild&	INVOKE_CHILD) const
		{
			if(has_child()) INVOKE_CHILD(get_child());
		}
	};


	template<typename ChildT, typename... ParentTn>
	class	Parent<ChildT, Bond<MANY_TO_ONE, ParentTn...>> 
		: private dpl::Chain<Parent<ChildT, Bond<MANY_TO_ONE, ParentTn...>>, ChildT, complex::get_bondID<ChildT, Bond<MANY_TO_ONE, ParentTn...>>()>
	{
	private: // subtypes
		using	BondT				= Bond<MANY_TO_ONE, ParentTn...>;

		static const uint32_t BOND_ID = complex::get_bondID<ChildT, BondT>();

		using	MyType				= Parent<ChildT, BondT>;
		using	MyLinkT				= dpl::Link<MyType, ChildT, BOND_ID>;
		using	MyChainT			= dpl::Chain<MyType, ChildT, BOND_ID>;

	public: // subtypes
		using	InvokeChild			= std::function<void(ChildT&)>;
		using	InvokeConstChild	= std::function<void(const ChildT&)>;

	public: // friends
		friend	MyLinkT;
		friend	MyChainT;
		friend	Child<ChildT, BondT>;

		template<typename, dpl::is_TypeList>
		friend class	ParentWithManyChildren;

	private: // lifecycle
		CLASS_CTOR				Parent() = default;

		CLASS_CTOR				Parent(			Parent&&				other) noexcept
			: MyChainT(std::move(other))
		{

		}

		Parent&					operator=(		Parent&&				other) noexcept
		{
			MyChainT::operator=(std::move(other));
			return *this;
		}

		Parent&					operator=(		dpl::Swap<Parent>		other)
		{
			MyChainT::operator=(dpl::Swap<MyChainT>(*other));
			return *this;
		}

	private: // functions
		inline bool				has_child() const
		{
			return MyChainT::size() > 0;
		}

		inline bool				can_have_another_child() const
		{
			return true;
		}

		inline uint32_t			numChildren() const
		{
			return MyChainT::size();
		}

		inline ChildT&			get_child()
		{
			return *MyChainT::first();
		}

		inline const ChildT&	get_child() const
		{
			return *MyChainT::first();
		}

		inline ChildT*			first_child()
		{
			return MyChainT::first();
		}

		inline const ChildT*	first_child() const
		{
			return MyChainT::first();
		}

		inline ChildT*			last_child()
		{
			return MyChainT::last();
		}

		inline const ChildT*	last_child() const
		{
			return MyChainT::last();
		}

		inline ChildT*			previous_child(	MyLinkT&				child)
		{
			if(!child.is_linked(*this)) return nullptr;
			return child.previous();
		}

		inline const ChildT*	previous_child(	const MyLinkT&			CHILD) const
		{
			if(!CHILD.is_linked(*this)) return nullptr;
			return CHILD.previous();
		}

		inline ChildT*			next_child(		MyLinkT&				child)
		{
			if(!child.is_linked(*this)) return nullptr;
			return child.next();
		}

		inline const ChildT*	next_child(		const MyLinkT&			CHILD) const
		{
			if(!CHILD.is_linked(*this)) return nullptr;
			return CHILD.next();
		}

		inline void				for_each_child(	const InvokeChild&		INVOKE_CHILD)
		{
			MyChainT::for_each_link(INVOKE_CHILD);
		}

		inline void				for_each_child(	const InvokeConstChild&	INVOKE_CHILD) const
		{
			MyChainT::for_each_link(INVOKE_CHILD);
		}
	};
}

// child interface
namespace complex
{
	template<typename ChildT, typename... ParentTn>
	class	Child<ChildT, Bond<ONE_TO_ONE, ParentTn...>> 
		: private dpl::Association<ChildT, Parent<ChildT, Bond<ONE_TO_ONE, ParentTn...>>, complex::get_bondID<ChildT, Bond<ONE_TO_ONE, ParentTn...>>()>
	{
	private: // subtypes
		using	BondT					= Bond<ONE_TO_ONE, ParentTn...>;

		static const uint32_t BOND_ID	= complex::get_bondID<ChildT, BondT>();

		using	ParentBaseT				= Parent<ChildT, BondT>;
		using	MyBaseT					= dpl::Association<ChildT, ParentBaseT, BOND_ID>;
		using	MyLinkT					= dpl::Association<ParentBaseT, ChildT, BOND_ID>;

	public: // subtypes
		using	ParentHandleT			= ParentHandle<ChildT, BondT>;
		using	SelectedT				= typename ParentHandleT::TypeWrapper;

	public: // friends
		friend	ParentBaseT;
		friend	MyBaseT;
		friend	MyLinkT;

		template<typename, dpl::is_TypeList>
		friend class	ChildWithManyParents;

		template<typename, dpl::is_TypeList>
		friend class	ParentWithManyChildren;

	public: // data
		dpl::ReadOnly<SelectedT, Child> parentType;

	protected: // lifecycle
		CLASS_CTOR				Child() = default;

		CLASS_CTOR				Child(			Child&&					other) noexcept
			: MyBaseT(std::move(other))
			, parentType(other.parentType)
		{
			other.parentType->invalidate();
		}

		Child&					operator=(		Child&&					other) noexcept
		{
			MyBaseT::operator=(std::move(other));
			parentType		= other.parentType;
			other.parentType->invalidate();
			return *this;
		}

		inline Child&			operator=(		dpl::Swap<Child>		other)
		{
			MyBaseT::operator=(dpl::Swap<MyBaseT>(*other));
			parentType->swap(other->parentType);
			return *this;
		}

	private: // lifecycle (deleted)
		CLASS_CTOR				Child(			const Child&			OTHER) = delete;
		Child&					operator=(		const Child&			OTHER) = delete;

	private: // functions
		inline bool				has_any_parent() const
		{
			return MyBaseT::is_linked();
		}

		template<is_Object ParentT>
		inline bool				has_parent() const
		{
			return parentType().is_same_as<ParentT>() && has_any_parent();
		}

		template<is_Object ParentT>
		inline ParentT&			get_parent()
		{
			parentType().throw_if_different_typeID<ParentT>();
			return static_cast<ParentT&>(*MyBaseT::other());
		}

		template<is_Object ParentT>
		inline const ParentT&	get_parent() const
		{
			parentType().throw_if_different_typeID<ParentT>();
			return static_cast<const ParentT&>(*MyBaseT::other());
		}

		inline void				detach_from_parent()
		{
			if(!MyBaseT::unlink()) return;
			parentType->invalidate();
		}

		inline ParentHandleT	get_parent_handle() const
		{
			const ObjectPack* PACK = ObjectManager::ref().find_base_variant(parentType().ID);
			return PACK? ParentHandleT(parentType(), PACK->guess_object_name<ParentBaseT>(MyBaseT::other())) : ParentHandleT();
		}

		void					set_parent(		const ParentHandleT&	PARENT_HANDLE)
		{
			detach_from_parent();

			if(ObjectPack* pack = ObjectManager::ref().find_base_variant(PARENT_HANDLE.type().ID))
			{
				if(ParentBaseT* parent = PARENT_HANDLE.acquire_parent(*pack))
				{
					if(MyBaseT::link(*parent))
					{
						parentType = PARENT_HANDLE.type;
					}
				}
			}
		}
	};


	template<typename ChildT, typename... ParentTn>
	class	Child<ChildT, Bond<MANY_TO_ONE, ParentTn...>> 
		: private dpl::Link<Parent<ChildT, Bond<MANY_TO_ONE, ParentTn...>>, ChildT, complex::get_bondID<ChildT, Bond<MANY_TO_ONE, ParentTn...>>()>
	{
	private: // subtypes
		using	BondT			= Bond<MANY_TO_ONE, ParentTn...>;

		static const uint32_t BOND_ID	= complex::get_bondID<ChildT, BondT>();

		using	ParentBaseT		= Parent<ChildT, BondT>;
		using	MyLinkT			= dpl::Link<ParentBaseT, ChildT, BOND_ID>;
		using	MyChainT		= dpl::Chain<ParentBaseT, ChildT, BOND_ID>;

	public: // subtypes
		using	ParentHandleT	= ParentHandle<ChildT, BondT>;
		using	SelectedT		= typename ParentHandleT::TypeWrapper;

	public: // friends
		friend	ParentBaseT;
		friend	MyLinkT;
		friend	MyLinkT::MyBase;
		friend	MyChainT;

		template<typename, dpl::is_TypeList>
		friend class	ChildWithManyParents;

		template<typename, dpl::is_TypeList>
		friend class	ParentWithManyChildren;

	public: // data
		dpl::ReadOnly<SelectedT, Child> parentType;

	protected: // lifecycle
		CLASS_CTOR				Child() = default;

		CLASS_CTOR				Child(			Child&&					other) noexcept
			: MyLinkT(std::move(other))
			, parentType(other.parentType)
		{
			other.parentType->invalidate();
		}

		Child&					operator=(		Child&&					other) noexcept
		{
			MyLinkT::operator=(std::move(other));
			parentType		= other.parentType;
			other.parentType->invalidate();
			return *this;
		}

		inline Child&			operator=(		dpl::Swap<Child>		other)
		{
			MyLinkT::operator=(dpl::Swap<MyLinkT>(*other));
			parentType->swap(other->parentType);
			return *this;
		}

	private: // lifecycle (deleted)
		CLASS_CTOR				Child(			const Child&			other) = delete;
		Child&					operator=(		const Child&			other) = delete;

	private: // functions
		inline bool				has_any_parent() const
		{
			return MyLinkT::is_linked();
		}

		template<is_Object ParentT>
		inline bool				has_parent() const
		{
			return parentType().is_same_as<ParentT>() && has_any_parent();
		}

		template<is_Object ParentT>
		inline ParentT&			get_parent()
		{
			parentType().throw_if_different_typeID<ParentT>();
			return static_cast<ParentT&>(*MyLinkT::get_group());
		}

		template<is_Object ParentT>
		inline const ParentT&	get_parent() const
		{
			parentType().throw_if_different_typeID<ParentT>();
			return static_cast<const ParentT&>(*MyLinkT::get_group());
		}

		inline void				detach_from_parent()
		{
			MyLinkT::detach();
			parentType->invalidate();
		}

		inline ParentHandleT	get_parent_handle() const
		{
			const ObjectPack* PACK = ObjectManager::ref().find_base_variant(parentType().ID);
			return PACK? ParentHandleT(parentType(), PACK->guess_object_name<ParentBaseT>(MyLinkT::get_group())) : ParentHandleT();
		}

		void					set_parent(		const ParentHandleT&	PARENT_HANDLE)
		{
			detach_from_parent();

			if(ObjectPack* pack = ObjectManager::ref().find_base_variant(PARENT_HANDLE.type().ID))
			{
				if(ParentBaseT* parent = PARENT_HANDLE.acquire_parent(*pack))
				{
					if(parent->attach_back(*this))
					{
						parentType = PARENT_HANDLE.type;
					}
				}
			}
		}
	};
}

// multichild definitions
namespace complex
{
	template<typename ChildT, is_Bond... BondTn>
	class	ChildWithManyParents<ChildT, dpl::TypeList<BondTn...>> : public Child<ChildT, BondTn>...
	{
	private: // subtypes
		using	BOND_TYPES		= dpl::TypeList<BondTn...>;
		using	PARENT_TYPES	= dpl::merge_t<typename BondTn::Alternative...>;

		template<dpl::is_one_of<PARENT_TYPES> ParentT>
		using	BaseT			= Child<ChildT, typename RelatedPair<ParentT, ChildT>::BondT>;

	public: // lifecycle
		CLASS_CTOR				ChildWithManyParents() = default;

		CLASS_CTOR				ChildWithManyParents(	ChildWithManyParents&&			other) noexcept = default;

		ChildWithManyParents&	operator=(				ChildWithManyParents&&			other) noexcept = default;

		ChildWithManyParents&	operator= (				dpl::Swap<ChildWithManyParents>	other)
		{
			(Child<ChildT, BondTn>::operator=(dpl::Swap<Child<ChildT, BondTn>>(*other)), ...);
			return *this;
		}

	private: // lifecycle
		CLASS_CTOR				ChildWithManyParents(	const ChildWithManyParents&		OTHER) = delete;

		ChildWithManyParents&	operator=(				const ChildWithManyParents&		OTHER) = delete;

	public: // functions
		template<dpl::is_one_of<PARENT_TYPES> ParentT>
		inline bool				has_parent() const
		{
			return BaseT<ParentT>::template has_parent<ParentT>();
		}

		template<dpl::is_one_of<PARENT_TYPES> ParentT>
		inline ParentT&			get_parent()
		{
			return BaseT<ParentT>::template get_parent<ParentT>();
		}

		template<dpl::is_one_of<PARENT_TYPES> ParentT>
		inline const ParentT&	get_parent() const
		{
			return BaseT<ParentT>::template get_parent<ParentT>();
		}

	protected: // functions
		inline bool				orphan(					dpl::CommandInvoker&			invoker)
		{
			return invoker.invoke<Orphan>(nullptr, *this);
		}
	};


	// Specialization for child without parents.
	template<typename ChildT>
	class	ChildWithManyParents<ChildT, dpl::TypeList<>>{};
}

// multiparent definitions
namespace complex
{
	template<typename ParentT, typename... ChildTn>
	class	ParentWithManyChildren<ParentT, dpl::TypeList<ChildTn...>> : public RelatedPair<ParentT, ChildTn>::ParentBaseT...
	{
	public: // subtypes
		using	CHILD_TYPES		= dpl::TypeList<ChildTn...>;

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		using	Invoke			= std::function<void(ChildT&)>;

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		using	InvokeConst		= std::function<void(const ChildT&)>;

	private: // subtypes
		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		using	ParentBase_of	= typename RelatedPair<ParentT, ChildT>::ParentBaseT;

	public: // friends
		template<typename, is_Bond>
		friend	class Child;

	protected: // lifecycle
		CLASS_CTOR				ParentWithManyChildren() = default;

		CLASS_CTOR				ParentWithManyChildren(		ParentWithManyChildren&&				other) noexcept = default;

		ParentWithManyChildren&	operator=(					ParentWithManyChildren&&				other) noexcept = default;

		ParentWithManyChildren&	operator=(					dpl::Swap<ParentWithManyChildren>		other)
		{
			(ParentBase_of<ChildTn>::operator=(dpl::Swap<ChildTn>(*other)), ...);
			return *this;
		}

	public: // functions
		template<is_Object ChildT>
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
		inline ChildT*			previous_child(				ChildT&						child)
		{
			return ParentBase_of<ChildT>::previous_child(child);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline const ChildT*	previous_child(				const ChildT&				CHILD) const
		{
			return ParentBase_of<ChildT>::previous_child(CHILD);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline ChildT*			next_child(					ChildT&						child)
		{
			return ParentBase_of<ChildT>::next_child(child);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline const ChildT*	next_child(					const ChildT&				CHILD) const
		{
			return ParentBase_of<ChildT>::next_child(CHILD);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline void				for_each_child(				const Invoke<ChildT>&		INVOKE_CHILD)
		{
			ParentBase_of<ChildT>::for_each_child(INVOKE_CHILD);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline void				for_each_child(				const InvokeConst<ChildT>&	INVOKE_CHILD) const
		{
			ParentBase_of<ChildT>::for_each_child(INVOKE_CHILD);
		}

	protected: // functions
		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline bool				adopt(						ChildT&						child,
															dpl::CommandInvoker&		invoker)
		{
			return invoker.invoke<Adopt<ChildT>>(nullptr, static_cast<Object<ParentT>&>(*this), child);
		}

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		inline bool				destroy_children_of_type(	dpl::CommandInvoker&		invoker)
		{
			return invoker.invoke<DestroyChildren_of_type<ChildT>>(nullptr, static_cast<Object<ParentT>&>(*this));
		}

		inline bool				destroy_all_children(		dpl::CommandInvoker&		invoker)
		{
			return invoker.invoke<DestroyAllChildren>(nullptr, static_cast<Object<ParentT>&>(*this));
		}
	};


	// Specialization of parent without children.
	template<typename ParentT>
	class	ParentWithManyChildren<ParentT, dpl::TypeList<>>{};
}

// class implementation
namespace complex
{
	template<typename ObjT>
	class	NonPolymorphic
	{
	public: // functions
		inline uint32_t		typeID()
		{
			return ObjectPack::get_typeID<ObjectPack_of<ObjT>>();
		}
	};

	template<typename ObjT>
	class	Polymorphic
	{
	public: 
		inline uint32_t		typeID()
		{
			return ObjectPack::get_typeID<ObjectPack_of<ObjT>>();
		}
	};

	template<typename ObjT>
	using	Form = std::conditional_t<std::is_final_v<ObjT>(), Polymorphic<ObjT>, NonPolymorphic<ObjT>>;
}

// occurrence implementation
namespace complex
{
	/*
		Base type of the object that cannot be instanced.
	*/
	template<typename ObjT>
	class	Unique
	{
	public: // subtypes
		using State		= dpl::EmptyCommand;
		/*
			[TODO]: Create dummy state that mimics InstancePack::AttachmentOperation.
		*/
	};

	/*
		Base type of the instantiable object.

		TODO: Undo/Redo
	*/
	template<typename ObjT>
	class	Serial : public InstancePack_of<ObjT>
	{
	public: // subtypes
		using State		= InstanceRow::AttachmentOperation;
	};

	/*
		Selects Unique or Serial base.
	*/
	template<typename ObjT>
	using	Occurring = std::conditional_t<complex::is_instantiable<ObjT>(), Serial<ObjT>, Unique<ObjT>>;
}

// composition implementation
namespace complex
{
	/*
		Base type of the object with no components.
	*/
	template<typename ObjT>
	class	Pure
	{

	};

	/*
		Base type of the object with additional components that are stored separately.

		TODO: Undo/Redo
	*/
	template<typename ObjT>
	class	Compound
	{
	public: // subtypes
		using	COMPONENT_TYPES		= typename Traits<ObjT>::ComponentTypes;
		using	Table				= ComponentTable<ObjT, ComponentTypes_of<ObjT>>;
		using	Column				= typename Table::Column;
		using	ConstColumn			= typename Table::ConstColumn;

	public: // component access
		template<dpl::is_one_of<COMPONENT_TYPES> T>
		inline T&					get_component()
		{
			ObjectPack_of<ObjT>& pack = ObjectPack_of<ObjT>::ref();
			return *pack.column<T>().at(get_index(pack));
		}

		template<dpl::is_one_of<COMPONENT_TYPES> T>
		inline const T&				get_component() const
		{
			const ObjectPack_of<ObjT>& PACK = ObjectPack_of<ObjT>::ref();
			return *PACK.column<T>().at(get_index(PACK));
		}

		inline Column				get_all_components()
		{
			ObjectPack_of<ObjT>& pack = ObjectPack_of<ObjT>::ref();
			return pack.column(get_index(pack));
		}

		inline ConstColumn			get_all_components() const
		{
			const ObjectPack_of<ObjT>& PACK = ObjectPack_of<ObjT>::ref();
			return PACK.column(get_index(PACK));
		}

	private: // functions
		inline const uint32_t		get_index(const ObjectPack_of<ObjT>& PACK) const
		{
			return PACK.index_of(static_cast<ObjT*>(this));
		}
	};

	/*
		Selects Pure or Compound base.
	*/
	template<typename ObjT>
	using	Composition = std::conditional_t<complex::is_composite<ObjT>(), Compound<ObjT>, Pure<ObjT>>;
}

// name
namespace complex
{
	class	Name
	{
	public: // subtypes
		enum	Type
		{
			UNIQUE, // Object will be named with exact name given by the user.
			GENERIC // Object name will be appended with generic postfix.
		};

	private: // data
		Type		m_type;
		std::string	m_str;

	private: // lifecycle
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
}

// object implementation
namespace complex
{
	/*
		Specialize complex::Traits<ObjT> to set types of: parents, children and per object components.
		Specialize complex::Instance<ObjT> to indicate that this object type is instantiable(look@ complex_Instances.h).
		Specialize complex::EntityState<ObjT> to preserve state of ObjT data and components. 
		Name of the object, attachment of instances, links to parents and state of the children are already preserved within internal state of the Object<ObjT> class.
	 
		[TODO]:
		- Finish commands
		- Finish instances

		[PROBLEM]: 
		- Saving state of the Object

		[SOLUTION]:
		- Prevent user form adding data to the class derived from object
		- Force use of components
	*/
	template<typename ObjT>
	class	Object	: private dpl::Labelable<char>
					, public dpl::NamedType<ObjT> // <-- Cannot be part of this class!!! (due to inheritance)
					, public ChildWithManyParents<ObjT, ParentTypes_of<ObjT>>
					, public ParentWithManyChildren<ObjT, ChildTypes_of<ObjT>>
					, public Form<ObjT>
					, public Occurring<ObjT>
					, public Composition<ObjT>
	{
	private: // subtypes
		using	Labelable_t	= dpl::Labelable<char>;

	public: // friends
		friend	ObjectPack_of<ObjT>;

	public: // inherited functions
		using	Labelable_t::get_label;

	public: // lifecycle
		CLASS_CTOR					Object(			const Name&				NAME)
		{
			if(NAME.is_unique())	ObjectManager::ref().m_labeler.label(*this, NAME);
			else					ObjectManager::ref().m_labeler.label_with_postfix(*this, NAME);
		}

		CLASS_CTOR					Object(			Object&&				other) noexcept = default;
		Object&						operator=(		Object&&				other) noexcept = default;

	private: // lifecycle(deleted)
		CLASS_CTOR					Object(			const Object&			OTHER) = delete;
		Object&						operator=(		const Object&			OTHER) = delete;

	public: // functions
		inline const std::string&	name() const
		{
			return Labelable_t::get_label();
		}

	protected: // functions
		inline bool					rename(			const std::string&		NEW_NAME,
													const Name::Type		NAME_TYPE,
													dpl::CommandInvoker&	invoker) const
		{
			return invoker.invoke<Rename>(*this, NEW_NAME, NAME_TYPE);
		}

		inline bool					selfdestruct(	dpl::CommandInvoker&	invoker)
		{
			return invoker.invoke<DestroyCommand>(*this);
		}
	};
}

// commands
namespace complex
{
	template<typename T>
	concept is_Finished = std::is_final_v<T>;


	template<is_Finished EntityT>
	struct	EntityCommands
	{
		using	BOND_TYPES	= ParentTypes_of<EntityT>;
		using	CHILD_TYPES	= ChildTypes_of<EntityT>;

		class	Create : public dpl::Command
		{
		public: // data
			dpl::ReadOnly<EntityT*, Create> obj; // DO NOT STORE! Invalidated with the next command.

		private: // data
			Name m_name;
			// [TODO]: We absolutely have to save the state on unexecute!!!

		public: // lifecycle
			CLASS_CTOR		Create(	const Name::Type	NAME_TYPE,
									const std::string&	NAME)
				: m_name(NAME_TYPE, NAME)
				, obj(nullptr)
			{

			}

		private: // implementation
			virtual bool	valid() const
			{
				if(ObjectPack_of<T>::ref().find(m_name)) return dpl::Logger::ref().push_error("Object with that name already exists: " + m_name);
				return true;
			}

			virtual void	execute() final override
			{
				EntityT& object = ObjectPack_of<EntityT>::ref().create(m_name);
				obj = &object;
				if(m_name.is_generic()) m_name.change(UNIQUE_NAME, object.name());
			}

			virtual void	unexecute() final override
			{
				EntityT& object = ObjectPack_of<EntityT>::ref().get(m_name);
				ObjectPack_of<EntityT>::ref().destroy(object);
			}
		};

		class	Destroy : public dpl::Command
		{
		private: // data
			std::string			m_name;
			Orphan				m_orphan; //<-- TODO: Internal logic of the Orphan command can be extracted into state to prevent duplication of the name.
			DestroyAllChildren	m_destroyAllChildren;
			Occurrence			m_occurrence;
			// [TODO]: Composition command

		public: // lifecycle
			CLASS_CTOR		Destroy(	Object<EntityT>&	object)
				: m_name(object.name())
				, m_orphan(object)
				, m_destroyAllChildren(object)
			{
				/*
					[TODO]: Initialize m_occurrance!!!
				*/
			}

		private: // implementation
			virtual void	execute() final override
			{
				auto& object = ObjectPack_of<EntityT>::ref().get(m_name);
				m_occurrence.detach(object);
				// [TODO]: store Composition state
				static_cast<dpl::Command&>(m_destroyAllChildren).execute();
				static_cast<dpl::Command&>(m_orphan).execute();
				ObjectPack_of<EntityT>::ref().destroy(object);
			}

			virtual void	unexecute() final override
			{
				EntityT& object = ObjectPack_of<EntityT>::ref().create(m_name);
				m_occurrence.attach(object);
				// [TODO]: restore Composition state
				static_cast<dpl::Command&>(m_orphan).unexecute();
				static_cast<dpl::Command&>(m_destroyAllChildren).unexecute();
			}
		};

		class	Rename : public dpl::Command
		{
		private: // data
			std::string m_oldName;
			std::string m_newName;
			Name::Type	m_type;

		public: // lifecycle
			CLASS_CTOR		Rename(	const std::string&	OLD_NAME,
									const std::string&	NEW_NAME,
									const Name::Type	NAME_TYPE)
				: m_oldName(OLD_NAME)
				, m_newName(NEW_NAME)
				, m_type(NAME_TYPE)
			{

			}

			CLASS_CTOR		Rename(	const Object<T>&	OBJECT,
									const std::string&	NEW_NAME,
									const Name::Type	NAME_TYPE)
				: Rename(OBJECT.name(), NEW_NAME, NAME_TYPE)
			{

			}

		private: // interface
			virtual bool	valid() const
			{
				return !ObjectPack_of<T>::ref().find(m_oldName); // TODO: Log failure.
			}

			virtual void	execute() final override
			{
				Object<EntityT>&	object = ObjectPack_of<EntityT>::ref().get(m_oldName);
				if(m_type == Name::Type::GENERIC)
				{
					object.change_to_generic_label(m_newName);
					m_newName	= object.get_label();
					m_type		= Name::Type::UNIQUE;
				}
				else
				{
					object.change_label(m_newName);
				}
			}

			virtual void	unexecute() final override
			{
				Object<EntityT>&	object = ObjectPack_of<EntityT>::ref().get(m_newName);
									object.change_label(m_oldName);
			}
		};


		// Break bond with a single parent.
		template<dpl::is_one_of<BOND_TYPES> T>
		class	Estrange : public dpl::Command
		{
		public: // data
			dpl::ReadOnly<std::string,				Estrange> childName;
			dpl::ReadOnly<ParentHandle<EntityT, T>,	Estrange> parent;

		private: // lifecycle
			CLASS_CTOR			Estrange(	Child<EntityT, T>&	child,
											const std::string&	CHILD_NAME)
				: childName(CHILD_NAME)
				, parent(child.get_parent_handle())
			{
				
			}

		public: // lifecycle
			CLASS_CTOR			Estrange(	Object<EntityT>&	child)
				: Estrange(child, child.name())
			{

			}

		private: // implementation
			virtual bool		valid() const
			{
				return parent().valid(); // TODO: log already estranged on failure
			}

			virtual void		execute() final override
			{
				if(!parent().valid()) return;
				Child<EntityT, T>&	child = ObjectPack_of<EntityT>::ref().get(childName);
									child.set_parent(ParentHandle<EntityT, T>());
			}

			virtual void		unexecute() final override
			{
				if(!parent().valid()) return;
				Child<EntityT, T>&	child = ObjectPack_of<EntityT>::ref().get(childName);
									child.set_parent(parent);
			}
		};


		// Break bond with all parents.
		class	Orphan : dpl::Command
		{
		private: // subtypes
			template<typename BondT>
			using	ParentHandleT = typename Child<EntityT, BondT>::ParentHandleT;
			using	ParentHandles = std::tuple<ParentHandleT<BondTn>...>;

		public: // data
			dpl::ReadOnly<std::string,		Orphan> childName;
			dpl::ReadOnly<ParentHandles,	Orphan> parents;

		public: // lifecycle
			CLASS_CTOR			Orphan(		Object<EntityT>&	child)
				: childName(child.name())
			{
				(Orphan::set_handle<BondTn>(child), ...);
			}

		private: // implementation
			virtual bool		valid() const
			{
				return (... || std::get<ParentHandleT<BondTn>>(*parents).valid());
			}

			virtual void		execute() final override
			{
				EntityT& child = ObjectPack_of<EntityT>::ref().get(childName);
				(child.Child<EntityT, BondTn>::set_parent(ParentHandle<ChildT, BondTn>()), ...);
			}

			virtual void		unexecute() final override
			{
				EntityT& child = ObjectPack_of<EntityT>::ref().get(childName);
				(child.Child<EntityT, BondTn>::set_parent(std::get<ParentHandleT<BondTn>>(*parents)), ...);
			}

		private: // functions
			template<is_Bond T>
			inline void			set_handle(	Child<EntityT, T>&	child)
			{
				std::get<ParentHandleT<T>>(*parents) = Child<EntityT, T>.get_parent_handle();
			}
		};

	public: // commands
		class	DestroyAllChildren;

	private: // commands
		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		class	IDestroyChildren_of_type
		{
		public: // friends
			friend	DestroyAllChildren;

		private: // subtypes
			using	DestroyChild =  typename EntityCommands<ChildT>::Destroy;

		private: // data
			std::vector<DestroyChild>	m_commands;

		protected: // functions
			void	execute_internal(	ParentWithManyChildren& parent)
			{
				if(!parent.has_child<ChildT>()) return;

				if(m_commands.empty())
				{
					m_commands.reserve(parent.numChildren<ChildT>());
					parent.for_each_child<ChildT>([&](Object<ChildT>& child)
					{
						m_commands.emplace_back(child);
					});
				}
				
				for(dpl::Command& command : m_commands)
				{
					command.execute();
				}
			}

			void	unexecute_internal(	ParentWithManyChildren& parent)
			{
				for(auto it = m_commands.rbegin(); it != m_commands.rend(); ++it)
				{
					dpl::Command&	command = *it;
									command.unexecute();
				}
			}
		};

	public: // commands
		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		class	Adopt : public dpl::Command
		{
		private: // subtypes
			using	ChildBaseT		= typename RelatedPair<EntityT, ChildT>::ChildBaseT;
			using	ParentHandleT	= typename ChildBaseT::ParentHandleT;

		public: // data
			dpl::ReadOnly<std::string,		Adopt> childName;
			dpl::ReadOnly<ParentHandleT,	Adopt> oldParent;
			dpl::ReadOnly<ParentHandleT,	Adopt> newParent;

		private: // lifecycle
			CLASS_CTOR			Adopt(	const std::string&		CHILD_NAME,
										ChildBaseT&				child,
										Object<EntityT>&		parent)
				: childName(CHILD_NAME)
				, oldParent(child.get_parent_handle())
				, newParent(parent)
			{
				
			}

		public: // lifecycle
			CLASS_CTOR			Adopt(	Object<EntityT>&		parent,
										Object<ChildT>&			child)
				: Adopt(child.name(), child, parent)
			{

			}

		private: // implementation
			virtual bool		valid() const
			{
				return oldParent() != newParent(); // TODO: Log that parent is the same.
			}

			virtual void		execute() final override
			{
				ChildBaseT&	child = ObjectPack_of<ChildT>::ref().get(childName);
							child.set_parent(newParent);
			}

			virtual void		unexecute() final override
			{
				ChildBaseT&	child = ObjectPack_of<ChildT>::ref().get(childName);
							child.set_parent(oldParent);
			}
		};

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		class	Abandon; // TODO

		template<dpl::is_one_of<CHILD_TYPES> ChildT>
		class	DestroyChildren_of_type : public dpl::Command
										, public IDestroyChildren_of_type<ChildT>
		{
		private: // data
			std::string m_name;

		public: // lifecycle
			CLASS_CTOR			DestroyChildren_of_type(Object<EntityT>& parent)
				: m_name(parent.name())
			{
			}

		private: // interface
			virtual bool		valid() const
			{
				ParentWithManyChildren& parent = ObjectPack_of<EntityT>::ref().get(m_name);
				return parent.numChildren<ChildT>() > 0; // TODO: Log "no children of that type" on failure.
			}

			virtual void		execute() final override
			{
				IDestroyChildren_of_type<ChildT>::execute_internal(ObjectPack_of<EntityT>::ref().get(m_name));
			}

			virtual void		unexecute() final override
			{
				IDestroyChildren_of_type<ChildT>::unexecute_internal(ObjectPack_of<EntityT>::ref().get(m_name));
			}
		};

		class	DestroyAllChildren : public dpl::Command
		{
		private: // data
			std::string											m_name;
			std::tuple<IDestroyChildren_of_type<ChildTn>...>	m_subcommands;

		public: // lifecycle
			CLASS_CTOR			DestroyAllChildren(	Object<EntityT>& parent)
				: m_name(parent.name())
			{
				// Since child may be owned by more than one parent, we cannot generate all commands in the constructor,
				// because that would duplicate state of those children.
			}

		private: // interface
			virtual bool		valid() const
			{
				ParentWithManyChildren& parent = ObjectPack_of<EntityT>::ref().get(m_name);
				return (... || parent.has_child<ChildTn>());
			}

			virtual void		execute() final override
			{
				ParentWithManyChildren& parent = ObjectPack_of<EntityT>::ref().get(m_name);
				(std::get<IDestroyChildren_of_type<ChildTn>>(m_subcommands).execute_internal(parent), ...);
			}

			virtual void		unexecute() final override
			{
				ParentWithManyChildren& parent = ObjectPack_of<EntityT>::ref().get(m_name);
				(std::get<IDestroyChildren_of_type<ChildTn>>(m_subcommands).unexecute_internal(parent), ...);
			}
		};
	};
}

// objects storage
namespace complex
{
	class	ObjectManager	: public dpl::Singleton<ObjectManager>
							, public dpl::Variation<ObjectManager, ObjectPack>
							, private dpl::StaticHolder<std::string, ObjectManager>
	{
	public: // friends
		template<typename>
		friend class Object;

		template<typename>
		friend class ObjectPack_of;

	private: // data
		dpl::Labeler<char> m_labeler;

	public: // functions
		static inline const std::string&	invalid_name()
		{
			return StaticHolder::data;
		}

		inline InstanceRow*				find_instance_pack(	const InstanceRow::Handle& HANDLE)
		{
			auto* objectPack = Variation::find_base_variant(HANDLE.typeName);
			return objectPack? objectPack->get_object_as_instance_pack(HANDLE.packName) : nullptr;
		}

		inline InstanceRow&				get_instance_pack(	const InstanceRow::Handle& HANDLE)
		{
			return *find_instance_pack(HANDLE);
		}

	public: // functions
		template<is_Object T>
		inline bool							create_pack()
		{
			return Variation::create_variant<ObjectPack_of<T>>();
		}
	};


	class	Dummy{};


	template<typename T>
	using	ComponentTableBase = std::conditional_t<complex::is_composite<T>(), ComponentTable<T, ComponentTypes_of<T>>, Dummy>;


	/*
		Objects are stored in a contiguous array, but they order is undefined and can change on erasure.
	*/
	template<typename ObjT>
	class	ObjectPack_of	: public ObjectPack
							, public dpl::Singleton<ObjectPack_of<ObjT>>
							, public ComponentTableBase<ObjT>

	{
	private: // subtypes
		using	MyTableBase				= ComponentTableBase<ObjT>;

	public: // subtypes
		using	Name					= typename Object<ObjT>::Name;
		using	EntityIndex				= uint32_t;
		using	NumEntities				= uint32_t;
		using	Invoke				= std::function<void(ObjT&)>;
		using	InvokeAll				= std::function<void(ObjT*, NumEntities)>;
		using	InvokeConst			= std::function<void(const ObjT&)>;
		using	InvokeAllConst			= std::function<void(const ObjT*, NumEntities)>;
		using	InvokeIndexed		= std::function<void(ObjT&, EntityIndex)>;
		using	InvokeIndexedConst	= std::function<void(const ObjT&, EntityIndex)>;

	public: // commands
		class	DestroyAll; //<-- TODO

	public: // friends
		friend  Object<ObjT>;

	private: // data
		std::vector<ObjT> m_objects;

	private: // lifecycle
		CLASS_CTOR					ObjectPack_of(				const Binding&				BINDING)
			: ObjectPack(BINDING)
		{

		}

	public: // query functions
		inline uint32_t				size() const
		{
			return (uint32_t)m_objects.size();
		}

		inline uint32_t				index_of(					const ObjT*						OBJECT) const
		{
			const uint64_t INDEX64 = dpl::get_element_index(m_objects, OBJECT);
			return (INDEX64 > ObjectPack::INVALID_INDEX) ? ObjectPack::INVALID_INDEX : (uint32_t)INDEX64;
		}

		inline bool					contains(					const uint32_t					OBJECT_INDEX) const
		{
			return OBJECT_INDEX < size();
		}

		inline bool					contains(					const ObjT*						OBJECT) const
		{
			return ObjectPack_of::contains(index_of(OBJECT));
		}

		inline ObjT*				find(						const std::string&				NAME)
		{
			ObjT* object = static_cast<ObjT*>(ObjectManager::ref().m_labeler.find_entry(NAME));
			return ObjectPack_of::contains(object)? object : nullptr;
		}

		inline const ObjT*			find(						const std::string&				NAME) const
		{
			const ObjT* OBJECT = static_cast<const ObjT*>(ObjectManager::ref().m_labeler.find_entry(NAME));
			return ObjectPack_of::contains(OBJECT)? OBJECT : nullptr;
		}

		inline ObjT&				get(						const std::string&				NAME)
		{
			return *find(NAME);
		}

		inline const ObjT&			get(						const std::string&				NAME) const
		{
			return *find(NAME);
		}

		inline ObjT&				get(						const uint32_t					INDEX)
		{
			return m_objects[INDEX];
		}

		inline const ObjT&			get(						const uint32_t					INDEX) const
		{
			return m_objects[INDEX];
		}

	public: // iteration functions
		inline void					for_each(					const Invoke&				INVOKE)
		{
			std::for_each(m_objects.begin(), m_objects.end(), INVOKE);
		}

		inline void					for_each(					const InvokeAll&				INVOKE)
		{
			INVOKE(m_objects.data(), size());
		}

		inline void					for_each(					const InvokeConst&			INVOKE) const
		{
			std::for_each(m_objects.begin(), m_objects.end(), INVOKE);
		}

		inline void					for_each(					const InvokeAllConst&			INVOKE) const
		{
			INVOKE(m_objects.data(), size());
		}

		inline void					for_each(					const InvokeIndexed&			INVOKE)
		{
			const uint32_t SIZE = size();
			for(uint32_t index = 0; index < SIZE; ++index)
			{
				INVOKE(m_objects[index], index);
			}
		}

		inline void					for_each(					const InvokeIndexedConst&	INVOKE)
		{
			const uint32_t SIZE = size();
			for(uint32_t index = 0; index < SIZE; ++index)
			{
				INVOKE(m_objects[index], index);
			}
		}

	private: // command functions
		inline ObjT&				create(						const Name&						NAME)
		{
			if constexpr (complex::is_composite<ObjT>()) MyTableBase::add_row();
			return m_objects.emplace_back(NAME);
		}

		inline void					destroy(					const ObjT&						OBJECT)
		{
			const uint64_t OBJECT_INDEX = dpl::get_element_index(m_objects, OBJECT);
			dpl::fast_remove(m_objects, m_objects.begin() + OBJECT_INDEX);
			if constexpr (complex::is_composite<ObjT>()) MyTableBase::remove_row((uint32_t)OBJECT_INDEX);
		}

	private: // interface
		virtual const std::string&	guess_name_from_byte(		const char*						OBJECT_MEMBER_PTR) const final override
		{
			static const uint64_t		STRIDE			= sizeof(ObjT);
			const char*					BEGIN			= reinterpret_cast<const char*>(m_objects.size());
			const uint64_t				TOTAL_NUM_BYTES = STRIDE * m_objects.size();
			const uint64_t				BYTE_OFFSET		= OBJECT_MEMBER_PTR - BEGIN;
			const char*					END				= BEGIN + TOTAL_NUM_BYTES;

			if(OBJECT_MEMBER_PTR < BEGIN || OBJECT_MEMBER_PTR > END) return ObjectManager::ref().invalid_name();
			const Object<ObjT>& OBJECT = get((uint32_t)(BYTE_OFFSET / STRIDE));
			return OBJECT.name();
		}

		virtual InstanceRow*		get_object_as_instance_pack(const std::string&				OBJECT_NAME) final override
		{
			if constexpr (std::is_base_of_v<InstanceRow, ObjT>) return find(OBJECT_NAME);
			else return nullptr;
		}

		virtual char*				get_object_as_bytes(		const std::string&				OBJECT_NAME) final override
		{
			Object<ObjT>* object = find(OBJECT_NAME);
			return object? reinterpret_cast<char*>(object) : nullptr;
		}
	};
}

#pragma pack(pop)