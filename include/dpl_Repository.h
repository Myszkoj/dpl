#pragma once


#include <vector>
#include <stdint.h>
#include <type_traits>
#include <functional>
#include <fstream>
#include "dpl_Buffer.h"
#include "dpl_Binary.h"


namespace dpl
{
	template<typename T, typename index_t = uint32_t>
	struct	DefaultImport
	{
		CLASS_CTOR	DefaultImport() = default;

		inline void operator()(T& data, std::istream& binary)
		{
			dpl::import_t(binary, data);
		}
	};

	template<typename T, typename index_t = uint32_t>
	struct	DefaultExport
	{
		CLASS_CTOR	DefaultExport() = default;

		inline void operator()(const T& DATA, std::ostream& binary)
		{
			dpl::export_t(binary, DATA);
		}
	};


	/**
		Stores data in continuous memory block.
		Elements can be removed without invalidating the iterators.
		T must have default constructor.
	*/
	template<typename T, typename index_t = uint32_t>
	class	Repository
	{
	public: // subtypes
		static const index_t MASK_BITS		= 2;
		static const index_t INDEX_BITS		= 8 * sizeof(index_t) - MASK_BITS;
		static const index_t MAX_INDEX		= (1 << INDEX_BITS) - 1;
		static const index_t INVALID_INDEX	= MAX_INDEX;

		enum	EntryState
		{
			INVALID = 0,
			VALID	= 1
		};

		class	Reference
		{
		public: // data
			index_t bValid	: MASK_BITS;
			index_t index	: INDEX_BITS;

		public: // lifecycle
			CLASS_CTOR Reference(const EntryState STATE = EntryState::INVALID)
				: bValid(STATE)
				, index(INVALID_INDEX)
			{

			}
		};

		class	Iterator
		{
		public: // relations
			friend Repository;

		private: // data
			Repository*	m_repository;
			index_t		m_entryID;

		private: // lifecycle
			CLASS_CTOR		Iterator(	Repository*		organizer, 
										index_t			entryID)
				: m_repository(organizer)
				, m_entryID(entryID)
			{
				validate();
			}

		public: // operators
			inline bool		operator==(	const Iterator&	OTHER) const
			{
				return m_entryID != OTHER.m_entryID;
			}

			inline bool		operator!=(	const Iterator&	OTHER) const
			{
				return m_entryID != OTHER.m_entryID;
			}

			inline void		operator++()
			{
				m_repository->validate_at(m_entryID);
				m_entryID = m_repository->get_next_valid_index(m_entryID);
			}
			
			inline T&		operator*()
			{
				return m_repository->get(m_entryID);
			}

			inline T*		operator->()
			{
				return &m_repository->get(m_entryID);
			}

			inline T*		get()
			{
				return &m_repository->get(m_entryID);
			}

			inline index_t	index() const
			{
				return m_entryID;
			}

		private: // functions
			inline void		validate() const
			{
#ifdef _DEBUG
				if(!m_repository)
					throw GeneralException(this, __LINE__, "Iterator was not initialized.");
#endif
			}
		};

		class	ConstIterator
		{
		public: // relations
			friend Repository;

		private: // data
			const Repository*	m_repository;
			index_t				m_entryID;

		private: // lifecycle
			CLASS_CTOR		ConstIterator(	const Repository*		REPOSITORY, 
											index_t					entryID)
				: m_repository(REPOSITORY)
				, m_entryID(entryID)
			{
				validate();
			}

		public: // operators
			inline bool		operator==(		const ConstIterator&	OTHER) const
			{
				return m_entryID == OTHER.m_entryID;
			}

			inline bool		operator!=(		const ConstIterator&	OTHER) const
			{
				return m_entryID != OTHER.m_entryID;
			}

			inline void		operator++()
			{
				m_repository->validate_at(m_entryID);
				m_entryID = m_repository->get_next_valid_index(m_entryID);
			}
			
			inline const T&	operator*()
			{
				return m_repository->get(m_entryID);
			}

			inline const T*	operator->()
			{
				return &m_repository->get(m_entryID);
			}

		public: // functions
			inline const T*	get() const
			{
				return &m_repository->get(m_entryID);
			}

			inline index_t	index() const
			{
				return m_entryID;
			}

		private: // functions
			inline void		validate() const
			{
#ifdef _DEBUG
				if(!m_repository)
					throw GeneralException(this, __LINE__, "Iterator was not initialized.");
#endif
			}
		};

		using	References	= std::vector<Reference>;

	private: // data
		Buffer<T>	m_data;
		References	m_references;
		index_t		m_freeBuckets;

	public: // lifecycle
		CLASS_CTOR									Repository(				const index_t		INITIAL_CAPACITY = 0)
			: m_data(INITIAL_CAPACITY)
			, m_freeBuckets(0)
		{
			assert_valid_index_type();
			initialize_new_references();
		}

		CLASS_CTOR									Repository(				Repository&&		other) noexcept
			: m_data(std::move(other.m_data))
			, m_references(std::move(other.m_references))
			, m_freeBuckets(other.m_freeBuckets)
		{
			other.m_freeBuckets = 0;
		}

