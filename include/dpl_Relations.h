#pragma once


#include <variant>
#include <array>
#include "dpl_Chain.h"
#include "dpl_TypeTraits.h"
#include "dpl_ReadOnly.h"
#include "dpl_GeneralException.h"

#pragma warning (disable : 26812)

namespace dpl
{
#pragma pack(push, 4)
	enum	RelationPattern
	{
		RELATION_COMMON,		// Child is shared between all parents in the ParentList.
		RELATION_ONELING,		// The same as COMMON, but each parent can have only one instance of given child type.
		RELATION_SELECTIVE,		// Child belongs to one of the parents in the ParentList.
		RELATION_INDEPENDENT	// Child may or may not have a parent. Type of the parent is unknown.
	};

	template <typename ParentT, typename ChildT, bool bHAS_ONELING>
	class	IParent;

	template <typename ParentT, typename ChildTs, RelationPattern PARENT_PATTERN>
	class	Parent;

	template <typename ParentT, typename ChildT, bool bIS_ONELING, uint32_t PID>
	class	IChild;

	template <typename ParentTs, typename ChildT, RelationPattern CHILD_PATTERN>
	class	Child;

	template<typename ChildT>
	class	ChosenParent;

	/*
		Encapsulates parent-child relation.
		Order of child types affects dependency graph and may cause compile-time errors.
		Make sure parents occur before children in the graph.  
	*/
	template<typename RelatedT, typename ParentTs, typename ChildTs, RelationPattern _PATTERN>
	class	Related;

	template<typename... Ts>
	using	ParentTypes	= dpl::TypeList<Ts...>;

	template<typename... Ts>
	using	ChildTypes	= dpl::TypeList<Ts...>;

	using	NoChildren	= ChildTypes<>;

	using	AnyParent	= ParentTypes<>;

	template<typename ParentT, typename ChildT, typename ParentListT, bool bANY_PARENT>
	constexpr uint32_t	get_parent_index()
	{
		if constexpr (bANY_PARENT)
			return 0;
		else	
			return ParentListT::template Type<ParentT>::INDEX;
	}

	constexpr bool		has_ChosenParent(	const RelationPattern PATTERN)
	{
		return (PATTERN == RelationPattern::RELATION_SELECTIVE) || (PATTERN == RelationPattern::RELATION_INDEPENDENT);
	}

	template<typename ParentT, typename ChildT, typename ParentListT, RelationPattern _PATTERN>
	using	ChildBase	= IChild<ParentT, ChildT, _PATTERN == RelationPattern::RELATION_ONELING, dpl::get_parent_index<ParentT, ChildT, ParentListT, false>()>; //<-- DO NOT test ChildT::PATTERN here!!!

	
	template<typename ParentT, typename ChildT>
	constexpr uint32_t	get_parent_index() //<-- This function cannot be used from IChild/Child class!!!
	{
		if constexpr(std::is_same_v<ParentT, ChildT>)
		{
			return 0;
		}
		else
		{
			return dpl::get_parent_index<ParentT, ChildT, typename ChildT::MyParentTypes, dpl::has_ChosenParent(ChildT::PATTERN)>();
		}
	}

	template<typename ParentT, typename ChildT, RelationPattern PARENT_PATTERN, bool bTREE_NODE>
	struct	IParentInfo;

	template<typename ParentT, typename ChildT, RelationPattern PARENT_PATTERN>
	struct	IParentInfo<ParentT, ChildT, PARENT_PATTERN, false>
	{
		using Type				= std::conditional_t<dpl::has_ChosenParent(ChildT::PATTERN), ChosenParent<ChildT>, IParent<ParentT, ChildT, ChildT::PATTERN == RelationPattern::RELATION_ONELING>>;
		using ChildSignature	= typename ChildT::Signature;
	};

	template<typename ParentT, typename ChildT, RelationPattern PARENT_PATTERN>
	struct	IParentInfo<ParentT, ChildT, PARENT_PATTERN, true>
	{
		using Type				= IParent<ParentT, ChildT, PARENT_PATTERN == RelationPattern::RELATION_ONELING>;
		using ChildSignature	= ChildTypes<>;
	};

