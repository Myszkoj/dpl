#pragma once


#include "dpl_Association.h"
#include <functional>


// forward declarations
namespace dpl
{
	template<typename T, uint32_t ID = 0>
	class Previous;

	template<typename T, uint32_t ID = 0>
	class Next;

	template<typename T, uint32_t ID = 0>
	class Link;
}

// implementations
namespace dpl
{
	/*
		Interface of the Link class.
	*/
	template<typename T, uint32_t ID>
	class	Previous : private Association<Previous<T, ID>, Next<T, ID>, ID>
	{
	private:	// [SUBTYPES]
		using	MyBase = Association<Previous<T, ID>, Next<T, ID>, ID>;

	public:		// [FRIENDS]
		friend	MyBase;
		friend	Association<Next<T, ID>, Previous<T, ID>, ID>;
		friend	Link<T, ID>;

	private:	// [LIFECYCLE]
		using	MyBase::MyBase;
		using	MyBase::operator=;

		Previous&				operator=(Swap<Previous> other)
		{
			return static_cast<Previous&>(MyBase::operator=(Swap(other)));
		}

	protected:	// [FUNCTIONS]
		Link<T, ID>*			get_raw_prev()
		{
			return static_cast<Link<T, ID>*>(MyBase::other());
		}

		const Link<T, ID>*		get_raw_prev() const
		{
			return static_cast<const Link<T, ID>*>(MyBase::other());
		}
	};


	/*
		Interface of the Link class.
	*/
	template<typename T, uint32_t ID>
	class	Next : private Association<Next<T, ID>, Previous<T, ID>, ID>
	{
	private:	// [SUBTYPES]
		using	MyBase = Association<Next<T, ID>, Previous<T, ID>, ID>;

	public:		// [FRIENDS]
		friend	MyBase;
		friend	Association<Previous<T, ID>, Next<T, ID>, ID>;
		friend	Link<T, ID>;

	private:	// [LIFECYCLE]
		using	MyBase::MyBase;
		using	MyBase::operator=;

		Next&				operator=(Swap<Next> other)
		{
			return static_cast<Next&>(MyBase::operator=(Swap(other)));
		}

	protected:	// [FUNCTIONS]
		Link<T, ID>*		get_raw_next()
		{
			return static_cast<Link<T, ID>*>(MyBase::other());
		}

		const Link<T, ID>*	get_raw_next() const
		{
			return static_cast<const Link<T, ID>*>(MyBase::other());
		}
	};


