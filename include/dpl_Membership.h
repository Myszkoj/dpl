#pragma once


#include "dpl_Sequence.h"


#pragma pack(push, 4)

// declarations
namespace dpl
{
	template<typename GroupT, typename MemberT, uint32_t ID = 0>
	class Group;

	template<typename GroupT, typename MemberT, uint32_t ID = 0>
	class Member;
}

// implemenations
namespace dpl
{
	/*
		Can be attached to the group.
		Acts as a linked list node.
	*/
	template<typename GroupT, typename MemberT, uint32_t ID>
	class Member : private Sequenceable<MemberT, ID>
	{
	protected: // subtypes
		using	MyBase		= Sequenceable<MemberT, ID>;
		using	MySequence	= Sequence<MemberT, ID>;
		using	MyLink		= Link<MemberT, ID>;
		using	MyGroup		= Group<GroupT, MemberT, ID>;

	public: // relations
		friend	MyBase;
		friend	MySequence;
		friend	MyLink;
		friend	MyGroup;

	private: // data
		MyGroup* m_group;

	protected: // lifecycle
		CLASS_CTOR				Member()
			: m_group(nullptr)
		{

		}

		CLASS_CTOR				Member(			MyGroup&			group)
			: Member()
		{
			group.add_end_member(*this);
		}

		CLASS_CTOR				Member(			const Member&		OTHER) = delete;

		CLASS_CTOR				Member(			Member&&			other) noexcept
			: MyBase(std::move(other))
			, m_group(other.m_group)
		{
			other.m_group = nullptr;
		}

		CLASS_DTOR				~Member()
		{
			dpl::no_except([&](){	invalidate_membership();	});
		}

		Member&					operator=(		const Member&		other) = delete;

		Member&					operator=(		Member&&			other) noexcept
		{
			MyBase::operator=(std::move(other));
			invalidate_membership();
			m_group			= other.m_group;
			other.m_group	= nullptr;
			return *this;
		}

		inline Member&			operator=(		Swap<Member>		other)
		{
			MyBase::operator=(Swap(static_cast<MyBase&>(*other.get())));
			std::swap(m_group, other->m_group);
			return *this;
		}

		inline Member&			operator=(		Swap<MemberT>		other)
		{
			return operator=(Swap<Member>(*other));
		}

	public: // functions
		inline MemberT*			previous()
		{
			return m_group ? static_cast<MemberT*>(MyBase::previous(*m_group)) : nullptr;
		}

		inline const MemberT*	previous() const
		{
			return m_group ? static_cast<const MemberT*>(MyBase::previous(*m_group)) : nullptr;
		}

		inline MemberT*			next()
		{
			return m_group ? static_cast<MemberT*>(MyBase::next(*m_group)) : nullptr;
		}

		inline const MemberT*	next() const
		{
			return m_group ? static_cast<const MemberT*>(MyBase::next(*m_group)) : nullptr;
		}

		inline bool				is_member() const
		{
			return MyBase::is_part_of_sequence();
		}

		inline bool				is_member_of(	const MyGroup&		GROUP) const
		{
			return m_group == &GROUP;
		}

		inline const GroupT*	get_group() const
		{
			return m_group ? m_group->cast() : nullptr;
		}

	protected: // functions
		inline GroupT*			get_group()
		{
			return m_group ? m_group->cast() : nullptr;
		}

		/*
			Remove this link from the chain.
		*/
		inline void				detach()
		{
			invalidate_membership();
			MyBase::remove_from_sequence();
		}

	private: // functions
		inline MemberT*			cast()
		{
			return static_cast<MemberT*>(this);
		}

		inline const MemberT*	cast() const
		{
			return static_cast<const MemberT*>(this);
		}

		inline void				invalidate_membership()
		{
			if(m_group)
			{
				--m_group->m_numMembers;
				m_group = nullptr;
			}
		}
	};


	/*
		Stores (but not owns) linked list of members.
	*/
	template<typename GroupT, typename MemberT, uint32_t ID>
	class Group : public Sequence<MemberT, ID>
	{
	protected: // subtypes
		using	MyBase		= Sequence<MemberT, ID>;
		using	MyMember	= Member<GroupT, MemberT, ID>;

	public: // friends
		friend	MyBase;
		friend	MyMember;