	template<typename ParentT, typename ChildT, RelationPattern PARENT_PATTERN>
	using	ParentBase	= typename IParentInfo<ParentT, ChildT, PARENT_PATTERN, std::is_same_v<ParentT, ChildT>>::Type;


	/*
		Child type interface.
		- 24B overhead
	*/
	template <typename ParentT, typename ChildT, uint32_t PID>
	class	IChild<ParentT, ChildT, false, PID> : private dpl::Link<ParentT, ChildT, PID>
	{
	private: // subtypes
		using MyParent				= IParent<ParentT, ChildT, false>;
		using MyChain				= dpl::Chain<ParentT, ChildT, PID>;
		using MyLinkBase			= dpl::Link<ParentT, ChildT, PID>;
		using MySequenceableBase	= dpl::Sequenceable<ChildT, PID>;

	public: // relations
		friend MyChain;
		friend MyLinkBase;
		friend MyParent;
		friend MySequenceableBase;

		template<typename, typename, RelationPattern>
		friend class Child;

	protected: // lifecycle
		CLASS_CTOR				IChild(		MyParent*			parent)
		{
			if(parent) parent->add_child(static_cast<ChildT&>(*this));
		}

		CLASS_CTOR				IChild(		const IChild&		OTHER) = delete;
		CLASS_CTOR				IChild(		IChild&&			other) noexcept = default;

		IChild&					operator=(	const IChild&		OTHER) = delete;
		IChild&					operator=(	IChild&&			other) noexcept = default;

		IChild&					operator=(	dpl::Swap<IChild>	other) noexcept
		{
			MyLinkBase::operator=(dpl::Swap<MyLinkBase>(*other));
			return *this;
		}

	private: // functions
		inline bool				has_parent() const
		{
			return MyLinkBase::is_linked();
		}

		inline ParentT&			get_parent()
		{
			return *MyLinkBase::get_chain();
		}

		inline const ParentT&	get_parent() const
		{
			return *MyLinkBase::get_chain();
		}

		inline ChildT*			previous_sibling()
		{
			return MyLinkBase::previous();
		}

		inline const ChildT*	previous_sibling() const
		{
			return MyLinkBase::previous();
		}

		inline ChildT*			next_sibling()
		{
			return MyLinkBase::next();
		}

		inline const ChildT*	next_sibling() const
		{
			return MyLinkBase::next();
		}
	};

	/*
		Specialization for RELATION_ONELING child type.
		- 8B overhead
	*/
	template <typename ParentT, typename ChildT, uint32_t PID>
	class	IChild<ParentT, ChildT, true, PID> : private dpl::Association<ChildT, ParentT, PID>
	{
	private: // subtypes
		using MyBase	= dpl::Association<ChildT, ParentT, PID>;
		using MyParent	= IParent<ParentT, ChildT, true>;

	public: // relations
		friend MyBase;
		friend dpl::Association<ParentT, ChildT, PID>;
		friend MyParent;

		template<typename, typename, RelationPattern>
		friend class Child;

	protected: // lifecycle
		CLASS_CTOR				IChild(		MyParent*			parent)
		{
			if(parent) parent->add_child(static_cast<ChildT&>(*this));
		}

		CLASS_CTOR				IChild(		const IChild&		OTHER) = delete;
		CLASS_CTOR				IChild(		IChild&&			other) noexcept = default;

		IChild&					operator=(	const IChild&		OTHER) = delete;
		IChild&					operator=(	IChild&&			other) noexcept = default;

		IChild&					operator=(	dpl::Swap<IChild>	other) noexcept
		{
			MyBase::operator=(dpl::Swap<MyBase>(*other));
			return *this;
		}

	private: // functions
		inline bool				has_parent() const
		{
			return MyBase::is_linked();
		}

		inline ParentT&			get_parent()
		{
			return *MyBase::other();
		}

		inline const ParentT&	get_parent() const
		{
			return *MyBase::other();
		}

		inline ChildT*			previous_sibling()
		{
			return nullptr;
		}

		inline const ChildT*	previous_sibling() const
		{
			return nullptr;
		}

		inline ChildT*			next_sibling()
		{
			return nullptr;
		}

