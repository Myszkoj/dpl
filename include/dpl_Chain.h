#pragma once


#include "dpl_Sequence.h"


#pragma pack(push, 4)

// declarations
namespace dpl
{
	template<typename ChainT, typename LinkT, uint32_t ID = 0>
	class Chain;

	template<typename ChainT, typename LinkT, uint32_t ID = 0>
	class Link;
}

// implemenations
namespace dpl
{
	template<typename ChainT, typename LinkT, uint32_t ID>
	class Link : private Sequenceable<LinkT, ID>
	{
	protected: // subtypes
		using	MyBase	= Sequenceable<LinkT, ID>;
		using	MyChain	= Chain<ChainT, LinkT, ID>;

	public: // relations
		friend	MyBase;
		friend	MyChain;

	private: // data
		MyChain* m_chain;

	protected: // lifecycle
		CLASS_CTOR				Link()
			: m_chain(nullptr)
		{

		}

		CLASS_CTOR				Link(			MyChain&		chain)
			: Link()
		{
			chain.attach_back(*this);
		}

		CLASS_CTOR				Link(			const Link&		other) = delete;

		CLASS_CTOR				Link(			Link&&			other) noexcept
			: MyBase(std::move(other))
			, m_chain(other.m_chain)
		{
			other.m_chain = nullptr;
		}

		CLASS_DTOR				~Link()
		{
			invalidate_chain();
		}

		Link&					operator=(		const Link&		other) = delete;

		Link&					operator=(		Link&&			other) noexcept
		{
			MyBase::operator=(std::move(other));
			invalidate_chain();
			m_chain			= other.m_chain;
			other.m_chain	= nullptr;
			return *this;
		}

		inline Link&			operator=(		Swap<Link>		other)
		{
			MyBase::operator=(Swap(static_cast<MyBase&>(*other.get())));
			std::swap(m_chain, other->m_chain);
			return *this;
		}

		inline Link&			operator=(		Swap<LinkT>		other)
		{
			return operator=(Swap<Link>(*other));
		}

	public: // functions
		inline LinkT*			previous()
		{
			return m_chain ? static_cast<LinkT*>(MyBase::previous(*m_chain)) : nullptr;
		}

		inline const LinkT*		previous() const
		{
			return m_chain ? static_cast<const LinkT*>(MyBase::previous(*m_chain)) : nullptr;
		}

		inline LinkT*			next()
		{
			return m_chain ? static_cast<LinkT*>(MyBase::next(*m_chain)) : nullptr;
		}

		inline const LinkT*		next() const
		{
			return m_chain ? static_cast<const LinkT*>(MyBase::next(*m_chain)) : nullptr;
		}

		inline bool				is_linked() const
		{
			return MyBase::is_part_of_sequence();
		}

		inline bool				is_linked(		const MyChain&	CHAIN) const
		{
			return m_chain == &CHAIN;
		}

		inline const ChainT*	get_chain() const
		{
			return m_chain ? m_chain->cast() : nullptr;
		}

	protected: // functions
		inline ChainT*			get_chain()
		{
			return m_chain ? m_chain->cast() : nullptr;
		}

		/*
			Remove this link from the chain.
		*/
		inline void				detach()
		{
			invalidate_chain();
			MyBase::remove_from_sequence();
		}

	private: // functions
		inline LinkT*			cast()
		{
			return static_cast<LinkT*>(this);
		}

		inline const LinkT*		cast() const
		{
			return static_cast<const LinkT*>(this);
		}

		inline void				invalidate_chain()
		{
			if(m_chain)
			{
				--m_chain->m_numLinks;
				m_chain = nullptr;
			}
		}
	};

	/*
		Double linked list of T elements without ownership.
	*/
	template<typename ChainT, typename LinkT, uint32_t ID>
	class Chain : public Sequence<LinkT, ID>
	{
	protected: // subtypes
		using	MyBase = Sequence<LinkT, ID>;
		using	MyLink = Link<ChainT, LinkT, ID>;

	public: // relations
		friend	MyBase;
		friend	MyLink;

	private: // data
		uint32_t m_numLinks;

	protected: // lifecycle
		CLASS_CTOR				Chain()
			: m_numLinks(0)
		{

		}

