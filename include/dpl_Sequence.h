#pragma once


#include "dpl_Association.h"
#include <functional>


#pragma pack(push, 4)

// declarations
namespace dpl
{
	template<typename T, uint32_t ID = 0>
	class Sequenceable;

	template<typename T, uint32_t ID = 0>
	class PrevInSequence;

	template<typename T, uint32_t ID = 0>
	class NextInSequence;

	template<typename T, uint32_t ID = 0>
	class Sequence;
}

// implemantations
namespace dpl
{
	/*
		Interface of the Sequenceable class.
		Association with the previous in sequence.
	*/
	template<typename T, uint32_t ID>
	class PrevInSequence : private Association<PrevInSequence<T, ID>, NextInSequence<T, ID>, ID>
	{
	private: // subtypes
		using MyBase = Association<PrevInSequence<T, ID>, NextInSequence<T, ID>, ID>;

	public: // relations
		friend MyBase;
		friend Association<NextInSequence<T, ID>, PrevInSequence<T, ID>, ID>;
		friend Sequence<T, ID>;
		friend Sequenceable<T, ID>;

	protected: // lifecycle
		inline PrevInSequence&				operator=(Swap<PrevInSequence> other)
		{
			return static_cast<PrevInSequence&>(MyBase::operator=(Swap(other)));
		}

	private: // functions
		inline Sequenceable<T, ID>*			get_prev()
		{
			return static_cast<Sequenceable<T, ID>*>(MyBase::other());
		}

		inline const Sequenceable<T, ID>*	get_prev() const
		{
			return static_cast<const Sequenceable<T, ID>*>(MyBase::other());
		}
	};


	/*
		Interface of the Sequenceable class.
		Association with the next in sequence.
	*/
	template<typename T, uint32_t ID>
	class NextInSequence : private Association<NextInSequence<T, ID>, PrevInSequence<T, ID>, ID>
	{
	private: // subtypes
		using MyBase = Association<NextInSequence<T, ID>, PrevInSequence<T, ID>, ID>;

	public: // relations
		friend MyBase;
		friend Association<PrevInSequence<T, ID>, NextInSequence<T, ID>, ID>;
		friend Sequence<T, ID>;
		friend Sequenceable<T, ID>;

	protected: // lifecycle
		inline NextInSequence&				operator=(Swap<NextInSequence> other)
		{
			return static_cast<NextInSequence&>(MyBase::operator=(Swap(other)));
		}

	private: // functions
		inline Sequenceable<T, ID>*			get_next()
		{
			return static_cast<Sequenceable<T, ID>*>(MyBase::other());
		}

		inline const Sequenceable<T, ID>*	get_next() const
		{
			return static_cast<const Sequenceable<T, ID>*>(MyBase::other());
		}
	};


