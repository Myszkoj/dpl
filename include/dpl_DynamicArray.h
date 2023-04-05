#pragma once


#include <functional>
#include "dpl_Buffer.h"
#include "dpl_Binary.h"


#pragma pack(push, 4)

namespace dpl
{
	template<typename T, uint32_t INITIAL_EXPONENT = 4> requires (INITIAL_EXPONENT < 16)
	class	DynamicArray
	{
	public: // subtypes
		using	OnModify		= std::function<void(dpl::Buffer<T>&)>;
		using	Invocation		= std::function<void(T&)>;
		using	ConstInvocation	= std::function<void(const T&)>;
		using	value_type		= T;
		using	size_type		= uint32_t;

	public: // constants
		static const size_type INITIAL_CAPACITY = (1<<INITIAL_EXPONENT);

	private: // data
		dpl::Buffer<T>							m_buffer;
		size_type								m_size;

	public: // lifecycle
		CLASS_CTOR					DynamicArray()
			: m_buffer(INITIAL_CAPACITY)
			, m_size(0)
		{
			
		}

		CLASS_CTOR					DynamicArray(					const uint32_t				INITIAL_SIZE)
			: DynamicArray()
		{
			resize(INITIAL_SIZE);
		}

		CLASS_CTOR					DynamicArray(					DynamicArray&&				other) noexcept
			: m_buffer(std::move(other.m_buffer))
			, m_size(other.m_size)
		{
			other.m_size = 0;
		}

		CLASS_DTOR					~DynamicArray()
		{
			dpl::no_except([&]()
			{
				clear();
			});
		}

		DynamicArray&				operator=(						DynamicArray&&				other) noexcept
		{
			clear();
			m_buffer.swap(other.m_buffer);
			std::swap(m_size, other.m_size);
			return *this;
		}

	public: // functions
		inline void					swap(							DynamicArray&				other)
		{
			m_buffer.swap(other.m_buffer);
			std::swap(m_size, other.m_size);
		}

		inline size_type			size() const
		{
			return m_size;
		}

		inline bool					empty() const
		{
			return size() == 0;
		}

		inline bool					full() const
		{
			return size() == capacity();
		}

		inline size_type			capacity() const
		{
			return m_buffer.capacity();
		}

		inline T*					data()
		{
			return m_buffer.data();
		}

		inline const T*				data() const
		{
			return m_buffer.data();
		}

		/*
			Returns INVALID_INDEX if out of range.
		*/
		inline uint32_t				index_of(						const T*					ADDRESS) const
		{
			return m_buffer.index_of(ADDRESS);
		}

		/*
			Returns INVALID_INDEX if out of range.
		*/
		inline uint32_t				index_of(						const T&					ELEMENT) const
		{
			return m_buffer.index_of(&ELEMENT);
		}

		inline T&					at(								const uint32_t				INDEX)
		{
			throw_if_invalid_index(INDEX);
			return data()[INDEX];
		}

		inline const T&				at(								const uint32_t				INDEX) const
		{
			throw_if_invalid_index(INDEX);
			return data()[INDEX];
		}

		inline T&					operator[](						const uint32_t			INDEX)
		{
			throw_if_invalid_index(INDEX);
			return at(INDEX);
		}

		inline const T&				operator[](						const uint32_t			INDEX) const
		{
			throw_if_invalid_index(INDEX);
			return at(INDEX);
		}

		inline T&					front()
		{
			return DynamicArray::at(0);
		}

		inline const T&				front() const
		{
			return DynamicArray::at(0);
		}

		inline T&					back()
		{
			return DynamicArray::at(size()-1);
		}

		inline const T&				back() const
		{
			return DynamicArray::at(size()-1);
		}

		inline void					for_each(						const Invocation&			INVOKE)
		{
			for(uint32_t index = 0; index < size(); ++index)
			{
				INVOKE(data()[index]);
			}
		}