		inline const ChildT*	next_sibling() const
		{
			return nullptr;
		}
	};


	/*
		Parent type interface.
		- 20B overhead
	*/
	template<typename ParentT, typename ChildT>
	class	IParent<ParentT, ChildT, false> : private dpl::Chain<ParentT, ChildT, dpl::get_parent_index<ParentT, ChildT>()>
	{
	public: // constants
		static constexpr uint32_t PID = dpl::get_parent_index<ParentT, ChildT>();

	private: // subtypes
		using	MyChainBase	= dpl::Chain<ParentT, ChildT, PID>;
		using	MyLink		= dpl::Link<ParentT, ChildT, PID>;

	public: //relations
		friend	MyChainBase;
		friend	MyLink;
		friend	IChild<ParentT, ChildT, false, PID>;

		template<typename, typename, RelationPattern>
		friend class Parent;

	public: // subtypes
		using	UpdateChild			= std::function<void(ChildT&)>;
		using	UpdateConstChild	= std::function<void(const ChildT&)>;

	protected: // lifecycle
		CLASS_CTOR				IParent() = default;
		CLASS_CTOR				IParent(				const IParent&			OTHER) = delete;
		CLASS_CTOR				IParent(				IParent&&				other) noexcept = default;

		IParent&				operator=(				const IParent&			OTHER) = delete;
		IParent&				operator=(				IParent&&				other) noexcept = default;
		IParent&				operator=(				dpl::Swap<IParent>		other) noexcept
		{
			MyChainBase::operator=(dpl::Swap<MyChainBase>(*other));
			return *this;
		}

	private: // functions
		inline uint32_t			get_numChildren() const
		{
			return MyChainBase::size();
		}

		inline bool				can_have_another_child() const
		{
			return (ChildT::PATTERN != RelationPattern::RELATION_ONELING) || (get_numChildren() == 0);
		}

		inline void				add_child(				ChildT&					child)
		{
			if(!can_have_another_child())
				throw dpl::GeneralException(this, __LINE__, "Could not add another child of this type.");

			if(!MyChainBase::attach_back(child))
				throw dpl::GeneralException(this, __LINE__, "Child already belongs to this parent.");
		}

		inline bool				remove_child(			ChildT&					child)
		{
			return MyChainBase::detach_link(child);
		}

		inline ChildT*			first_child()
		{
			return MyChainBase::first();
		}

		inline const ChildT*	first_child() const
		{
			return MyChainBase::first();
		}

		inline void				for_each_child(			const UpdateChild&		FUNCTION)
		{
			MyChainBase::for_each_link(FUNCTION);
		}

		inline void				for_each_child(			const UpdateConstChild&	FUNCTION) const
		{
			MyChainBase::for_each_link(FUNCTION);
		}
	};

	/*
		Specialization for RELATION_ONELING child type.
		- 8B overhead
	*/
	template<typename ParentT, typename ChildT>
	class	IParent<ParentT, ChildT, true> : private dpl::Association<ParentT, ChildT, dpl::get_parent_index<ParentT, ChildT>()>
	{
	public: // constants
		static constexpr uint32_t PID = dpl::get_parent_index<ParentT, ChildT>();

	private: // subtypes
		using	MyBase	= dpl::Association<ParentT, ChildT, PID>;

	public: //relations
		friend	MyBase;
		friend	dpl::Association<ChildT, ParentT, PID>;
		friend	IChild<ParentT, ChildT, true, PID>;

		template<typename, typename, RelationPattern>
		friend class Parent;

	public: // subtypes
		using	UpdateChild			= std::function<void(ChildT&)>;
		using	UpdateConstChild	= std::function<void(const ChildT&)>;

	protected: // lifecycle
		CLASS_CTOR				IParent() = default;
		CLASS_CTOR				IParent(				const IParent&			OTHER) = delete;
		CLASS_CTOR				IParent(				IParent&&				other) noexcept = default;

		IParent&				operator=(				const IParent&			OTHER) = delete;
		IParent&				operator=(				IParent&&				other) noexcept = default;
		IParent&				operator=(				dpl::Swap<IParent>		other) noexcept
		{
			MyBase::operator=(dpl::Swap<MyBase>(*other));
			return *this;
		}

