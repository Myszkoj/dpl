#pragma once


#include <stdint.h>
#include <limits>
#include <vector>
#include <type_traits>
#include "dpl_ReadOnly.h"


namespace dpl
{
	template<typename T>
	class Indexer;


#pragma pack(push, 1)
	/*
		Object with unique ID assigned by the Indexer<T> class.
	*/
	template<typename T>
	class Indexable
	{
	public: // relations
		friend Indexer<T>;

	public: // constants
		static const uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

	private: // data
		Indexer<T>* m_indexer;

	public: // data
		ReadOnly<uint32_t, Indexable> index;

	public: // lifecycle
		CLASS_CTOR		Indexable()
			: m_indexer(nullptr)
			, index(INVALID_INDEX)
		{

		}

		CLASS_CTOR		Indexable(		Indexer<T>&			indexer)
			: Indexable()
		{
			indexer.add(*this);
		}

		CLASS_CTOR		Indexable(		const Indexable<T>&	OTHER) = delete;

		CLASS_CTOR		Indexable(		Indexable&&			other) noexcept
			: m_indexer(other.m_indexer)
			, index(other.index)
		{
			if(m_indexer)
			{
				m_indexer->update_at(index, this);
				other.reset();
			}
		}

		Indexable&		operator=(		const Indexable&	OTHER) = delete;

		Indexable&		operator=(		Indexable&&			other) noexcept
		{
			if(m_indexer)
			{
				m_indexer->remove_internal(*this);
			}

			if(other.m_indexer)
			{
				index		= other.index;
				m_indexer	= other.m_indexer;
				m_indexer->update_at(index, this);
				other.reset();
			}

			return *this;
		}

	protected: // lifecycle
		CLASS_DTOR		~Indexable()
		{
			if(m_indexer)
			{
				m_indexer->remove_internal(*this);
			}
		}

	public: // functions
		inline bool		is_valid() const
		{
			return index() != INVALID_INDEX;
		}

		inline bool		is_invalid() const
		{
			return index() == INVALID_INDEX;
		}

		/*
			Removes indexable from the index group.
		*/
		inline void		invalidate()
		{
			if(m_indexer)
			{
				m_indexer->remove_internal(*this);
			}
		}

	private: // functions
		inline void		initialize(		Indexer<T>*			indexer)
		{
			index		= indexer->add(*this);
			m_indexer	= indexer;
		}

		inline void		reset()
		{
			m_indexer	= nullptr;
			index		= INVALID_INDEX;
		}

		inline void		update_index(	const uint32_t		NEW_INDEX)
		{
			index = NEW_INDEX;
		}

		inline T&		cast()
		{
			return *static_cast<T*>(this);
		}

		inline const T&	cast() const
		{
			return *static_cast<T*>(this);
		}
	};
#pragma pack(pop)


	/*
		Assigns unique IDs to the indexable objects.
	*/
	template<typename T>
	class Indexer
	{
	public: // subtypes
		using Indexables = std::vector<Indexable<T>*>;

	public: // relations
		friend Indexable<T>;

	public: // lifecycle
		CLASS_CTOR		Indexer() = default;

		CLASS_CTOR		Indexer(			const Indexer&	OTHER) = delete;

		CLASS_CTOR		Indexer(			Indexer&&		other) noexcept
			: m_indexables(std::move(other.m_indexables))
		{
			notify_moved();
		}

		CLASS_DTOR		~Indexer()
		{
			remove_all();
		}

		Indexer&		operator=(			const Indexer&	OTHER) = delete;

		Indexer&		operator=(			Indexer&&		other) noexcept
		{
			if(this != &other)
			{
				remove_all();
				m_indexables = std::move(other.m_indexables);
				notify_moved();
			}

			return *this;
		}

	public: // functions
		inline uint32_t size() const
		{
			return static_cast<uint32_t>(m_indexables.size());
		}

		inline T&		indexable_at(		const uint32_t	INDEX)
		{
			return m_indexables.at(INDEX)->cast();
		}

		inline const T& indexable_at(		const uint32_t	INDEX) const
		{
			return m_indexables.at(INDEX)->cast();
		}

		inline void		add(				Indexable<T>&	indexable)
		{
			if(indexable.m_indexer != this)
			{
				if(indexable.m_indexer != nullptr)
				{
					indexable.m_indexer->remove_internal(indexable);
				}
				
				m_indexables.push_back(&indexable);
				indexable.m_indexer = this;
				indexable.update_index(this->size() - 1u);
			}
		}

		/*
			Performs fast removal by swapping with the last indexable.
		*/
		inline void		remove(				Indexable<T>&	indexable)
		{
			if(indexable.m_indexer == this) remove_internal(indexable);
		}

		inline void		remove_all()
		{
			while(m_indexables.size() > 0) remove_internal(*m_indexables.back());
		}

	private: // functions
		inline void		update_at(			const uint32_t	INDEX,
											Indexable<T>*	newAddress)
		{
			m_indexables[INDEX] = newAddress;
		}

		inline void		notify_moved()
		{
			for(auto& indexable : m_indexables)
			{
				indexable->m_indexer = this;
			}
		}

		void			remove_internal(	Indexable<T>&	indexable)
		{
			if(indexable.index() + 1u < this->size())
			{
				m_indexables.back()->update_index(indexable.index);
				m_indexables[indexable.index] = m_indexables.back();
			}

			indexable.reset();
			m_indexables.pop_back();
		}

	private: // data
		Indexables m_indexables;
	};


	template<typename T>
	class IndexableType;


	class IType
	{
	public: // relations
		template<typename>
		friend class IndexableClass;

		template<typename>
		friend class IndexableType;

	public: // data
		static ReadOnly<uint32_t,	IType> numTypes;

	private: // data
		const uint32_t m_TYPE_INDEX;

	protected: // lifecycle
		CLASS_CTOR			IType(		const uint32_t	TYPE_INDEX)
			: m_TYPE_INDEX(TYPE_INDEX)
		{
			
		}

		CLASS_CTOR			IType(		const IType&	OTHER)
			: IType(OTHER.m_TYPE_INDEX)
		{

		}

		CLASS_CTOR			IType(		IType&&			other) noexcept
			: IType(other.m_TYPE_INDEX)
		{

		}

		inline IType&		operator=(	const IType&	OTHER)
		{
			return *this; // Index stays the same.
		}

		inline IType&		operator=(	IType&&			other) noexcept
		{
			return *this;// Index stays the same.
		}

	public: //functions
		template<typename T>
		inline bool			is_type() const
		{
			static_assert(std::is_base_of<IndexableType<T>, T>::value, "Invalid class type.");
			return m_TYPE_INDEX == IndexableType<T>::typeIndex();
		}

		template<typename T>
		inline T*			cast_to()
		{
			return is_type<T>() ? static_cast<T*>(this) : nullptr;
		}

		template<typename T>
		inline const T*		cast_to() const
		{
			return is_type<T>() ? static_cast<const T*>(this) : nullptr;
		}

	private: // functions
		static uint32_t		assign_type_index()
		{
			return (*numTypes)++;
		}
	};


	template<typename T>
	class IndexableType : public IType
	{
	public: // data
		static ReadOnly<uint32_t, IndexableType> typeIndex;

	public: // lifecycle
		CLASS_CTOR		IndexableType()
			: IType(typeIndex)
		{

		}
	};


	template<typename T>
	ReadOnly<uint32_t, IndexableType<T>> IndexableType<T>::typeIndex = IType::assign_type_index();
}