		CLASS_CTOR									Repository(				const Repository&	OTHER)
			: Repository(0)
		{
			copy_from(OTHER);
		}

		CLASS_DTOR									~Repository()
		{
			clear();
		}

		Repository&									operator=(				const Repository&	OTHER)
		{
			copy_from(OTHER);
			return *this;
		}

		Repository&									operator=(				Repository&&		other) noexcept
		{		
			m_data				= std::move(other.m_data);
			m_references		= std::move(other.m_mask);
			m_freeBuckets		= other.m_freeBuckets;
			other.m_freeBuckets = 0;
			return *this;
		}

	public: // operators
		inline T&									operator[](				const index_t		INDEX)
		{
			return m_data[INDEX];
		}

		inline const T&								operator[](				const index_t		INDEX) const
		{
			return m_data[INDEX];
		}

	public: // functions
		inline index_t								capacity() const
		{
			return m_data.capacity();
		}

		inline index_t								size() const
		{
			return m_data.capacity() - m_freeBuckets;
		}

		inline index_t								available_space() const
		{
			return m_freeBuckets > 0;
		}

		inline bool									valid_at(				const index_t		INDEX) const
		{
			return (INDEX < capacity()) ? m_references[INDEX].bValid : false;
		}

		inline index_t								get_entry_index(		const T*			ENTRY_ADDRESS) const
		{
			if(ENTRY_ADDRESS < m_data.data()) return INVALID_INDEX;
			const index_t INDEX = (index_t)(ENTRY_ADDRESS - m_data.data());
			return (INDEX < capacity()) ? INDEX : INVALID_INDEX;
		}

		inline index_t								get_next_valid_index(	index_t				index) const
		{
			while(++index < m_data.capacity()) if(valid_at(index)) return index;
			return INVALID_INDEX;
		}

		inline index_t								get_first_valid_index() const
		{
			return get_next_valid_index(INVALID_INDEX); //<-- Note: Index will winded back to 0.
		}

		inline T&									get(					const index_t		INDEX)
		{
			validate_at(INDEX);
			return m_data[INDEX];
		}

		inline const T&								get(					const index_t		INDEX) const
		{
			validate_at(INDEX);
			return m_data[INDEX];
		}

		inline T*									find(					const index_t		INDEX)
		{
			return valid_at(INDEX) ? m_data.data() + INDEX : nullptr;
		}

		inline const T*								find(					const index_t		INDEX) const
		{
			return valid_at(INDEX) ? m_data.data() + INDEX : nullptr;
		}

		/*
			Returns index of the bucket to be used next.
		*/
		inline index_t								peek_next_bucket() const
		{
			if(!available_space()) return size();
			return m_references[m_freeBuckets-1].index;
		}

		template<typename... CTOR>
		inline T&									emplace(				CTOR&&...			args)
		{
			const index_t INDEX = reserve_bucket();
			m_data.construct_at(INDEX, std::forward<CTOR>(args)...);
			return m_data[INDEX];
		}

		inline void									erase_at(				const index_t		INDEX)
		{
			release_bucket(INDEX);
			m_data.destroy_at(INDEX);
		}

		inline void									erase(					const T&			ENTRY)
		{
			erase_at(get_entry_index(&ENTRY));
		}

		inline void									clear()
		{
			index_t index = 0;
			while(index < capacity())
			{
				if(valid_at(index)) m_data.destroy_at(index);
				release_bucket(capacity() - (++index));
			}
		}

		void										reserve(				const index_t		capacity)
		{
			if(capacity > m_data.capacity())
			{
				Buffer<T> newData(capacity);
				for(index_t index = 0; index < m_data.capacity(); ++index)
				{
					if(valid_at(index))
					{
						newData.construct_at(index, std::move(m_data[index]));
						m_data.destroy_at(index);
					}
				}

				m_data = std::move(newData);
				initialize_new_references();
			}
		}

		/*
			Reduces capacity to number of valid entries in the repository.
			Note: Invalidates iterators/indices.
		*/
		void										compact()
		{
			if(available_space() > 0)
			{
				Buffer<T>	newData(size());
				uint32_t	newIndex = 0;

				for(index_t oldIndex = 0; oldIndex < capacity(); ++oldIndex)
				{
					if(valid_at(oldIndex))
					{
						newData.construct_at(newIndex++, std::move(m_data[oldIndex]));
						m_data.destroy_at(oldIndex);
					}
				}

				m_data			= std::move(newData);
				m_freeBuckets	= 0;
				m_references.swap(References(size(), Reference(EntryState::VALID)));
			}
		}