	private: // functions
		inline uint32_t			get_numChildren() const
		{
			return MyBase::is_linked() ? 1 : 0;
		}

		inline bool				can_have_another_child() const
		{
			return !MyBase::is_linked();
		}

		inline void				add_child(				ChildT&					child)
		{
			if(!can_have_another_child())
				throw dpl::GeneralException(this, __LINE__, "Could not add another child of this type.");

			if(!MyBase::link(child))
				throw dpl::GeneralException(this, __LINE__, "Child already belongs to this parent.");
		}

		inline bool				remove_child(			ChildT&					child)
		{
			return MyBase::unlink(child);
		}

		inline ChildT*			first_child()
		{
			return MyBase::other();
		}

		inline const ChildT*	first_child() const
		{
			return MyBase::other();
		}

		inline void				for_each_child(			const UpdateChild&		FUNCTION)
		{
			if(MyBase::is_linked()) FUNCTION(*MyBase::other());
		}

		inline void				for_each_child(			const UpdateConstChild&	FUNCTION) const
		{
			if(MyBase::is_linked()) FUNCTION(*MyBase::other());
		}

		template<typename CompT>
		inline void				sort(					CompT&&)
		{
			// dummy function
		}
	};



	/*
		Intermediate parent type for RELATION_SELECTIVE types. 
	*/
	template<typename ChildT>
	class	ChosenParent : public IParent<ChosenParent<ChildT>, ChildT, false>{};



	/*
		Specialization of NON-RELATION_SELECTIVE types.
	*/
	template<typename... ParentTs, typename ChildT, RelationPattern _PATTERN>
	class	Child<ParentTypes<ParentTs...>, ChildT, _PATTERN> : public ChildBase<ParentTs, ChildT, ParentTypes<ParentTs...>, _PATTERN>...
	{
	public: // subtypes
		using MyParentTypes	= ParentTypes<ParentTs...>;

		static_assert(ParentTypes<ParentTs...>::SIZE > 0, "Child needs at least one parent. If you want to create a 'root' object use RELATION_INDEPENDENT type.");

	protected: // lifecycle
		CLASS_CTOR				Child(		ParentTs&...		parents) : ChildBase<ParentTs, ChildT, MyParentTypes, _PATTERN>(&parents)...{}

		CLASS_CTOR				Child(		const Child&		OTHER) = delete;

		CLASS_CTOR				Child(		Child&&				other) noexcept : ChildBase<ParentTs, ChildT, MyParentTypes, _PATTERN>(std::move(other))...{}

		Child&					operator=(	const Child&		OTHER) = delete;

		Child&					operator=(	Child&&				other) noexcept
		{
			(ChildBase<ParentTs, ChildT, MyParentTypes, _PATTERN>::operator=(std::move(other)), ...);
			return *this;
		}

		Child&					operator=(	dpl::Swap<Child>	other) noexcept
		{
			(ChildBase<ParentTs, ChildT, MyParentTypes, _PATTERN>::operator=(dpl::Swap<ChildBase<ParentTs, ChildT, MyParentTypes, _PATTERN>>(*other)), ...);
			return *this;
		}

	public: // functions
		template<typename ParentT>
		inline bool				has_parent() const
		{
			return ChildBase<ParentT, ChildT, MyParentTypes, _PATTERN>::has_parent();
		}

		template<typename ParentT>
		inline const ParentT&	get_parent() const
		{
			return ChildBase<ParentT, ChildT, MyParentTypes, _PATTERN>::get_parent();
		}

		template<typename ParentT>
		inline const ChildT*	previous_sibling() const
		{
			return ChildBase<ParentT, ChildT, MyParentTypes, _PATTERN>::previous_sibling();
		}

		template<typename ParentT>
		inline const ChildT*	next_sibling() const
		{
			return ChildBase<ParentT, ChildT, MyParentTypes, _PATTERN>::next_sibling();
		}

	protected: // functions
		template<typename ParentT>
		inline ParentT&			get_parent()
		{
			return ChildBase<ParentT, ChildT, MyParentTypes, _PATTERN>::get_parent();
		}

