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
	private: // subtypes
		using	MyBase		= Link<T, ID>;
		using	MySequence	= Sequence<T, ID>;

	public: // friends
		friend	MyBase;
		friend	MySequence;

	private: // functions
		using	MyBase::attach;

	protected: // lifecycle
		CLASS_CTOR				Sequenceable() = default;
		CLASS_CTOR				Sequenceable(	const Sequenceable&		OTHER) = delete;
		CLASS_CTOR				Sequenceable(	Sequenceable&&			other) noexcept = default;
		Sequenceable&			operator=(		const Sequenceable&		OTHER) = delete;
		Sequenceable&			operator=(		Sequenceable&&			other) noexcept = default;

		Sequenceable&			operator=(		Swap<Sequenceable>		other)
		{
			MyBase::operator=(Swap<MyBase>(*other));
			return *this;
		}

		Sequenceable&			operator=(		Swap<T>					other)
		{
			return operator=(Swap<Sequenceable>(*other));
		}

	public: // functions
		inline bool				is_part_of_sequence() const
		{
			return MyBase::is_linked_to_any();
		}

		inline T*				previous(		const MySequence&		SEQUENCE)
		{
			auto* prev = static_cast<Sequenceable*>(MyBase::get_raw_prev());
			return (SEQUENCE.is_end(prev)) ? nullptr : prev->cast();
		}

		inline const T*			previous(		const MySequence&		SEQUENCE) const
		{
			const auto* PREV = static_cast<const Sequenceable*>(MyBase::get_raw_prev());
			return (SEQUENCE.is_end(PREV)) ? nullptr : PREV->cast();
		}

		inline T*				next(			const MySequence&		SEQUENCE)
		{
			auto* next = static_cast<Sequenceable*>(MyBase::get_raw_next());
			return (SEQUENCE.is_end(next)) ? nullptr : next->cast();
		}

		inline const T*			next(			const MySequence&		SEQUENCE) const
		{
			const auto* NEXT = static_cast<const Sequenceable*>(MyBase::get_raw_next());
			return (SEQUENCE.is_end(NEXT)) ? nullptr : NEXT->cast();
		}

	protected: // functions
		inline void				remove_from_sequence()
		{
			MyBase::detach();
		}
	};


	/*
		Sequence of T objects.
	*/
	template<typename T, uint32_t ID>
	class Sequence
	{
	private: // subtypes
		using	MySequenceable		= Sequenceable<T, ID>;
		using	MyLink				= Link<T, ID>;

	public: // subtypes
		using	Invoke				= std::function<void(T&)>;
		using	InvokeConst			= std::function<void(const T&)>;
		using	InvokeUntil			= std::function<bool(T&)>;
		using	InvokeConstUntil	= std::function<bool(const T&)>;
		using	NumInSequence		= uint32_t;

	private: // data
		MySequenceable	m_loop;

	protected: // lifecycle
		CLASS_CTOR				Sequence() = default;
		CLASS_CTOR				Sequence(				const Sequence&				OTHER) = delete;
		CLASS_CTOR				Sequence(				Sequence&&					other) noexcept = default;
		Sequence&				operator=(				const Sequence&				OTHER) = delete;

		Sequence&				operator=(				Sequence&&					other) noexcept = default;

		Sequence&				operator=(				Swap<Sequence>				other)
		{
			m_loop = Swap(other->m_loop);
			return *this;
		}

	public: // functions
		inline const T*			first() const
		{
			return m_loop.next(*this);
		}

		inline const T*			last() const
		{
			return m_loop.previous(*this);
		}

		inline bool				empty() const
		{
			return first() == nullptr;
		}

		/*
			Returns true if object is a nullptr or it points to the internal loop object(end of the sequence).
		*/
		inline bool				is_end(					const MySequenceable*		OBJ) const
		{
			return OBJ == &m_loop || OBJ == nullptr;
		}

		NumInSequence			iterate_forward(		const InvokeConst&			INVOKE) const
		{
			NumInSequence			count = 0;
			const MySequenceable*	CURRENT = first();
			while(!is_end(CURRENT))
			{
				const MySequenceable* NEXT = CURRENT->next(*this);
				INVOKE(*CURRENT->cast());
				++count;
				CURRENT = NEXT;
			}
			return count;
		}

		NumInSequence			iterate_forward_until(	const InvokeConstUntil&		INVOKE) const
		{
			NumInSequence			count = 0;
			const MySequenceable*	CURRENT = first();
			while(!is_end(CURRENT))
			{
				const MySequenceable* NEXT = CURRENT->next(*this);
				++count;
				if(INVOKE(*CURRENT->cast())) break;			
				CURRENT = NEXT;
			}
			return count;
		}

		NumInSequence			iterate_backwards(		const InvokeConst&			INVOKE) const
		{
			NumInSequence			count = 0;
			const MySequenceable*	CURRENT = last();
			while(!is_end(CURRENT))
			{
				const MySequenceable* NEXT = CURRENT->previous(*this);
				INVOKE(*CURRENT->cast());
				++count;
				CURRENT = NEXT;
			}
			return count;
		}

		NumInSequence			iterate_backwards_until(const InvokeConstUntil&		INVOKE) const
		{
			NumInSequence			count = 0;
			const MySequenceable*	CURRENT = last();
			while(!is_end(CURRENT))
			{
				const MySequenceable* NEXT = CURRENT->previous(*this);
				++count;
				if(INVOKE(*CURRENT->cast())) break;			
				CURRENT = NEXT;
			}
			return count;
		}

		inline NumInSequence	for_each(				const InvokeConst&			INVOKE) const
		{
			return Sequence::iterate_forward(INVOKE);
		}

		inline NumInSequence	for_each_until(			const InvokeConstUntil&		INVOKE) const
		{
			return Sequence::iterate_forward_until(INVOKE);
		}

	protected: // functions
		inline T*				first()
		{
			return m_loop.next(*this);
		}

		inline T*				last()
		{
			return m_loop.previous(*this);
		}

		/*
			Adds given object at the front of the sequence.
		*/
		void					add_front(				MySequenceable&				newObject)
		{
			if(MySequenceable* next = first())
			{
				newObject.attach(m_loop, *next);
			}
			else
			{
				newObject.attach(m_loop, m_loop);
			}			
		}

		/*
			Adds given object at the end of the sequence.
		*/
		void					add_back(				MySequenceable&				newObject)
		{
			if(MySequenceable* prev = last())
			{
				newObject.attach(*prev, m_loop);
			}
			else
			{
				newObject.attach(m_loop, m_loop);
			}
		}

		void					remove_all()
		{
			while(MySequenceable* current = first())
			{
				current->remove_from_sequence();
			}
		}

		NumInSequence			iterate_forward(		const Invoke&				INVOKE)
		{
			NumInSequence	count	= 0;
			MySequenceable* current = first();
			while(!is_end(current))
			{
				MySequenceable* next = current->next(*this);
				INVOKE(*current->cast());
				++count;
				current = next;
			}
			return count;
		}

		NumInSequence			iterate_forward_until(	const InvokeUntil&			INVOKE)
		{
			NumInSequence	count	= 0;
			MySequenceable* current = first();
			while(!is_end(current))
			{
				MySequenceable* next = current->next(*this);
				++count;
				if(INVOKE(*current->cast())) break;			
				current = next;
			}
			return count;
		}

		NumInSequence			iterate_backwards(		const Invoke&				INVOKE)
		{
			NumInSequence	count	= 0;
			MySequenceable* current = last();
			while(!is_end(current))
			{
				MySequenceable* next = current->previous(*this);
				INVOKE(*current->cast());
				++count;
				current = next;
			}
			return count;
		}

		NumInSequence			iterate_backwards_until(const InvokeUntil&			INVOKE)
		{
			NumInSequence	count	= 0;
			MySequenceable* current = last();
			while(!is_end(current))
			{
				MySequenceable* next = current->previous(*this);
				++count;
				if(INVOKE(*current->cast())) break;			
				current = next;
			}
			return count;
		}

		inline NumInSequence	for_each(				const Invoke&				INVOKE)
		{
			return Sequence::iterate_forward(INVOKE);
		}

		inline NumInSequence	for_each_until(			const InvokeUntil&			INVOKE)
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
			uint32_t	numSwapps	= 0;
			MyLink*		current		= first();

			while(!is_end(current))
			{
				MyLink* prev	= current->get_raw_prev();	// Current will be compared with the link before it.
				MyLink* next	= current->get_raw_next();	// We need to store next link(future current) before we start.
				T& tmp			= *current->cast();

				while(!is_end(prev))
				{
					if(COMPARE(tmp, *prev->cast()))
					{
						prev->get_raw_prev()->Next<T, ID>::link(*current);
						current->get_raw_next()->Previous<T, ID>::link(*prev);
						current->Next<T, ID>::link(*prev);
						++numSwapps;
						prev = current->get_raw_prev();
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
		}
	};
}

#pragma pack(pop)