	public: // subtypes
		using	Invoke				= std::function<void(MemberT&)>;
		using	InvokeConst			= std::function<void(const MemberT&)>;
		using	InvokeUntil			= std::function<bool(MemberT&)>;
		using	InvokeConstUntil	= std::function<bool(const MemberT&)>;

	private: // data
		uint32_t m_numMembers;

	protected: // lifecycle
		CLASS_CTOR				Group()
			: m_numMembers(0)
		{

		}

		CLASS_CTOR				Group(					const Group&				OTHER) = delete;

		CLASS_CTOR				Group(					Group&&						other) noexcept
			: MyBase(std::move(other))
			, m_numMembers(other.m_numMembers)
		{
			other.m_numMembers = 0;
			update_members();
		}

		CLASS_DTOR				~Group()
		{
			dpl::no_except([&](){	remove_all_members();	});
		}

		inline Group&			operator=(				const Group&				OTHER) = delete;

		Group&					operator=(				Group&&						other) noexcept
		{
			remove_all_members();//<-- We have to manually remove the members to nullify their connection to this group.
			MyBase::operator=(std::move(other));
			other.m_numMembers = 0;
			update_members();
			return *this;
		}

		Group&					operator=(				Swap<Group>					other)
		{
			MyBase::operator=(Swap(static_cast<MyBase&>(*other.get())));
			std::swap(m_numMembers, other->m_numMembers);
			this->update_members();
			other->update_members();
			return *this;
		}

		inline Group&			operator=(				Swap<GroupT>				other)
		{
			return operator=(Swap<Group>(*other));
		}

	public: // functions
		/*
			Returns number of links in this chain.
		*/
		inline uint32_t			size() const
		{
			return m_numMembers;
		}

		/*
			Returns true if object is a nullptr or it points to the internal loop object(end of the sequence).
		*/
		inline bool				is_end(					const MyMember*				MEMBER) const
		{
			return MyBase::is_end(MEMBER);
		}

		inline uint32_t			for_each_member(		const InvokeConst&			INVOKE) const
		{
			return MyBase::for_each(INVOKE);
		}

		/*
			Invokes all members in a group until given function returns false.
			Returns number of function calls.
		*/
		inline uint32_t			for_each_member_until(	const InvokeConstUntil&		INVOKE) const
		{
			return MyBase::for_each_until(INVOKE);
		}

	protected: // functions
		/*
			Adds given member at the front of the group.
			Returns false if member is already attached.
		*/
		bool					add_front_member(		MyMember&					newMember)
		{
			if(newMember.m_group != this)
			{
				newMember.invalidate_membership();
				MyBase::add_front(newMember);
				newMember.m_group = this;
				++m_numMembers;
				return true;
			}

			return false;
		}

		/*
			Adds given member at the end of the group.
			Returns false if member is already attached.
		*/
		bool					add_end_member(			MyMember&					newMember)
		{
			if(newMember.m_group != this)
			{
				newMember.invalidate_membership();
				MyBase::add_back(newMember);
				newMember.m_group = this;
				++m_numMembers;
				return true;
			}

			return false;
		}

		inline uint32_t			for_each_member(		const Invoke&				INVOKE)
		{
			return MyBase::for_each(INVOKE);
		}

		/*
			Invokes all members in a group until given function returns false.
			Returns number of function calls.
		*/
		inline uint32_t			for_each_member_until(	const InvokeUntil&			INVOKE)
		{
			return MyBase::for_each_until(INVOKE);
		}

		inline bool				remove_member(			MyMember&					member)
		{
			if(!member.is_member_of(*this)) return false;
			member.detach();
			return true;
		}

		bool					remove_all_members()
		{
			if(size() == 0) return false;
			MyMember* current = MyBase::first();
			while(!MyBase::is_end(current))
			{
				current->detach(); //<-- Number of members is updated here.
				current = MyBase::first();
			}
			return true;
		}

	private: // functions
		inline GroupT*			cast()
		{
			return static_cast<GroupT*>(this);
		}

		inline const GroupT*	cast() const
		{
			return static_cast<const GroupT*>(this);
		}

		/*
			Updates pointer-to-group for each attached member.
		*/
		inline void				update_members()
		{
			Group::for_each_member([&](MemberT& link)
			{
				static_cast<MyMember&>(link).m_group = this;
			});
		}

	private: // hidden functions
		using MyBase::add_front;
		using MyBase::add_back;
		using MyBase::remove_all;
	};
}

#pragma pack(pop)