#pragma once


#include "dpl_Link.h"


#pragma pack(push, 4)

// forward declarations
namespace dpl
{
	template<typename T, uint32_t ID = 0>
	class Sequenceable;

	template<typename T, uint32_t ID = 0>
	class Sequence;
}

// implemantations
namespace dpl
{
	template<typename T, uint32_t ID>
	class Sequenceable	: private Link<T, ID>
	{
	protected:	// [SUBTYPES]
		using	MyBase		= Link<T, ID>;
		using	MySequence	= Sequence<T, ID>;

	public:		// [FRIENDS]
		friend	MyBase;
		friend	MySequence;

	private:	// [HIDDEN]
		using	MyBase::attach;

	protected:	// [LIFECYCLE]
		CLASS_CTOR			Sequenceable() = default;
		CLASS_CTOR			Sequenceable(	const Sequenceable&		OTHER) = delete;
		CLASS_CTOR			Sequenceable(	Sequenceable&&			other) noexcept = default;
		Sequenceable&		operator=(		const Sequenceable&		OTHER) = delete;
		Sequenceable&		operator=(		Sequenceable&&			other) noexcept = default;

		Sequenceable&		operator=(		Swap<Sequenceable>		other)
		{
			MyBase::operator=(Swap<MyBase>(*other));
			return *this;
		}

		Sequenceable&		operator=(		Swap<T>					other)
		{
			return operator=(Swap<Sequenceable>(*other));
		}

	public:		// [FUNCTIONS]
		bool				is_part_of_sequence() const
		{
			return MyBase::is_linked_to_any();
		}

		T*					previous(		const MySequence&		SEQUENCE)
		{
			auto* prev = raw_previous();
			return (SEQUENCE.is_end(prev)) ? nullptr : prev->cast();
		}

		const T*			previous(		const MySequence&		SEQUENCE) const
		{
			const auto* PREV = raw_previous();
			return (SEQUENCE.is_end(PREV)) ? nullptr : PREV->cast();
		}

		T*					next(			const MySequence&		SEQUENCE)
		{
			auto* next = raw_next();
			return (SEQUENCE.is_end(next)) ? nullptr : next->cast();
		}

		const T*			next(			const MySequence&		SEQUENCE) const
		{
			const auto* NEXT = raw_next();
			return (SEQUENCE.is_end(NEXT)) ? nullptr : NEXT->cast();
		}

	protected:	// [FUNCTIONS]
		void				remove_from_sequence()
		{
			MyBase::detach();
		}

	private:	// [FUNCTIONS]
		Sequenceable*		raw_previous()
		{
			return static_cast<Sequenceable*>(MyBase::get_raw_prev());
		}

		const Sequenceable*	raw_previous() const
		{
			return static_cast<const Sequenceable*>(MyBase::get_raw_prev());
		}

		Sequenceable*		raw_next()
		{
			return static_cast<Sequenceable*>(MyBase::get_raw_next());
		}

		const Sequenceable*	raw_next() const
		{
			return static_cast<const Sequenceable*>(MyBase::get_raw_next());
		}
	};


	/*
		Sequence of T objects.
	*/
	template<typename T, uint32_t ID>
	class Sequence
	{
	private:	// [SUBTYPES]
		using	MySequenceable		= Sequenceable<T, ID>;
		using	MyLink				= Link<T, ID>;

	public:		// [SUBTYPES]
		using	Invoke				= std::function<void(T&)>;
		using	InvokeConst			= std::function<void(const T&)>;
		using	InvokeUntil			= std::function<bool(T&)>;
		using	InvokeConstUntil	= std::function<bool(const T&)>;
		using	NumInSequence		= uint32_t;

	private:	// [DATA]
		MySequenceable	m_loop;

	protected:	// [LIFECYCLE]
		CLASS_CTOR				Sequence() = default;

		CLASS_CTOR				Sequence(				const Sequence&				OTHER) = delete;

		CLASS_CTOR				Sequence(				Sequence&&					other) noexcept
			: m_loop(std::move(other.m_loop))
		{

		}

		Sequence&				operator=(				const Sequence&				OTHER) = delete;

		Sequence&				operator=(				Sequence&&					other) noexcept
		{
			m_loop = std::move(other.m_loop);
			return *this;
		}

		Sequence&				operator=(				Swap<Sequence>				other)
		{
			m_loop = Swap(other->m_loop);
			return *this;
		}

	public:		// [FUNCTIONS]
		const T*				first() const
		{
			return m_loop.next(*this);
		}

		const T*				last() const
		{
			return m_loop.previous(*this);
		}

		bool					empty() const
		{
			return first() == nullptr;
		}

		// Returns true if object is a nullptr or it points to the internal loop object(end of the sequence).
		bool					is_end(					const MySequenceable*		OBJ) const
		{
			return OBJ == &m_loop || OBJ == nullptr;
		}

		NumInSequence			iterate_forward(		const InvokeConst&			INVOKE) const
		{
			NumInSequence			count	= 0;
			const MySequenceable*	CURRENT = m_loop.raw_next();
			while(!is_end(CURRENT))
			{
				const MySequenceable* NEXT = CURRENT->raw_next();
				INVOKE(*CURRENT->cast());
				++count;
				CURRENT = NEXT;
			}
			return count;
		}

		NumInSequence			iterate_forward_until(	const InvokeConstUntil&		INVOKE) const
		{
			NumInSequence			count = 0;
			const MySequenceable*	CURRENT = m_loop.raw_next();
			while(!is_end(CURRENT))
			{
				const MySequenceable* NEXT = CURRENT->raw_next();
				++count;
				if(INVOKE(*CURRENT->cast())) break;			
				CURRENT = NEXT;
			}
			return count;
		}