		template<typename ParentT>
		inline ChildT*			previous_sibling()
		{
			return ChildBase<ParentT, ChildT, MyParentTypes, _PATTERN>::previous_sibling();
		}

		template<typename ParentT>
		inline ChildT*			next_sibling()
		{
			return ChildBase<ParentT, ChildT, MyParentTypes, _PATTERN>::next_sibling();
		}
	};



	/*
		Specialization of RELATION_SELECTIVE types.
	*/
	template<typename... ParentTs, typename ChildT>
	class	Child<ParentTypes<ParentTs...>, ChildT, RelationPattern::RELATION_SELECTIVE> : public IChild<ChosenParent<ChildT>, ChildT, false, 0>
	{
	private: // subtypes
		using MyBase			= IChild<ChosenParent<ChildT>, ChildT, false, 0>;

	public: // subtypes
		using MyParentTypes		= ParentTypes<ChosenParent<ChildT>>;

		static_assert(ParentTypes<ParentTs...>::SIZE > 0, "Child needs at least one parent. If you want to create a 'root' object use RELATION_INDEPENDENT type.");

		static const uint32_t INVALID_PARENT_OPTION_INDEX = std::numeric_limits<uint32_t>::max();

	private: // data
		dpl::ReadOnly<uint32_t, Child> parentOptionIndex;

	protected: // lifecycle
		/*
			Derived class should implement constructor for each potential parent type.
		*/
		template<typename ParentT>
		CLASS_CTOR					Child(		ParentT&			chosenParent)
			: MyBase(&chosenParent) // ParentT must be derived from ChosenParent<ChildT>
			, parentOptionIndex(get_parent_option_index<ParentT>())
		{

		}

		CLASS_CTOR					Child(		const Child&		OTHER) = delete;

		CLASS_CTOR					Child(		Child&&				other) noexcept
			: MyBase(std::move(other))
			, parentOptionIndex(other.parentOptionIndex)
		{
			other.parentOptionIndex = INVALID_PARENT_OPTION_INDEX;
		}

		Child&						operator=(	const Child&		OTHER) = delete;

		Child&						operator=(	Child&&				other) noexcept
		{
			MyBase::operator=(std::move(other));
			parentOptionIndex		= other.parentOptionIndex;
			other.parentOptionIndex	= INVALID_PARENT_OPTION_INDEX;
			return *this;
		}

		Child&						operator=(	dpl::Swap<Child>	other) noexcept
		{
			MyBase::operator=(dpl::Swap<MyBase>(other));
			parentOptionIndex.swap(other->parentOptionIndex);
			return *this;
		}

	public: // functions
		template<typename ParentT>
		static constexpr uint32_t	get_parent_option_index()
		{
			return ParentTypes<ParentTs...>::template Type<ParentT>::INDEX;
		}

		template<typename ParentT>
		inline bool					has_parent() const
		{
			return MyBase::has_parent() && (get_parent_option_index<ParentT>() == parentOptionIndex());
		}

		template<typename ParentT>
		inline const ParentT&		get_parent() const
		{
			validate_parent<ParentT>();
			return static_cast<ParentT*>(MyBase::get_parent());
		}

		inline const ChildT*		previous_sibling() const
		{
			return MyBase::previous_sibling();
		}

		inline const ChildT*		next_sibling() const
		{
			return MyBase::next_sibling();
		}

	protected: // functions
		template<typename ParentT>
		inline ParentT&				get_parent()
		{
			validate_parent<ParentT>();
			return static_cast<const ParentT*>(MyBase::get_parent());
		}

		inline ChildT*				previous_sibling()
		{
			return MyBase::previous_sibling();
		}

		inline ChildT*				next_sibling()
		{
			return MyBase::next_sibling();
		}

	private: // functions
		template<typename ParentT>
		inline void					validate_parent() const
		{
#ifdef _DEBUG
			if(this->has_parent<ParentT>()) return;
			throw dpl::GeneralException(this, __LINE__, "Invalid parent access.");
#endif // _DEBUG
		}
	};