		/*
			Reduces capacity to the last valid entry.
			Note: Does not invalidate iterators/indices.
		*/
		void										shrink()
		{
			const index_t NEW_CAPACITY = find_last_valid_entry() + 1;
			if(NEW_CAPACITY < capacity())
			{
				Buffer<T> newData(NEW_CAPACITY);

				for(index_t index = 0; index < NEW_CAPACITY; ++index)
				{
					if(valid_at(index))
					{
						newData.construct_at(index, std::move(m_data[index]));
						m_data.destroy_at(index);
					}
				}

				m_data	= std::move(newData);
				m_references.resize(NEW_CAPACITY);
				m_references.shrink_to_fit();
			}
		}

		inline void									enlarge(				const index_t		AMOUNT)
		{
			reserve(size() + AMOUNT);
		}

		bool										copy_from(				const Repository&	OTHER)
		{
			if(this == &OTHER) return false;

			clear();

			m_data			= std::move(Buffer<T>(OTHER.capacity()));
			m_references	= OTHER.m_references;
			m_freeBuckets	= OTHER.m_freeBuckets;

			for(index_t index = 0; index < OTHER.capacity(); ++index)
				if(OTHER.valid_at(index))
					m_data.construct_at(index, OTHER.m_data[index]);

			return true;
		}

		void										swap(					Repository&			other)
		{
			if(this == &other) return;
			m_data.swap(other.m_data);
			m_references.swap(other.m_references);
			std::swap(m_freeBuckets, other.m_freeBuckets);
		}

	public: // iterators
		inline Iterator								begin()
		{
			return Iterator(this, get_first_valid_index());
		}

		inline ConstIterator						begin() const
		{
			return ConstIterator(this, get_first_valid_index());
		}

		inline Iterator								end()
		{
			return Iterator(this, INVALID_INDEX);
		}

		inline ConstIterator						end() const
		{
			return ConstIterator(this, INVALID_INDEX);
		}

	public: // import/export
		template<typename ImportF = DefaultImport<T, index_t>>
		void										import_from(			std::istream&		binary,
																			ImportF				on_import = DefaultImport<T, index_t>())
		{
			clear();
			size_t newCapacity = 0;
			binary.read(reinterpret_cast<char*>(&newCapacity), sizeof(size_t));
			m_references.resize(newCapacity);
			m_references.shrink_to_fit();
			binary.read(reinterpret_cast<char*>(m_references.data()), sizeof(Reference) * newCapacity);
			binary.read(reinterpret_cast<char*>(&m_freeBuckets), sizeof(m_freeBuckets));
			m_data	= std::move(Buffer<T>((index_t)m_references.size()));
			for(index_t index = 0; index < capacity(); ++index)
			{
				if(valid_at(index))
				{
					m_data.construct_at(index);
					on_import(m_data[index], binary);
				}
			}
		}

		template<typename ExportF = DefaultExport<T, index_t>>
		void										export_to(				std::ostream&		binary,
																			ExportF				on_export = DefaultExport<T, index_t>()) const
		{
			const size_t SIZE = m_references.size();
			binary.write(reinterpret_cast<const char*>(&SIZE), sizeof(size_t));
			binary.write(reinterpret_cast<const char*>(m_references.data()), sizeof(Reference) * m_references.size());
			binary.write(reinterpret_cast<const char*>(&m_freeBuckets), sizeof(m_freeBuckets));
			for(index_t index = 0; index < capacity(); ++index)
			{
				on_export(m_data[index], binary);
			}
		}

	private: // functions
		inline index_t								find_last_valid_entry() const
		{
			index_t lastValid = capacity();
			for(index_t index = 0; index < capacity(); ++index)
			{
				if(valid_at(index))
				{
					lastValid = index;
				}
			}
			return lastValid;
		}

		inline void									assert_valid_index_type() const
		{
			static_assert(std::is_unsigned<index_t>::value, "index_t must be unsigned");
			static_assert(std::is_integral<index_t>::value, "index_t must be integral");
		}

		inline void									validate_at(			const index_t		INDEX) const
		{
#ifdef _DEBUG
			if(!valid_at(INDEX))
				throw std::out_of_range(typeid(this).name() + std::string(": Invalid index"));
#endif
		}

		void										initialize_new_references()
		{
			const index_t	NUM_NEW		= capacity() - (index_t)m_references.size();
			index_t			reverse_id	= m_freeBuckets + NUM_NEW;

			m_references.resize(capacity());
			for(index_t index = 0; index < NUM_NEW; ++index)
			{
				m_references[m_freeBuckets++].index = --reverse_id;
			}
		}

		inline index_t								reserve_bucket()
		{
			if(!available_space()) enlarge(size()); //<-- Double in size

			Reference&	reference = m_references[--m_freeBuckets];
						reference.bValid	= EntryState::VALID;

			const index_t INDEX = reference.index;
			reference.index = INVALID_INDEX;
			return INDEX;
		}

		inline void									release_bucket(			const index_t		INDEX)
		{
#ifdef _DEBUG
			if(!valid_at(INDEX))
				throw GeneralException(this, __LINE__, "Indexof of range.");
#endif // _DEBUG

			Reference&	reference = m_references[m_freeBuckets++];
						reference.bValid	= EntryState::INVALID;
						reference.index		= INDEX;
		}
	};
}