		NumInSequence			iterate_backwards(		const InvokeConst&			INVOKE) const
		{
			NumInSequence			count	= 0;
			const MySequenceable*	CURRENT = m_loop.raw_previous();
			while(!is_end(CURRENT))
			{
				const MySequenceable* NEXT = CURRENT->raw_previous();
				INVOKE(*CURRENT->cast());
				++count;
				CURRENT = NEXT;
			}
			return count;
		}

		NumInSequence			iterate_backwards_until(const InvokeConstUntil&		INVOKE) const
		{
			NumInSequence			count	= 0;
			const MySequenceable*	CURRENT = m_loop.raw_previous();
			while(!is_end(CURRENT))
			{
				const MySequenceable* NEXT = CURRENT->raw_previous(*this);
				++count;
				if(INVOKE(*CURRENT->cast())) break;			
				CURRENT = NEXT;
			}
			return count;
		}

		NumInSequence			for_each(				const InvokeConst&			INVOKE) const
		{
			return Sequence::iterate_forward(INVOKE);
		}

		NumInSequence			for_each_until(			const InvokeConstUntil&		INVOKE) const
		{
			return Sequence::iterate_forward_until(INVOKE);
		}

	protected:	// [FUNCTIONS]
		T*						first()
		{
			return m_loop.next(*this);
		}

		T*						last()
		{
			return m_loop.previous(*this);
		}

		void					add_front(				MySequenceable&				newObject)
		{
			MySequenceable* next = m_loop.raw_next();
			is_end(next)? newObject.attach(m_loop, m_loop)
						: newObject.attach(m_loop, *next);			
		}

		void					add_back(				MySequenceable&				newObject)
		{
			MySequenceable* prev = m_loop.raw_previous();
			is_end(prev)? newObject.attach(m_loop, m_loop)
						: newObject.attach(*prev, m_loop);
		}

		void					remove_all()
		{
			MySequenceable* current = m_loop.raw_next();
			while(!is_end(current))
			{
				current->remove_from_sequence();
			}
		}

		NumInSequence			iterate_forward(		const Invoke&				INVOKE)
		{
			NumInSequence	count	= 0;
			MySequenceable* current = m_loop.raw_next();
			while(!is_end(current))
			{
				MySequenceable* next = current->raw_next();
				INVOKE(*current->cast());
				++count;
				current = next;
			}
			return count;
		}

		NumInSequence			iterate_forward_until(	const InvokeUntil&			INVOKE)
		{
			NumInSequence	count	= 0;
			MySequenceable* current = m_loop.raw_next();
			while(!is_end(current))
			{
				MySequenceable* next = current->raw_next();
				++count;
				if(INVOKE(*current->cast())) break;			
				current = next;
			}
			return count;
		}

		NumInSequence			iterate_backwards(		const Invoke&				INVOKE)
		{
			NumInSequence	count	= 0;
			MySequenceable* current = m_loop.raw_previous();
			while(!is_end(current))
			{
				MySequenceable* next = current->raw_previous();
				INVOKE(*current->cast());
				++count;
				current = next;
			}
			return count;
		}

		NumInSequence			iterate_backwards_until(const InvokeUntil&			INVOKE)
		{
			NumInSequence	count	= 0;
			MySequenceable* current = m_loop.raw_previous();
			while(!is_end(current))
			{
				MySequenceable* next = current->raw_previous();
				++count;
				if(INVOKE(*current->cast())) break;			
				current = next;
			}
			return count;
		}

		NumInSequence			for_each(				const Invoke&				INVOKE)
		{
			return Sequence::iterate_forward(INVOKE);
		}

		NumInSequence			for_each_until(			const InvokeUntil&			INVOKE)
		{
			return Sequence::iterate_forward_until(INVOKE);
		}

		/*
			Sorts objects using insertion sort algorithm.
			Returns number of times objects have been swapped.
		*/
		template<typename CompT>
		uint32_t				sort(					CompT&&						COMPARE)
		{
			uint32_t		numSwapps	= 0;
			MySequenceable*	current		= m_loop.raw_next();

			while(!is_end(current))
			{
				MySequenceable* prev	= current->raw_previous();	// Current will be compared with the link before it.
				MySequenceable* next	= current->raw_next();		// We need to store next link(future current) before we start.
				T& tmp	= *current->cast();

				current->detach();

				while(!is_end(prev))
				{
					if(COMPARE(tmp, *prev->cast()))
					{
						++numSwapps;
						prev = prev->raw_previous();
					}
					else
					{
						break;
					}
				}

				if(prev)
				{
					current->attach_back(*prev);
				}

				// Sort next link.
				current = next;
			}

			return numSwapps;

			// OLD SOLUTION
			/*
			uint32_t		numSwapps	= 0;
			MySequenceable*	current		= m_loop.raw_next();

			while(!is_end(current))
			{
				MySequenceable* prev	= current->raw_previous();	// Current will be compared with the link before it.
				MySequenceable* next	= current->raw_next();		// We need to store next link(future current) before we start.
				T& tmp	= *current->cast();

				while(!is_end(prev))
				{
					if(COMPARE(tmp, *prev->cast()))
					{
						prev->get_raw_prev()->Next<T, ID>::link(*current);
						current->get_raw_next()->Previous<T, ID>::link(*prev);
						current->Next<T, ID>::link(*prev);
						++numSwapps;
						prev = current->raw_previous();
					}
					else
					{
						break;
					}
				}

				// Sort next link.
				current = next;
			}

			return numSwapps;
			*/
		}
	};
}

#pragma pack(pop)