	/*
		Specialization of RELATION_INDEPENDENT types.
	*/
	template<typename... ParentTs, typename ChildT>
	class	Child<ParentTypes<ParentTs...>, ChildT, RelationPattern::RELATION_INDEPENDENT> : public IChild<ChosenParent<ChildT>, ChildT, false, 0>
	{
	private: // subtypes
		using MyBase			= IChild<ChosenParent<ChildT>, ChildT, false, 0>;

	public: // subtypes
		using MyParentTypes		= ParentTypes<ChosenParent<ChildT>>;

		static_assert(std::is_same_v<ParentTypes<ParentTs...>, AnyParent>, "RELATION_INDEPENDENT cannot specify parent types, use empty type list, or AnyParent.");

	protected: // lifecycle
		CLASS_CTOR				Child(		ChosenParent<ChildT>*	potentialParent = nullptr)
			: MyBase(potentialParent)
		{
			
		}

		CLASS_CTOR				Child(		const Child&			OTHER) = delete;

		CLASS_CTOR				Child(		Child&&					other) noexcept
			: MyBase(std::move(other))
		{

		}

		Child&					operator=(	const Child&			OTHER) = delete;

		Child&					operator=(	Child&&					other) noexcept
		{
			MyBase::operator=(std::move(other));
			return *this;
		}

		Child&					operator=(	dpl::Swap<Child>		other) noexcept
		{
			MyBase::operator=(dpl::Swap<MyBase>(*other));
			return *this;
		}

	public: // functions
		inline bool				has_parent() const
		{
			return MyBase::has_parent();
		}

		inline const ChildT*	previous_sibling() const
		{
			return MyBase::previous_sibling();
		}

		inline const ChildT*	next_sibling() const
		{
			return MyBase::next_sibling();
		}

	protected: // functions
		inline ChildT*			previous_sibling()
		{
			return MyBase::previous_sibling();
		}

		inline ChildT*			next_sibling()
		{
			return MyBase::next_sibling();
		}
	};



	template <typename ParentT, typename... ChildTs, RelationPattern PARENT_PATTERN>
	class	Parent<ParentT, ChildTypes<ChildTs...>, PARENT_PATTERN> : public ParentBase<ParentT, ChildTs, PARENT_PATTERN>...
	{
	private: // subtypes
		template<typename ChildT>
		using	MyBase		= ParentBase<ParentT, ChildT, PARENT_PATTERN>;

	public: // subtypes
		template <typename ChildT>
		using	Update		= std::function<void(ChildT&)>;

		template <typename ChildT>
		using	UpdateConst	= std::function<void(const ChildT&)>;

		using	Signature	= dpl::merge_t<dpl::TypeList<ChildTs...>, typename IParentInfo<ParentT, ChildTs, PARENT_PATTERN, std::is_same_v<ParentT, ChildTs>>::ChildSignature...>;

	public: // relations
		template<typename, typename, RelationPattern>
		friend class Parent;

		template<typename, typename, typename, RelationPattern>
		friend class Related;

	protected: // lifecycle
		CLASS_CTOR					Parent() = default;

		CLASS_CTOR					Parent(					const Parent&				OTHER) = delete;

		CLASS_CTOR					Parent(					Parent&&					other) noexcept
			: MyBase<ChildTs>(std::move(other))...
		{

		}

		Parent&						operator=(				const Parent&				OTHER) = delete;

		Parent&						operator=(				Parent&&					other) noexcept
		{
			(MyBase<ChildTs>::operator=(std::move(other)), ...);
			return *this;
		}

		Parent&						operator=(				dpl::Swap<Parent>			other)
		{
			(MyBase<ChildTs>::operator=(dpl::Swap<MyBase<ChildTs>>(*other)), ...);
			return *this;
		}

	public: // functions
		/*
			Get PID of the relation with given ChildT.
		*/
		template<typename ChildT>
		static constexpr uint32_t	get_PID()
		{
			return MyBase<ChildT>::PID;
		}

		template<typename ChildT>
		inline uint32_t				get_numChildren() const
		{
			return MyBase<ChildT>::get_numChildren();
		}

		inline uint32_t				get_totalNumChildren() const
		{
			uint32_t number = 0;
			([&](){number += get_numChildren<ChildTs>();},...);
			return number;
		}