		CLASS_CTOR				Chain(					const Chain&						OTHER) = delete;

		CLASS_CTOR				Chain(					Chain&&								other) noexcept
			: MyBase(std::move(other))
			, m_numLinks(other.m_numLinks)
		{
			other.m_numLinks = 0;
			update_links();
		}

		CLASS_DTOR				~Chain()
		{
			remove_all_links();
		}

		inline Chain&			operator=(				const Chain&						OTHER) = delete;

		Chain&					operator=(				Chain&&								other) noexcept
		{
			remove_all_links();//<-- We have to manually remove the links to nullify their connection to this chain.
			MyBase::operator=(std::move(other));//<-- Changes target of the first and last link to this chain.
			other.m_numLinks = 0;
			update_links();
			return *this;
		}

		Chain&					operator=(				Swap<Chain>							other)
		{
			MyBase::operator=(Swap(static_cast<MyBase&>(*other.get())));
			std::swap(m_numLinks, other->m_numLinks);
			this->update_links();
			other->update_links();
			return *this;
		}

		inline Chain&			operator=(				Swap<ChainT>						other)
		{
			return operator=(Swap<Chain>(*other));
		}

	public: // functions
		/*
			Returns number of links in this chain.
		*/
		inline uint32_t			size() const
		{
			return m_numLinks;
		}

		/*
			Returns true if object is a nullptr or it points to the internal loop object(end of the sequence).
		*/
		inline bool				is_end(					const MyLink*						OBJ) const
		{
			return MyBase::is_end(OBJ);
		}

		/*
			Loops over all links and returns their number.
		*/
		inline uint32_t			for_each_link(			std::function<void(const LinkT&)>	FUNCTION) const
		{
			return MyBase::for_each(FUNCTION);
		}

		/*
			Returns number of 'FUNCTION' calls until true.
		*/
		inline uint32_t			for_each_link_until(	std::function<bool(const LinkT&)>	FUNCTION) const
		{
			return MyBase::for_each_until(FUNCTION);
		}

	protected: // functions
		/*
			Adds given link at the front of the chain.
			Returns false if link is already attached.
		*/
		bool					attach_front(			MyLink&								newLink)
		{
			if(newLink.m_chain != this)
			{
				newLink.invalidate_chain();
				MyBase::add_front(newLink);
				newLink.m_chain = this;
				++m_numLinks;
				return true;
			}

			return false;
		}

		/*
			Adds given link at the end of the chain.
			Returns false if link is already attached.
		*/
		bool					attach_back(			MyLink&								newLink)
		{
			if(newLink.m_chain != this)
			{
				newLink.invalidate_chain();
				MyBase::add_back(newLink);
				newLink.m_chain = this;
				++m_numLinks;
				return true;
			}

			return false;
		}

		/*
			Loops over all links and returns their number.
		*/
		inline uint32_t			for_each_link(			std::function<void(LinkT&)>			function)
		{
			return MyBase::for_each(function);
		}

		/*
			Returns number of 'FUNCTION' calls until true.
		*/
		inline uint32_t			for_each_link_until(	std::function<bool(LinkT&)>			function)
		{
			return MyBase::for_each_until(function);
		}

		inline bool				detach_link(			MyLink&								link)
		{
			if(!link.is_linked(*this)) return false;
			link.detach();
			return true;
		}

		/*
			Removes all links from this chain.
		*/
		bool					remove_all_links()
		{
			if(size() == 0) return false;
			MyLink* current = MyBase::first();
			while(!MyBase::is_end(current))
			{
				current->detach(); //<-- Number of links is updated here.
				current = MyBase::first();
			}
			return true;
		}

	private: // functions
		inline ChainT*			cast()
		{
			return static_cast<ChainT*>(this);
		}

		inline const ChainT*	cast() const
		{
			return static_cast<const ChainT*>(this);
		}

		/*
			Updates pointer-to-chain for each attached link.
		*/
		inline void				update_links()
		{
			for_each_link([&](LinkT& link)
			{
				static_cast<MyLink&>(link).m_chain = this;
			});
		}

	private: // hidden functions
		using MyBase::add_front;
		using MyBase::add_back;
		using MyBase::remove_all;
	};
}

#pragma pack(pop)