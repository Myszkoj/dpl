#pragma once


#include <stdint.h>
#include <memory>
#include "dpl_Buffer.h"
#include "dpl_ReadOnly.h"


namespace dpl
{
	template<typename T>
	concept is_unsigned_integer = std::is_unsigned_v<T> && std::is_integral_v<T>;

	template<typename T, typename index_t = uint32_t> requires is_unsigned_integer<index_t>
	class	Holder;


	template<typename T, typename index_t = uint32_t> requires is_unsigned_integer<index_t>
	class	Distributor
	{
	public: // subtypes
		static const index_t MASK_BITS		= 2;
		static const index_t INDEX_BITS		= 8 * sizeof(index_t) - MASK_BITS;
		static const index_t MAX_INDEX		= (1 << INDEX_BITS) - 1;
		static const index_t INVALID_INDEX	= MAX_INDEX;

		enum	ItemState
		{
			INVALID		= 0,
			AVAILABLE	= 1,
			RENTED		= 2
		};

		class	Reference
		{
		public: // data
			index_t state	: MASK_BITS; //<-- Mapped with item at the same index.
			index_t index	: INDEX_BITS;//<-- Belongs to the array of rented or available indices.

		public: // lifecycle
			CLASS_CTOR	Reference()
				: state(ItemState::INVALID)
				, index(INVALID_INDEX)
			{

			}
		};

		using	References	= std::unique_ptr<Reference[]>;
		using	MyHolder	= Holder<T, index_t>;

	public: // friends
		friend	MyHolder;

	public: // data
		ReadOnly<index_t, Distributor> numAvailable;
		ReadOnly<index_t, Distributor> numRented;

	private: // data
		Buffer<T>	m_data;	// [AVAILABLE_ITEMS][RENTED_ITEMS][FREE_SPACE]
		References	m_references;

	public: // lifecycle
		CLASS_CTOR					Distributor(			const index_t			INITIAL_CAPACITY = 0)
			: numAvailable(0)
			, numRented(0)
			, m_data(INITIAL_CAPACITY)
		{
			validate_capacity(INITIAL_CAPACITY);
			if(INITIAL_CAPACITY > 0)
			{
				m_references = std::make_unique<Reference[]>(INITIAL_CAPACITY);
				for(index_t index = 0; index < INITIAL_CAPACITY; ++index)
				{
					m_references[index].index = index;
				}
			}
		}

		CLASS_CTOR					Distributor(			Distributor&&			other) noexcept
			: numAvailable(other.numAvailable)
			, numRented(other.numRented)
			, m_data(std::move(other.m_data))
			, m_references(std::move(other.m_references))
		{
			other.numAvailable = 0;
			other.numRented	= 0;
		}

		CLASS_CTOR					Distributor(			const Distributor&		OTHER)
			: Distributor(0)
		{
			copy_blind(OTHER);
		}

		CLASS_DTOR					~Distributor()
		{
			remove_all_items();
		}

		inline Distributor&			operator=(				const Distributor&		OTHER)
		{
			copy(OTHER);
			return *this;
		}

		Distributor&				operator=(				Distributor&&			other) noexcept
		{		
			numAvailable		= other.numAvailable;
			numRented			= other.numRented;
			m_data			= std::move(other.m_data);
			m_references	= std::move(other.m_mask);
			other.numAvailable = 0;
			other.numRented	= 0;
			return *this;
		}

	public: // functions
		inline index_t				capacity() const
		{
			return m_data.capacity();
		}

		inline index_t				size() const
		{
			return numAvailable() + numRented();
		}

		inline index_t				space() const
		{
			return capacity() - size();
		}

		inline bool					has_available_items() const
		{
			return numAvailable() > 0;
		}

		inline bool					has_free_space() const
		{
			return space() > 0;
		}

		inline bool					empty() const
		{
			return size() == 0;
		}

		inline MyHolder				get_first_available_item()
		{
			return MyHolder(*this);
		}

		template<typename... CTOR>
		T&							emplace_item(			CTOR&&...			args)
		{
			if(!space()) make_room();
			const index_t INDEX = get_next_free_index();
			set_item_state(INDEX, ItemState::AVAILABLE);
			return m_data.construct_at(INDEX, std::forward<CTOR>(args)...);
		}