		template<typename ChildT>
		inline bool					can_have_another_child() const
		{
			return MyBase<ChildT>::can_have_another_child();
		}

		template<typename ChildT>
		inline bool					has_children() const
		{
			return get_numChildren<ChildT>() > 0;
		}

		template<typename ChildT>
		inline void					for_each_child(			const Update<ChildT>&		FUNCTION)
		{
			MyBase<ChildT>::for_each_child(FUNCTION);
		}

		template<typename ChildT>
		inline void					for_each_child(			const UpdateConst<ChildT>&	FUNCTION) const
		{
			MyBase<ChildT>::for_each_child(FUNCTION);
		}

		template<typename ChildT>
		inline ChildT*				first_child()
		{
			return MyBase<ChildT>::first_child();
		}

		template<typename ChildT>
		inline const ChildT*		first_child() const
		{
			return MyBase<ChildT>::first_child();
		}

	protected: // functions
		template<typename ChildT>
		inline bool					remove_child(			ChildT&						child)
		{
			return MyBase<ChildT>::remove_child(child);
		}

		template<typename ChildT, typename CompT>
		inline void					sort_children(			CompT&&						compare)
		{
			MyBase<ChildT>::sort(std::forward<CompT>(compare));
		}

	private: // functions
		template<typename GraphSignature, typename ChildT>
		static constexpr bool		test_dependency()
		{
			if constexpr (!std::is_same_v<ParentT, ChildT>)
			{
				constexpr RelationPattern CHILD_PATTERN = ChildT::PATTERN;
				if constexpr (CHILD_PATTERN != RelationPattern::RELATION_INDEPENDENT)
				{
					constexpr bool bVALID = GraphSignature::template Type<ParentT>::INDEX < GraphSignature::template Type<ChildT>::INDEX;
					static_assert(bVALID, "There can only be one root per dependency graph and 'parents' have to be declared before 'children' in the dependency list. Try to reorder child types in the parent class of the RelatedT.");
					return ChildT::template test_dependencies<GraphSignature>();
				}
				else
				{
					return ChildT::VALID_SIGNATURE;
				}
			}

			return true;
		}

		template<typename GraphSignature>
		static constexpr bool		test_dependencies()
		{
			return (... && test_dependency<GraphSignature, ChildTs>());
		}

		template<typename GraphSignature, typename CounterT>
		inline void					count_all_children(		CounterT&					counters) const
		{
			// Increase count only if this is the first parent of that child.
			((counters.get_value<ChildTs>() += (get_PID<ChildTs>() == 0)? get_numChildren<ChildTs>() : 0), ...);
			(for_each_child<ChildTs>([&](const ChildTs& CHILD)
			{
				CHILD.count_all_children<GraphSignature, CounterT>(counters);
			}), ...);
		}
	};