		inline void					for_each(						const ConstInvocation&		INVOKE) const
		{
			for(uint32_t index = 0; index < size(); ++index)
			{
				INVOKE(data()[index]);
			}
		}

		inline void					reserve(						const uint32_t				NEW_SIZE)
		{
			m_buffer.relocate(calculate_exponential_capacity(NEW_SIZE), [&](dpl::Buffer<T>& newBuffer)
			{
				newBuffer.move_from(m_buffer, size());
			});
		}

		template<typename... CTOR>
		inline T&					emplace_back(					CTOR&&...					args)
		{
			if(full()) upsize();
			return m_buffer.construct_at(m_size++, std::forward<CTOR>(args)...);
		}

		void						pop_back()
		{
			throw_if_empty();
			const uint32_t LAST_ELEMENT_INDEX = size()-1;
			m_buffer.destroy_at(LAST_ELEMENT_INDEX);
			m_size = LAST_ELEMENT_INDEX;
			if(too_much_capacity()) downsize();
		}

		/*
			Adds given AMOUNT of elements and returns pointer to the first of them.
		*/
		T*							enlarge(						const uint32_t				AMOUNT)
		{
			if(AMOUNT == 0) return nullptr;
			const uint32_t OLD_SIZE		= size();
			const uint32_t NEW_SIZE		= OLD_SIZE + AMOUNT;
			const uint32_t NEW_CAPACITY = calculate_exponential_capacity(NEW_SIZE);
			if(capacity() != NEW_CAPACITY) relocate(NEW_CAPACITY);
			for(uint32_t index = OLD_SIZE; index < NEW_SIZE; ++index)
			{
				m_buffer.construct_at(index);
			} 
			m_size = NEW_SIZE;
			return m_buffer.data() + OLD_SIZE;
		}

		T*							enlarge(						const uint32_t				AMOUNT,
																	const T&					DEFAULT)
		{
			if(AMOUNT == 0) return nullptr;
			const uint32_t OLD_SIZE		= size();
			const uint32_t NEW_SIZE		= OLD_SIZE + AMOUNT;
			const uint32_t NEW_CAPACITY = calculate_exponential_capacity(NEW_SIZE);
			if(capacity() != NEW_CAPACITY) relocate(NEW_CAPACITY);
			for(uint32_t index = OLD_SIZE; index < NEW_SIZE; ++index)
			{
				m_buffer.construct_at(index, DEFAULT);
			} 
			m_size = NEW_SIZE;
			return m_buffer.data() + OLD_SIZE;
		}

		void						reduce_if_possible(				const uint32_t				AMOUNT)
		{
			const uint32_t NEW_SIZE = (AMOUNT < size())? size() - AMOUNT : 0;
			for(uint32_t index = NEW_SIZE; index < size(); ++index)
			{
				m_buffer.destroy_at(index);
			}
			m_size = NEW_SIZE;
			if(too_much_capacity()) downsize();
		}

		inline void					reduce(							const uint32_t				AMOUNT)
		{
			throw_if_invalid_reduce(AMOUNT);
			reduce_if_possible(AMOUNT);
		}

		inline void					resize(							const uint32_t				NEW_SIZE)
		{
			if(size() < NEW_SIZE) DynamicArray::enlarge(NEW_SIZE - size());
			else if(size() > NEW_SIZE) DynamicArray::reduce(size() - NEW_SIZE);
		}

		inline void					swap_elements(					const uint32_t				FIRST_INDEX,
																	const uint32_t				SECOND_INDEX)
		{
			if (FIRST_INDEX == SECOND_INDEX) return;
			std::swap(at(FIRST_INDEX), at(SECOND_INDEX));
		}

		inline void					make_last(						const uint32_t				INDEX)
		{
			throw_if_invalid_index(INDEX);
			swap_elements(INDEX, m_size - 1);
		}

		inline void					fast_erase(						const uint32_t				INDEX)
		{
			swap_elements(INDEX, size()-1);
			pop_back();
		}
		