	template<typename T, uint32_t ID>
	class Sequenceable	: public PrevInSequence<T, ID>
						, public NextInSequence<T, ID>
	{
	private: // subtypes
		using MySequence	= Sequence<T, ID>;
		using MyPrev		= PrevInSequence<T, ID>;
		using MyNext		= NextInSequence<T, ID>;

	public: // relations
		friend MySequence;

	protected: // lifecycle
		CLASS_CTOR				Sequenceable() = default;

		CLASS_CTOR				Sequenceable(	const Sequenceable&		OTHER) = delete;

		CLASS_CTOR				Sequenceable(	Sequenceable&&			other) noexcept
			: MyPrev(std::move(other))
			, MyNext(std::move(other))
		{
			
		}

		CLASS_DTOR				~Sequenceable()
		{
			dpl::no_except([&](){	remove_from_sequence();	});
		}

		Sequenceable&			operator=(		const Sequenceable&		OTHER) = delete;

		inline Sequenceable&	operator=(		Sequenceable&&			other) noexcept
		{
			remove_from_sequence();
			MyPrev::operator=(std::move(other));
			MyNext::operator=(std::move(other));
			return *this;
		}

		inline Sequenceable&	operator=(		Swap<Sequenceable>		other)
		{
			MyPrev::operator=(Swap<MyPrev>(*other));
			MyNext::operator=(Swap<MyNext>(*other));
			return *this;
		}

		inline Sequenceable&	operator=(		Swap<T>					other)
		{
			return operator=(Swap<Sequenceable>(*other));
		}

	public: // functions
		/*
			Get previous object in sequence.
		*/
		inline T*				previous(		const MySequence&		SEQUENCE)
		{
			return SEQUENCE.is_end(MyPrev::get_prev()) ? nullptr : MyPrev::get_prev()->cast();
		}

		/*
			Get previous object in sequence.
		*/
		inline const T*			previous(		const MySequence&		SEQUENCE) const
		{
			return SEQUENCE.is_end(MyPrev::get_prev()) ? nullptr : MyPrev::get_prev()->cast();
		}

		/*
			Get next object in sequence.
		*/
		inline T*				next(			const MySequence&		SEQUENCE)
		{
			return SEQUENCE.is_end(MyNext::get_next()) ? nullptr : MyNext::get_next()->cast();
		}

		/*
			Get next object in sequence.
		*/
		inline const T*			next(			const MySequence&		SEQUENCE) const
		{
			return SEQUENCE.is_end(MyNext::get_next()) ? nullptr : MyNext::get_next()->cast();
		}

		/*
			Returns true if this object is part of the sequence.
		*/
		inline bool				is_part_of_sequence() const
		{
			return MyPrev::get_prev() != nullptr; // && MyNext::target(); //<-- Second condition is redundant.
		}

	protected: // functions
		/*
			Remove this object from the current sequence.
		*/
		inline void				remove_from_sequence()
		{
			if(MyPrev::get_prev() != MyNext::get_next())
			{
				MyPrev::other()->link(*MyNext::other());
			}
			else // This was the last object in sequence, we have to break the loop.
			{
				MyPrev::unlink();
				MyNext::unlink();
			}
		}
	
	private: // functions
		inline void				insert(			MyNext&					prev,
												MyPrev&					next)
		{
			prev.link(*this);
			next.link(*this);
		}

		inline T*				cast()
		{
			return static_cast<T*>(this);
		}

		inline const T*			cast() const
		{
			return static_cast<const T*>(this);
		}
	};


	/*
		Sequence of T objects.
	*/
	template<typename T, uint32_t ID>
	class Sequence
	{
	private: // subtypes
		using MySequenceable = Sequenceable<T, ID>;

	public: // subtypes
		using NumInSequence = uint32_t;

	private: // data
		MySequenceable	m_loop;

	protected: // lifecycle
		CLASS_CTOR				Sequence() = default;

		CLASS_CTOR				Sequence(				const Sequence&							OTHER) = delete;

		CLASS_CTOR				Sequence(				Sequence&&								other) noexcept = default;

		CLASS_DTOR				~Sequence() = default;

		inline Sequence&		operator=(				const Sequence&							OTHER) = delete;

		inline Sequence&		operator=(				Sequence&&								other) noexcept = default;

		inline Sequence&		operator=(				Swap<Sequence>							other)
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
		inline bool				is_end(					const MySequenceable*					OBJ) const
		{
			return OBJ == &m_loop || OBJ == nullptr;
		}

		NumInSequence			iterate_forward(		const std::function<void(const T&)>&	FUNCTION) const
		{
			NumInSequence			count = 0;
			const MySequenceable*	CURRENT = m_loop.get_next();
			while(!is_end(CURRENT))
			{
				const MySequenceable* NEXT = CURRENT->get_next();
				FUNCTION(*CURRENT->cast());
				++count;
				CURRENT = NEXT;
			}
			return count;
		}

		NumInSequence			iterate_forward_until(	const std::function<bool(const T&)>&	FUNCTION) const
		{
			NumInSequence			count = 0;
			const MySequenceable*	CURRENT = m_loop.get_next();
			while(!is_end(CURRENT))
			{
				const MySequenceable* NEXT = CURRENT->get_next();
				++count;
				if(FUNCTION(*CURRENT->cast())) break;			
				CURRENT = NEXT;
			}
			return count;
		}

		NumInSequence			iterate_backwards(		const std::function<void(const T&)>&	FUNCTION) const
		{
			NumInSequence			count = 0;
			const MySequenceable*	CURRENT = m_loop.get_prev();
			while(!is_end(CURRENT))
			{
				const MySequenceable* NEXT = CURRENT->get_prev();
				FUNCTION(*CURRENT->cast());
				++count;
				CURRENT = NEXT;
			}
			return count;
		}