	template<typename RelatedT, typename ParentTs, typename... ChildTs, RelationPattern _PATTERN>
	class	Related<RelatedT, ParentTs, ChildTypes<ChildTs...>, _PATTERN> 
		: public Child<ParentTs, RelatedT, _PATTERN>
		, public Parent<RelatedT, ChildTypes<ChildTs...>, _PATTERN>
	{
	public: // subtypes
		using	MyChildBase		= Child<ParentTs, RelatedT, _PATTERN>;
		using	MyParentBase	= Parent<RelatedT, ChildTypes<ChildTs...>, _PATTERN>;
		using	MyParentTypes	= typename MyChildBase::MyParentTypes;

	public: // constants
		static constexpr RelationPattern	PATTERN	= _PATTERN;
		static constexpr bool				ROOT	= PATTERN == RelationPattern::RELATION_INDEPENDENT;

	public: // subtypes
		using	Signature	= std::conditional_t<ROOT , dpl::merge_t<dpl::TypeList<RelatedT>, typename MyParentBase::Signature>, typename MyParentBase::Signature>::Unique; //::Type;
		using	Counters	= typename Signature::template UniformArray<size_t>;

	private: // functions
		static constexpr bool	test_root_dependency()
		{
			if constexpr (ROOT) return MyParentBase::template test_dependencies<Signature>();
			return true;
		}

	public: // constants		
		static constexpr bool VALID_SIGNATURE	= test_root_dependency();

	public: // relations
		template<typename, typename, typename, RelationPattern>
		friend class Related;

	public: // lifecycle
		template<typename... PotentialParentTs>
		CLASS_CTOR					Related(		PotentialParentTs&...	parents)
			: MyChildBase(parents...)
		{
			
		}

		CLASS_CTOR					Related(		const Related&			OTHER) = delete;

		CLASS_CTOR					Related(		Related&&				other) noexcept
			: MyChildBase(std::move(other))
			, MyParentBase(std::move(other))
		{

		}

		inline Related&				operator=(		const Related&			OTHER) = delete;

		inline Related&				operator=(		Related&&				other) noexcept
		{
			MyChildBase::operator=(std::move(other));
			MyParentBase::operator=(std::move(other));
			return *this;
		}

		inline Related&				operator=(		dpl::Swap<Related>		other)
		{
			MyChildBase::operator=(dpl::Swap<MyChildBase>(*other));
			MyParentBase::operator=(dpl::Swap<MyParentBase>(*other));
			return *this;
		}

	public: // functions
		template<typename ChildT>
		static constexpr uint32_t	get_child_dependency_INDEX()
		{
			return Signature::template Type<ChildT>::INDEX;
		}

		/*
			Returns number of objects of each type in the Signature.
		*/
		inline Counters				get_graph_size() const
		{
			Counters counters; counters.get_value<RelatedT>() = 1;
			((counters.get_value<ChildTs>() = 0), ...);
			MyParentBase::template count_all_children<Signature, Counters>(counters);
			return counters;
		}
	};
#pragma pack(pop)

#pragma pack(push, 4)
	namespace relations_example
	{
		class ObjectA;
		class ObjectB;
		class ObjectC;
		class ObjectD;

		class Base
		{

		};

		class ObjectA   : public dpl::Related<ObjectA, dpl::ParentTypes<ObjectB, ObjectC>, dpl::ChildTypes<>, dpl::RELATION_COMMON>
						, public Base
		{
		public: using Related::Related;
		};

		class ObjectB	: public dpl::Related<ObjectB, dpl::ParentTypes<ObjectC, ObjectD>, dpl::ChildTypes<ObjectA>, dpl::RELATION_SELECTIVE>
						, public Base
		{
		public: 
			using Related::Related;
		};

		class ObjectC	: public dpl::Related<ObjectC, dpl::ParentTypes<ObjectD>, dpl::ChildTypes<ObjectA, ObjectB>, dpl::RELATION_COMMON>
		{
		public: using Related::Related;
		};

		class ObjectX	: public dpl::Related<ObjectX, dpl::ParentTypes<ObjectD>, dpl::NoChildren, dpl::RELATION_ONELING>
		{
		public: using Related::Related;
		};

		class ObjectD	: public dpl::Related<ObjectD, dpl::AnyParent, dpl::ChildTypes<ObjectC, ObjectB, ObjectX>, dpl::RELATION_INDEPENDENT>
		{
		public: using Related::Related;
		};

		bool test()
		{
			ObjectD objD;
			ObjectC objC(objD);
			ObjectB objB(objD);
			ObjectA objA(objB, objC);
			ObjectX objX(objD);

			const auto GRAPH_SIZE = objD.get_graph_size();

			{ // OVERHEAD
				const auto SIZE_A = sizeof(ObjectA); // 48
				const auto SIZE_B = sizeof(ObjectB); // 48
				const auto SIZE_C = sizeof(ObjectC); // 64
				const auto SIZE_D = sizeof(ObjectD); // 72
				const auto SIZE_X = sizeof(ObjectX); // 8

				const auto TOTAL_SIZE = SIZE_A + SIZE_B + SIZE_C + SIZE_D;
			}

			{ // CONDITIONAL PARENT
				const bool HAS_C = objB.has_parent<ObjectC>();
				const bool HAS_D = objB.has_parent<ObjectD>();

				const bool HAS_ANY = HAS_C || HAS_D;
			}
		
			{ // ITERATION
				objB.for_each_child<ObjectA>([](Base& child)
				{

				});
			}
			

			return true;
		}
	}
#pragma pack(pop)
}