	/*
		Linked list node.
	*/
	template<typename T, uint32_t ID>
	class	Link	: public Previous<T, ID>
					, public Next<T, ID>
	{
	private:	// [SUBTYPES]
		using	MyPrev				= Previous<T, ID>;
		using	MyNext				= Next<T, ID>;

	public:		// [SUBTYPES]
		using	Invoke				= std::function<void(T&)>;
		using	InvokeConst			= std::function<void(const T&)>;
		using	InvokeUntil			= std::function<bool(T&)>;
		using	InvokeConstUntil	= std::function<bool(const T&)>;

	protected:	// [LIFECYCLE]
		CLASS_CTOR			Link() = default;

		CLASS_CTOR			Link(						const Link&				OTHER) = delete;

		CLASS_CTOR			Link(						Link&&					other) noexcept
			: MyPrev(std::move(other))
			, MyNext(std::move(other))
		{
			
		}

		CLASS_DTOR			~Link()
		{
			dpl::no_except([&](){	detach();	});
		}

		Link&				operator=(					const Link&				OTHER) = delete;

		Link&				operator=(					Link&&					other) noexcept
		{
			detach();
			MyPrev::operator=(std::move(other));
			MyNext::operator=(std::move(other));
			return *this;
		}

		Link&				operator=(					Swap<Link>				other)
		{
			MyPrev::operator=(Swap<MyPrev>(*other));
			MyNext::operator=(Swap<MyNext>(*other));
			return *this;
		}

		Link&				operator=(					Swap<T>					other)
		{
			return operator=(Swap<Link>(*other));
		}

	public:		// [FUNCTIONS]
		T*					cast()
		{
			return static_cast<T*>(this);
		}

		const T*			cast() const
		{
			return static_cast<const T*>(this);
		}

		bool				is_linked_to_previous() const
		{
			return MyPrev::get_raw_prev() != nullptr;
		}

		bool				is_linked_to_next() const
		{
			return MyNext::get_raw_next() != nullptr;
		}

		bool				is_linked_to_any() const
		{
			return is_linked_to_previous() || is_linked_to_next();
		}

		T*					previous()
		{
			return is_linked_to_previous() ? MyPrev::get_raw_prev()->cast() : nullptr;
		}

		const T*			previous() const
		{
			return is_linked_to_previous() ? MyPrev::get_raw_prev()->cast() : nullptr;
		}

		T*					next()
		{
			return is_linked_to_next() ? MyNext::get_raw_next()->cast() : nullptr;
		}

		const T*			next() const
		{
			return is_linked_to_next() ? MyNext::get_raw_next()->cast() : nullptr;
		}

		uint32_t			iterate_forward(			const Invoke&			INVOKE)
		{
			uint32_t	count	= 0;
			Link*		current = next();
			while(current)
			{
				Link* next = current->next();
				INVOKE(*current->cast());
				++count;
				current = next;
			}
			return count;
		}

		uint32_t			iterate_forward_until(		const InvokeUntil&		INVOKE)
		{
			uint32_t	count	= 0;
			Link*		current = next();
			while(current)
			{
				Link* next = current->next();
				++count;
				if(INVOKE(*current->cast())) break;			
				current = next;
			}
			return count;
		}

		uint32_t			iterate_backwards(			const Invoke&			INVOKE)
		{
			uint32_t	count	= 0;
			Link*		current = previous();
			while(current)
			{
				Link* next = current->previous();
				INVOKE(*current->cast());
				++count;
				current = next;
			}
			return count;
		}

		uint32_t			iterate_backwards_until(	const InvokeUntil&		INVOKE)
		{
			uint32_t	count	= 0;
			Link*		current = previous();
			while(current)
			{
				Link* next = current->previous();
				++count;
				if(INVOKE(*current->cast())) break;			
				current = next;
			}
			return count;
		}

		uint32_t			for_each_other(				const Invoke&			INVOKE)
		{
			return Link::iterate_forward(INVOKE) + Link::iterate_backwards(INVOKE);
		}

		uint32_t			for_each(					const Invoke&			INVOKE)
		{
			INVOKE(*cast());
			return 1 + Link::iterate_forward(INVOKE) + Link::iterate_backwards(INVOKE);
		}

		uint32_t			iterate_forward(			const InvokeConst&		INVOKE) const
		{
			uint32_t	count = 0;
			const Link*	CURRENT = next();
			while(CURRENT)
			{
				const Link* NEXT = CURRENT->next();
				INVOKE(*CURRENT->cast());
				++count;
				CURRENT = NEXT;
			}
			return count;
		}

		uint32_t			iterate_forward_until(		const InvokeConstUntil&	INVOKE) const
		{
			uint32_t	count = 0;
			const Link*	CURRENT = next();
			while(CURRENT)
			{
				const Link* NEXT = CURRENT->next();
				++count;
				if(INVOKE(*CURRENT->cast())) break;			
				CURRENT = NEXT;
			}
			return count;
		}

		uint32_t			iterate_backwards(			const InvokeConst&		INVOKE) const
		{
			uint32_t	count = 0;
			const Link*	CURRENT = previous();
			while(CURRENT)
			{
				const Link* NEXT = CURRENT->previous();
				INVOKE(*CURRENT->cast());
				++count;
				CURRENT = NEXT;
			}
			return count;
		}

		uint32_t			iterate_backwards_until(	const InvokeConstUntil&	INVOKE) const
		{
			uint32_t	count = 0;
			const Link*	CURRENT = previous();
			while(CURRENT)
			{
				const Link* NEXT = CURRENT->previous();
				++count;
				if(INVOKE(*CURRENT->cast())) break;			
				CURRENT = NEXT;
			}
			return count;
		}

		uint32_t			for_each_other(				const InvokeConst&		INVOKE) const
		{
			return Link::iterate_forward(INVOKE) + Link::iterate_backwards(INVOKE);
		}

		uint32_t			for_each(					const InvokeConst&		INVOKE) const
		{
			INVOKE(*cast());
			return 1 + Link::iterate_forward(INVOKE) + Link::iterate_backwards(INVOKE);
		}

	protected:	// [FUNCTIONS]
		void				attach_front(				Link&					other)
		{
			throw_if_same(other);
			detach();
			Link* prev = other.get_raw_prev();
			MyNext::link(other);
			if (prev) MyPrev::link(*prev);
		}

		void				attach_back(				Link&					other)
		{
			throw_if_same(other);
			detach();
			Link* next = other.get_raw_next();
			MyPrev::link(other);
			if (next) MyNext::link(*next);
		}

		void				attach(						MyNext&					prev,
														MyPrev&					next)
		{
			throw_if_same(static_cast<const Link&>(prev));
			throw_if_same(static_cast<const Link&>(next));
			detach();
			prev.link(*this);
			next.link(*this);
		}

		void				detach()
		{
			if(MyPrev::get_raw_prev() != MyNext::get_raw_next())
			{
				MyPrev::other()->link(*MyNext::other());
			}
			else // This was the last object in sequence, we have to break the loop.
			{
				MyPrev::unlink();
				MyNext::unlink();
			}
		}

	private:	// [DEBUG]
		void				throw_if_same(				const Link&				OTHER) const
		{
#ifdef _DEBUG
			if (this != &OTHER) return;
			throw dpl::GeneralException(this, __LINE__, "Cannot link to itself.");
#endif // _DEBUG
		}
	};
}