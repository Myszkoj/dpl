#pragma once


#include <stdint.h>
#include <stdexcept>
#include <functional>
#include <memory>
#include "dpl_ReadOnly.h"
#include "dpl_GeneralException.h"

#pragma pack(push, 4)

namespace dpl
{
	/*
		Value at Nth position represent new position. 
	*/
	class	DeltaArray
	{
	public: // subtypes
		using OldPosition_t	= uint32_t;
		using NewPosition_t	= uint32_t;

		using Callback		= std::function<void(const NewPosition_t, OldPosition_t&)>;
		using ConstCallback = std::function<void(const NewPosition_t, const OldPosition_t)>;

	public: // data
		ReadOnly<uint32_t, DeltaArray>		size;

	private: // data
		std::unique_ptr<NewPosition_t[]>	indices;

	public: // lifecycle
		CLASS_CTOR				DeltaArray(	const uint32_t			NUM_INDICES)
			: size(NUM_INDICES)
			, indices(std::make_unique<OldPosition_t[]>(NUM_INDICES))
		{

		}

	public: // functions
		void					for_each(	const Callback&			CALL_AT)
		{
			for(NewPosition_t oldPosition = 0; oldPosition < size(); ++oldPosition)
			{
				CALL_AT(indices[oldPosition], oldPosition);
			}
		}

		void					for_each(	const ConstCallback&	CALL_AT) const
		{
			for(NewPosition_t oldPosition = 0; oldPosition < size(); ++oldPosition)
			{
				CALL_AT(indices[oldPosition], oldPosition);
			}
		}

	public: // operators
		NewPosition_t&			operator[](	const OldPosition_t		INDEX)
		{
			return indices[INDEX];
		}

		const NewPosition_t&	operator[](	const OldPosition_t		INDEX) const
		{
			return indices[INDEX];
		}
	};


	template<typename T>
	class	Buffer
	{
	private: // subtypes
		using	MyBase = Buffer<T>;

	public: // subtypes
		using	NewBuffer	= Buffer<T>;
		using	OnRelocate	= std::function<void(NewBuffer&)>;

	public: // constants
		static const uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

	private: // data
		T*			m_data;
		uint32_t	m_capacity;

	public: // lifecycle
		CLASS_CTOR			Buffer(							const uint32_t		CAPACITY = 0)
			: m_data(nullptr)
			, m_capacity(CAPACITY)
		{
			allocate();
		}

		CLASS_CTOR			Buffer(							const Buffer&		OTHER) = delete;

		CLASS_CTOR			Buffer(							Buffer&&			other) noexcept
			: m_data(other.m_data)
			, m_capacity(other.m_capacity)
		{
			other.invalidate();
		}

		CLASS_DTOR			~Buffer()
		{
			release_data();
		}

	public: // operators
		Buffer&				operator=(						const Buffer&		OTHER) = delete;

		Buffer&				operator=(						Buffer&&			other) noexcept
		{
			if (this != &other)
			{
				release_data();
				m_data		= other.m_data;
				m_capacity	= other.m_capacity;
				other.invalidate();
			}

			return *this;
		}

		T&					operator[](			const uint32_t		INDEX)
		{
			return m_data[INDEX];
		}

		const T&			operator[](			const uint32_t		INDEX) const
		{
			return m_data[INDEX];
		}

	public: // functions
		/*
			Returns capacity of the buffer in bytes.
		*/
		uint32_t			bytes() const
		{
			return static_cast<uint32_t>(sizeof(T) * capacity());
		}

		uint32_t			capacity() const
		{
			return m_capacity;
		}

		T*					data()
		{
			return m_data;
		}

		const T*			data() const
		{
			return m_data;
		}

		T&					at(								const uint32_t		INDEX)
		{
			throw_if_invalid_index(INDEX);
			return data()[INDEX];
		}

		const T&			at(								const uint32_t		INDEX) const
		{
			throw_if_invalid_index(INDEX);
			return data()[INDEX];
		}

		bool				index_in_range(					const uint32_t		INDEX) const
		{
			return INDEX < m_capacity;
		}

		bool				index_in_range(					const uint64_t		INDEX) const
		{
			return INDEX < m_capacity;
		}

		/*
			Returns INVALID_INDEX if out of range.
		*/
		uint32_t			index_of(						const T*			ADDRESS) const
		{
			if (ADDRESS < data()) return INVALID_INDEX;
			const uint64_t INDEX = ADDRESS - data();
			return index_in_range(INDEX)? (uint32_t)INDEX : INVALID_INDEX;
		}

		bool				contains_address(				const T*			ADDRESS) const
		{
			if (ADDRESS < data()) return false;
			return index_in_range(ADDRESS - data());
		}

		bool				can_fit(						const uint32_t		NUM_ELEMENTS) const
		{
			return NUM_ELEMENTS <= m_capacity;
		}

		template<typename... Args>
		T&					construct_at(					const uint32_t		INDEX, 
															Args&&...			args)
		{
			auto& obj = at(INDEX);
			new(&obj)T(std::forward<Args>(args)...);
			return obj;
		}

		void				fill()
		{
			for(uint32_t index = 0; index < capacity(); ++index)
			{
				Buffer::construct_at(index);
			}
		}