		inline void					fast_erase(						const T*					ELEMENT)
		{
			DynamicArray::fast_erase(index_of(ELEMENT));
		}

		inline void					fast_erase(						const T&					ELEMENT)
		{
			DynamicArray::fast_erase(&ELEMENT);
		}

		inline void					rearrange(						const dpl::DeltaArray&		DELTA)
		{
			throw_if_invalid_delta();
			m_buffer.relocate(capacity(), [&](dpl::Buffer<T>& newBuffer)
			{
				newBuffer.move_from(m_buffer, DELTA);
			});
		}

		inline void					clear()
		{
			clear_internal();
			relocate(INITIAL_CAPACITY);
		}

		void						import_from(					std::istream&				binary)
		{
			clear_internal();
			const uint32_t NEW_SIZE = dpl::import_t<uint32_t>(binary);
			m_buffer.relocate(calculate_exponential_capacity(NEW_SIZE), [&](auto&){}); //<-- Relocate to an empty buffer.
			m_size = NEW_SIZE;
			dpl::import_t(binary, size(), data());
		}

		void						import_tail_from(				std::istream&				binary)
		{
			const uint32_t TAIL_SIZE = dpl::import_t<uint32_t>(binary);
			dpl::import_t(binary, TAIL_SIZE, DynamicArray::enlarge(TAIL_SIZE));
		}

		void						export_to(						std::ostream&				binary) const
		{
			dpl::export_t(binary, size());
			dpl::export_t(binary, size(), data());
		}

		void						export_tail_to(					std::ostream&				binary,
																	const uint32_t				TAIL_SIZE) const
		{
			if(TAIL_SIZE > size()) throw dpl::GeneralException(this, __LINE__, "Fail to export given tail: %d", TAIL_SIZE);
			const uint32_t OFFSET = size() - TAIL_SIZE;
			dpl::export_t(binary, TAIL_SIZE);
			dpl::export_t(binary, TAIL_SIZE, data()+OFFSET);
		}

	private: // functions
		inline bool					too_much_capacity() const
		{
			return size() < capacity()/4;
		}

		inline void					clear_internal()
		{
			m_buffer.destroy_range(0, size());
			m_size = 0;
		}

		inline void					relocate(						const uint32_t				NEW_CAPACITY)
		{
			m_buffer.relocate(NEW_CAPACITY, [&](dpl::Buffer<T>& newBuffer)
			{
				newBuffer.move_from(m_buffer, size());
			});
		}

		inline void					upsize()
		{
			relocate(capacity() * 2);
		}

		inline void					downsize()
		{
			relocate(capacity() / 2);
		}

		inline uint32_t				calculate_exponential_capacity(	const uint32_t				NEW_SIZE) const
		{
			uint32_t newCapacity = (capacity() > 0)? capacity() : INITIAL_CAPACITY;
			while(NEW_SIZE > newCapacity) newCapacity *= 2;
			return newCapacity;
		}

	private: // debug exceptions
		inline void					throw_if_empty() const
		{
#ifdef _DEBUG
			if(empty())
				throw dpl::GeneralException(this, __LINE__, "Array is empty.");
#endif // _DEBUG
		}

		inline void					throw_if_invalid_reduce(		const uint32_t				AMOUNT) const
		{
#ifdef _DEBUG
			if(size() < AMOUNT)
				throw dpl::GeneralException(this, __LINE__, "Not enough elements to reduce.");
#endif // _DEBUG
		}

		inline void					throw_if_invalid_index(			const uint32_t				INDEX) const
		{
#ifdef _DEBUG
			if(INDEX < size()) return;
			throw dpl::GeneralException(this, __LINE__, "Index out of range.");
#endif // _DEBUG
		}

		inline void					throw_if_invalid_delta(			const dpl::DeltaArray&		DELTA) const
		{
#ifdef _DEBUG
			if(DELTA.size() != size())
				throw dpl::GeneralException(this, __LINE__, "Invalid delta.");
#endif // _DEBUG
		}
	};
}

#pragma pack(pop)