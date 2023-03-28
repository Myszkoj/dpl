#pragma once


#include "dpl_GeneralException.h"


namespace dpl
{
#pragma pack(push, 4)
	template<typename T, uint32_t HEADER_BYTES = 0>
	class DynamicBuffer
	{
	public: // subtypes

	private: // data
		uint8_t*	m_bytes;
		uint32_t	m_capacity;

	public: // lifecycle
		CLASS_CTOR					DynamicBuffer(			const uint32_t			CAPACITY = 0)
			: m_bytes(nullptr)
			, m_capacity(CAPACITY)
		{
			allocate();
		}

		CLASS_CTOR					DynamicBuffer(			const DynamicBuffer&	OTHER) = delete;

		CLASS_CTOR					DynamicBuffer(			DynamicBuffer&&			other) noexcept
			: m_bytes(other.m_bytes)
			, m_capacity(other.m_capacity)
		{
			other.invalidate();
		}

		CLASS_DTOR					~DynamicBuffer()
		{
			release();
		}

		DynamicBuffer&				operator=(				const DynamicBuffer&	OTHER) = delete;

		DynamicBuffer&				operator=(				DynamicBuffer&&			other) noexcept
		{
			other.swap(*this);
			other.release();
			return *this;
		}

	public: // functions
		inline T&					operator[](				const uint32_t			index)
		{
			return data()[index];
		}

		inline const T&				operator[](				const uint32_t			index) const
		{
			return data()[index];
		}

		inline uint32_t				capacity() const
		{
			return m_capacity;
		}

		template<typename HeaderT>
		inline HeaderT&				header()
		{
			test_header_type<HeaderT>();
			return *reinterpret_cast<HeaderT*>(m_bytes);
		}

		template<typename HeaderT>
		inline const HeaderT&		header() const
		{
			test_header_type<HeaderT>();
			return *reinterpret_cast<const HeaderT*>(m_bytes);
		}

		inline T*					data()
		{
			return reinterpret_cast<T*>(m_bytes + HEADER_BYTES);
		}

		inline const T*				data() const
		{
			return reinterpret_cast<const T*>(m_bytes + HEADER_BYTES);
		}

		inline void					swap(					DynamicBuffer&			other)
		{
			std::swap(m_bytes,		other.m_bytes);
			std::swap(m_capacity,	other.m_capacity);
		}

		template<typename... Args>
		inline T&					construct_at(			const uint32_t			INDEX, 
															Args&&...				args)
		{
			auto& obj = data()[INDEX];
			new(&obj)T(std::forward<Args>(args)...);
			return obj;
		}

		inline void					destroy_at(				const uint32_t			INDEX)
		{
			data()[INDEX].~T();
		}

	private: // functions
		inline void					allocate()
		{
			if(const size_t NUM_BYTES = HEADER_BYTES + capacity() * sizeof(T))
			{
				m_bytes = static_cast<uint8_t*>(malloc(NUM_BYTES));
				if (!m_bytes) throw GeneralException(this, __LINE__, std::string("Fail to allocate ") + std::to_string(NUM_BYTES) + " bytes.");
			}
		}

		inline void					release()
		{
			if(m_bytes != nullptr)
			{
				free(m_bytes);
				invalidate();
			}
		}

		inline void					invalidate()
		{
			m_bytes		= nullptr;
			m_capacity	= 0;
		}

		template<typename HeaderT>
		inline void					test_header_type() const
		{
			static_assert(sizeof(HeaderT) == HEADER_BYTES, "Invalid header type.");
		}
	};
#pragma pack(pop)
}