		template<typename U = T>
		void				fill(							const U&			DEFAULT)
		{
			for(uint32_t index = 0; index < capacity(); ++index)
			{
				Buffer::construct_at(index, DEFAULT);
			}
		}

		void				destroy_at(						const uint32_t		INDEX)
		{
			if constexpr (!std::is_trivially_destructible_v<T>)
			{
				at(INDEX).~T();
			}
		}

		void				destroy_range(					const uint32_t		OFFSET,
															const uint32_t		COUNT)
		{
			if constexpr (!std::is_trivially_destructible_v<T>)
			{
				throw_if_invalid_range(OFFSET, COUNT);
				const uint32_t END = OFFSET + COUNT;
				for(uint32_t index = OFFSET; index != END; ++index)
				{
					destroy_at(index);
				}
			}
		}

		void				swap(							Buffer&				other)
		{
			std::swap(m_data,		other.m_data);
			std::swap(m_capacity,	other.m_capacity);
		}

		void				relocate(						const uint32_t		NEW_CAPACITY,
															const OnRelocate&	ON_RELOCATE)
		{
			Buffer<T> newBuffer(NEW_CAPACITY);
			if(ON_RELOCATE) ON_RELOCATE(newBuffer);
			newBuffer.swap(*this);
		}

	public: // contiguous data functions (WARNING! None of this functions call destructors of the elements of this buffer!)
		void				mimic(							const Buffer&		SOURCE,
															uint32_t			numElements = -1)
		{
			if(numElements > SOURCE.m_capacity) numElements = SOURCE.m_capacity;
			if(m_capacity != SOURCE.m_capacity) relocate(SOURCE.m_capacity, nullptr);
			for(uint32_t index = 0; index < numElements; ++index)
			{
				Buffer::construct_at(index, SOURCE[index]);
			}
		}

		void				copy_from(						const Buffer&		SOURCE,
															const uint32_t		NUM_ELEMENTS,
															const uint32_t		SRC_OFFSET = 0,
															const uint32_t		DST_OFFSET = 0)
		{
			SOURCE.throw_if_invalid_range(SRC_OFFSET, NUM_ELEMENTS);
			throw_if_invalid_range(DST_OFFSET, NUM_ELEMENTS);
			for(uint32_t index = 0; index < NUM_ELEMENTS; ++index)
			{
				Buffer::construct_at(index + DST_OFFSET, SOURCE[index + SRC_OFFSET]);
			}
		}

		void				move_from(						Buffer&				source,
															const uint32_t		NUM_ELEMENTS,
															const uint32_t		SRC_OFFSET = 0,
															const uint32_t		DST_OFFSET = 0)
		{
			source.throw_if_invalid_range(SRC_OFFSET, NUM_ELEMENTS);
			throw_if_invalid_range(DST_OFFSET, NUM_ELEMENTS);
			for(uint32_t index = 0; index < NUM_ELEMENTS; ++index)
			{
				const uint32_t DST_INDEX = index + DST_OFFSET;
				const uint32_t SRC_INDEX = index + SRC_OFFSET;
				Buffer::construct_at(DST_INDEX, std::move(source[SRC_INDEX]));
				if constexpr (!std::is_trivially_destructible_v<T>)
				{
					source.destroy_at(SRC_INDEX);
				}
			}
		}

		void				move_from(						Buffer&				source,
															const DeltaArray&	DELTA)
		{
			throw_if_invalid_source_delta(source, DELTA);
			for(uint32_t oldIndex = 0; oldIndex < DELTA.size(); ++oldIndex)
			{
				const uint32_t NEW_INDEX = DELTA[oldIndex];
				source.throw_if_invalid_index(NEW_INDEX);
				Buffer::construct_at(NEW_INDEX, std::move(source.at(oldIndex)));
			}
		}

	private: // functions
		void				allocate()
		{
			if(capacity() > 0)
			{
				const size_t NUM_BYTES = this->bytes();
				m_data = static_cast<T*>(malloc(NUM_BYTES));

				if (!m_data)
					throw GeneralException(this, __LINE__, std::string("Fail to allocate ") + std::to_string(NUM_BYTES) + " bytes.");
			}
		}

		void				release_data()
		{
			if(m_data != nullptr)
			{
				free(static_cast<void*>(m_data));
				m_data = nullptr;
			}
		}

		void				invalidate()
		{
			m_data		= nullptr;
			m_capacity	= 0;
		}

	private: // debug exceptions
		void				throw_if_invalid_index(			const uint32_t		INDEX) const
		{
#ifdef _DEBUG
			if(index_in_range(INDEX)) return;
			throw std::out_of_range("Buffer: Invalid index");
#endif
		}

		void				throw_if_invalid_range(			const uint32_t		OFFSET,
															const uint32_t		COUNT) const
		{
#ifdef _DEBUG
			if(can_fit(OFFSET + COUNT)) return;
			throw std::out_of_range("Buffer: Invalid range");
#endif
		}

		void				throw_if_invalid_source_delta(	const Buffer&		SOURCE,
															const DeltaArray&	DELTA) const
		{
#ifdef _DEBUG
			if(SOURCE.capacity() < DELTA.size())
				throw GeneralException(this, __LINE__, "Invalid source delta.");
#endif // _DEBUG
		}
	};
}

#pragma pack(pop)