		void						remove_item(			MyHolder&			holder)
		{
			decrement_numRented();
			const index_t INDEX = holder.release_index();
			set_item_state(INDEX, ItemState::INVALID);
			add_free_index(INDEX);
			m_data.destroy_at(INDEX);
		}

		void						remove_available_items()
		{
			while(has_available_items())
			{
				const index_t INDEX = get_next_available_index();
				m_data.destroy_at(INDEX);
				set_item_state(INDEX, ItemState::INVALID);
				add_free_index(INDEX);
			}
		}

		inline void					remove_all_items()
		{
			validate_not_used();
			remove_available_items();
		}

		void						reserve(				const index_t		NEW_CAPACITY)
		{
			if(NEW_CAPACITY > m_data.capacity())
			{
				validate_capacity(NEW_CAPACITY);

				const index_t OLD_CAPACITY = capacity();
				
				Buffer<T>	newData(NEW_CAPACITY);
				References	newReferences = std::make_unique<Reference[]>(NEW_CAPACITY);

				for(index_t index = 0; index < OLD_CAPACITY; ++index)
				{
					const auto& OLD_REFERENCE = m_references[index];
					if(OLD_REFERENCE.state != ItemState::INVALID)
					{
						newData.construct_at(index, std::move(m_data[index]));
						m_data.destroy_at(index);
					}
					newReferences[index] = OLD_REFERENCE;
				}
				for(index_t index = OLD_CAPACITY; index < NEW_CAPACITY; ++index)
				{
					newReferences[index].index = index;
				}
				m_data.swap(newData);
				m_references.swap(newReferences);
			}
		}

		inline void					enlarge(				const index_t		AMOUNT)
		{
			reserve(capacity() + AMOUNT);
		}

		void						copy(					const Distributor&	OTHER)
		{
			validate_not_self();
			validate_not_used();
			remove_all_items();
			copy_blind(OTHER);
		}

		void						swap(					Distributor&		other)
		{
			validate_not_self();
			numAvailable.swap(other.numAvailable);
			numRented.swap(other.numRented);
			m_data.swap(other.m_data);
			m_references.swap(other.m_references);
		}

		inline bool					valid_at(				const index_t		INDEX) const
		{
			return (INDEX < capacity()) ? m_references[INDEX].state != ItemState::INVALID : false;
		}

	private: // validation
		inline void					validate_capacity(		const index_t		CAPACITY) const
		{
#ifdef _DEBUG
			if(MAX_INDEX+1 <= CAPACITY)
				throw GeneralException(this, __LINE__, "Array size to big");
#endif
		}

		inline void					validate_at(			const index_t		INDEX) const
		{
#ifdef _DEBUG
			if(!valid_at(INDEX))
				throw GeneralException(this, __LINE__, "Invalid index");
#endif
		}

		inline void					validate_not_used() const
		{
#ifdef _DEBUG
			if(numRented() > 0)
				throw GeneralException(this, __LINE__, "Library in use");
#endif // _DEBUG
		}

		inline void					validate_not_self(		const Distributor&	OTHER) const
		{
#ifdef _DEBUG
			if(this == &OTHER) 
				throw GeneralException(this, __LINE__, "Self reference");
#endif // _DEBUG
		}

		inline index_t				decrement_numRented()
		{
#ifdef _DEBUG
			if(numRented() == 0)
				throw GeneralException(this, __LINE__, "Invalid numRented decrementation.");
#endif // _DEBUG

			return --(*numRented);
		}

		inline index_t				decrement_numAvailable()
		{
#ifdef _DEBUG
			if(numAvailable() == 0)
				throw GeneralException(this, __LINE__, "Invalid numAvailable decrementation.");
#endif // _DEBUG

			return --(*numAvailable);
		}

	private: // internal resize
		inline void					make_room()
		{
			const index_t CURRENT_SIZE = size();
			(CURRENT_SIZE > 0) ? enlarge(CURRENT_SIZE) : reserve(2);
		}

	private: // copy functions
		inline void					copy_info(				const Distributor&	OTHER)
		{
			numAvailable	= OTHER.numAvailable;
			numRented		= OTHER.numRented;
		}