		NumInSequence			iterate_backwards_until(const std::function<bool(const T&)>&	FUNCTION) const
		{
			NumInSequence			count = 0;
			const MySequenceable*	CURRENT = m_loop.get_prev();
			while(!is_end(CURRENT))
			{
				const MySequenceable* NEXT = CURRENT->get_prev();
				++count;
				if(FUNCTION(*CURRENT->cast())) break;			
				CURRENT = NEXT;
			}
			return count;
		}

		inline NumInSequence	for_each(				const std::function<void(const T&)>&	FUNCTION) const
		{
			return Sequence::iterate_forward(FUNCTION);
		}

		inline NumInSequence	for_each_until(			const std::function<bool(const T&)>&	FUNCTION) const
		{
			return Sequence::iterate_forward_until(FUNCTION);
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
		void					add_front(				MySequenceable&							newObject)
		{
			newObject.remove_from_sequence();
			m_loop.is_part_of_sequence()	? newObject.insert(m_loop, *m_loop.get_next())
											: newObject.insert(m_loop, m_loop);			
		}

		/*
			Adds given object at the end of the sequence.
		*/
		void					add_back(				MySequenceable&							newObject)
		{
			newObject.remove_from_sequence();
			m_loop.is_part_of_sequence()	? newObject.insert(*m_loop.get_prev(), m_loop)
											: newObject.insert(m_loop, m_loop);	
		}

		void					remove_all()
		{
			MySequenceable* current = m_loop.get_next();
			while(!is_end(current))
			{
				current->remove_from_sequence();
				current = m_loop.get_next();
			}
		}

		NumInSequence			iterate_forward(		const std::function<void(T&)>&			function)
		{
			NumInSequence	count	= 0;
			MySequenceable* current = m_loop.get_next();
			while(!is_end(current))
			{
				MySequenceable* next = current->get_next();
				function(*current->cast());
				++count;
				current = next;
			}
			return count;
		}

		NumInSequence			iterate_forward_until(	const std::function<bool(T&)>&			function)
		{
			uint32_t		count	= 0;
			MySequenceable* current = m_loop.get_next();
			while(!is_end(current))
			{
				MySequenceable* next = current->get_next();
				++count;
				if(function(*current->cast())) break;			
				current = next;
			}
			return count;
		}

		NumInSequence			iterate_backwards(		const std::function<void(T&)>&			function)
		{
			NumInSequence	count	= 0;
			MySequenceable* current = m_loop.get_prev();
			while(!is_end(current))
			{
				MySequenceable* next = current->get_prev();
				function(*current->cast());
				++count;
				current = next;
			}
			return count;
		}

		NumInSequence			iterate_backwards_until(const std::function<bool(T&)>&			function)
		{
			uint32_t		count	= 0;
			MySequenceable* current = m_loop.get_prev();
			while(!is_end(current))
			{
				MySequenceable* next = current->get_prev();
				++count;
				if(function(*current->cast())) break;			
				current = next;
			}
			return count;
		}

		inline NumInSequence	for_each(				const std::function<void(T&)>&			function)
		{
			return iterate_forward(function);
		}

		inline NumInSequence	for_each_until(			const std::function<bool(T&)>&			function)
		{
			return iterate_forward_until(function);
		}

		/*
			Sorts objects using insertion sort algorithm.
			Returns number of times objects have been swapped.
		*/
		template<typename CompT>
		uint32_t				sort(					CompT&&									COMPARE)
		{
			uint32_t	numSwapps	= 0;
			auto*		current		= m_loop.get_next();

			while(!is_end(current))
			{
				auto* prev	= current->get_prev();	// Current will be compared with the link before it.
				auto* next	= current->get_next();	// We need to store next link(future current) before we start.
				auto& tmp	= *current->cast();		// We can cast current before we start to make it faster.

				while(prev != &m_loop)
				{
					if(COMPARE(tmp, *prev->cast()))
					{
						prev->get_prev()->NextInSequence<T, ID>::link(*current);
						current->get_next()->PrevInSequence<T, ID>::link(*prev);
						current->NextInSequence<T, ID>::link(*prev);
						++numSwapps;
						prev = current->get_prev();
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