		inline void					copy_references(		const Distributor&	OTHER)
		{
			m_references = std::make_unique<Reference[]>(OTHER.capacity());
			std::memcpy(m_references.get(), OTHER.m_references.get(), sizeof(Reference) * OTHER.capacity());
		}

		inline void					copy_data(				const Distributor&	OTHER)
		{
			Buffer<T> newData(OTHER.capacity());
			for(index_t index = 0; index < OTHER.capacity(); ++index)
				if(OTHER.valid_at(index))
					newData.construct_at(index, OTHER.m_data[index]);

			m_data.swap(newData);
		}

		inline void					copy_blind(				const Distributor&	OTHER)
		{
			copy_info(OTHER);
			copy_references(OTHER);
			copy_data(OTHER);
		}

	private: // index and state manipulation
		inline void					set_item_state(			const index_t		INDEX,
															const ItemState		STATE)
		{
			m_references[INDEX].state = STATE;
		}

		inline index_t				get_next_available_index()
		{
#ifdef _DEBUG
			if(!has_available_items())
				throw GeneralException(this, __LINE__, "No available items.");
#endif // _DEBUG
			return m_references[decrement_numAvailable()].index;
		}

		inline index_t				get_next_free_index()
		{
#ifdef _DEBUG
			if(!has_free_space())
				throw GeneralException(this, __LINE__, "No free space for items.");
#endif // _DEBUG
			const index_t INDEX = m_references[size()].index;
			m_references[(*numAvailable)++].index = INDEX; //<-- Add index to the available stack.
			return INDEX;		
		}

		/*
			Put index on the "free space" stack(right side of the buffer).
			NOTE: Must be done AFTER numAvailable or numRented was decremented.
		*/
		inline void					add_free_index(			const index_t		INDEX)
		{
#ifdef _DEBUG
			if(INDEX >= capacity())
				throw GeneralException(this, __LINE__, "Invalid free index.");
#endif // _DEBUG
			m_references[size()].index = INDEX;
		}

		/*
			Put index on the "available" stack(left side of the buffer).
			NOTE: This function increments numAvailable.
		*/
		inline void					add_available_index(	const index_t		INDEX)
		{
			m_references[(*numAvailable)++].index = INDEX;
		}

	private: // holder functions
		index_t						rent_item()
		{
			const index_t INDEX = get_next_available_index();
			set_item_state(INDEX, ItemState::RENTED);
			++(*numRented);
			return INDEX;
		}

		void						return_item(			const index_t		INDEX)
		{
			if(!valid_at(INDEX)) return; // Ignore if item is already invalid.
			set_item_state(INDEX, ItemState::AVAILABLE);
			add_available_index(INDEX);
			decrement_numRented();
		}

		inline T&					access_item_at(			const index_t		INDEX)
		{
			//validate_at(INDEX);
			return m_data[INDEX];
		}
	};


	template<typename T, typename index_t> requires is_unsigned_integer<index_t>
	class	Holder //<-- Requires C++17
	{
	public: // subtypes
		using	MyDistributor = Distributor<T, index_t>;

	public: // friends
		friend	MyDistributor;

	private: // data
		MyDistributor&	m_distributor;

	public: // data
		ReadOnly<index_t, Holder> itemID;

	private: // lifecycle
		CLASS_CTOR			Holder(		MyDistributor&	distributor)
			: m_distributor(distributor)
			, itemID(distributor.rent_item())
		{
			
		}

		CLASS_CTOR			Holder(		const Holder&	OTHER) = delete;

		CLASS_CTOR			Holder(		Holder&&		other) noexcept = delete;

		Holder&				operator=(	const Holder&	OTHER) = delete;

		Holder&				operator=(	Holder&&		other) noexcept = delete;

	public: // lifecycle
		CLASS_DTOR			~Holder()
		{
			m_distributor.return_item(itemID);
		}

	public: // item access
		inline T&			get()
		{
			return m_distributor.access_item_at(itemID);
		}

		inline operator		T&()
		{
			return get();
		}

		inline T*			operator->()
		{
			return get();
		}

	private: // functions
		inline index_t		release_index()
		{
			const index_t TMP = itemID;
			itemID = std::numeric_limits<index_t>::max();
			return TMP;
		}
	};
}