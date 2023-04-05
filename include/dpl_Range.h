#pragma once


#include <functional>
#include <ratio>
#include <iostream>
#include "dpl_ReadOnly.h"
#include "dpl_GeneralException.h"


namespace dpl
{
	template<typename T>
	class Range
	{
	public: // data
		ReadOnly<T, Range> min;
		ReadOnly<T, Range> max;

	public: // lifecycle
		CLASS_CTOR				Range()
			: min((T)0)
			, max((T)0)
		{

		}

		CLASS_CTOR				Range(		const T			MIN,
											const T			MAX)
			: Range()
		{
			reset(MIN, MAX);
		}

	public: // operators
		friend std::istream&	operator>>(	std::istream&	stream, 
											Range&			range) 
		{
			thread_local T min = T();
			thread_local T max = T();
			stream >> min >> max;
			range.reset(min, max);
			return stream;
		}

		friend std::ostream&	operator<<(	std::ostream&	stream, 
											const Range&	RANGE)
		{
			stream << RANGE.min() << " " << RANGE.max();
			return stream;
		}

	public: // functions
		inline void				set_min(	const T			NEW_MIN)
		{
			min = NEW_MIN;
			set_max(max());
		}

		inline void				set_max(	const T			NEW_MAX)
		{
			max = (min() > NEW_MAX) ? min() : NEW_MAX;
		}

		inline void				reset(		const T			NEW_MIN,
											const T			NEW_MAX)
		{
			min = NEW_MIN;
			set_max(NEW_MAX);
		}

		inline T				size() const
		{
			return max() - min();
		}

		inline T				center() const
		{
			return (min() + max()) / (T)2;
		}

		inline T				clamp(		const T			VALUE) const
		{
			if(VALUE < min()) return min();
			if(VALUE > max()) return max();
			return VALUE;
		}

		inline bool				contains(	const T			VALUE) const
		{
			return (min() <= VALUE) && (VALUE >= max()); 
		}

		inline float			to_factor(	T				value) const
		{
			value = clamp(value);
			return (float)(value-min())/(size());
		}

		inline T				from_factor(float			factor) const
		{
			if(factor < 0.f) factor = 0.f;
			if(factor > 1.f) factor = 1.f;
			return min() + (T)(factor * size());
		}
	};


	template<typename IndexT = uint32_t> requires std::is_integral_v<IndexT> && std::is_unsigned_v<IndexT>
	class IndexRange
	{
	public: // data
		ReadOnly<IndexT, IndexRange> begin;
		ReadOnly<IndexT, IndexRange> end;

	public: // lifecycle
		CLASS_CTOR					IndexRange()
			: begin(0)
			, end(0)
		{

		}

		CLASS_CTOR					IndexRange(		const IndexT									BEGIN)
			: begin(BEGIN)
			, end(BEGIN)
		{

		}

		CLASS_CTOR					IndexRange(		const IndexT									BEGIN,
													const IndexT									END)
			: begin(BEGIN)
			, end(END)
		{
			check_swapped();
		}

	public: // functions
		inline IndexT				size() const
		{
			return end() - begin();
		}

		inline bool					empty() const
		{
			return size() == 0;
		}

		inline IndexT				front() const
		{
			check_size();
			return begin();
		}

		inline IndexT				back() const
		{
			check_size();
			return end() - 1;
		}

		inline void					set_begin(		const IndexT									NEW_BEGIN)
		{
			begin	= NEW_BEGIN;
			end		= (NEW_BEGIN > end()) ? NEW_BEGIN : end();
		}

		inline void					set_end(		const IndexT									NEW_END)
		{
			begin	= (begin() < NEW_END) ? begin() : NEW_END;
			end		= NEW_END;
		}

		inline void					reset()
		{
			begin	= 0;
			end		= 0;
			check_swapped();
		}

		inline void					reset(			const IndexT									NEW_BEGIN,
													const IndexT									NEW_END)
		{
			begin	= NEW_BEGIN;
			end		= NEW_END;
			check_swapped();
		}

		inline void					increase_front(	const IndexT									AMOUNT = 1)
		{
			check_begin(AMOUNT);
			*begin -= AMOUNT;
		}

		inline void					increase_back(	const IndexT									AMOUNT = 1)
		{
			*end += AMOUNT;
		}

		inline void					decrease_front(	const IndexT									AMOUNT = 1)
		{
			*begin += AMOUNT;
			check_swapped();
		}

		inline void					decrease_back(	const IndexT									AMOUNT = 1)
		{
			check_end(AMOUNT);
			*end -= AMOUNT;
			check_swapped();
		}

		inline void					shift_front(	const IndexT									AMOUNT = 1)
		{
			check_begin(AMOUNT);
			*begin	-= AMOUNT;
			*end	-= AMOUNT;
		}

		inline void					shift_back(		const IndexT									AMOUNT = 1)
		{
			*begin	+= AMOUNT;
			*end	+= AMOUNT;
		}

		inline IndexRange			intersection(	const IndexRange&								OTHER) const
		{
			IndexRange result = *this;

			if(OTHER.begin() > this->begin())
				result.set_begin(OTHER.begin());

			if(OTHER.end() < this->end())
				result.set_end(OTHER.end());

			return result;
		}

		inline bool					contains_index(	const IndexT									INDEX) const
		{
			return begin() <= INDEX && INDEX < end();
		}

		inline void					for_each(		std::function<void(IndexT)>						function) const
		{
			for(IndexT index = begin(); index < end(); ++index)
			{
				function(index);
			}
		}

		void						for_each_split(	const IndexT									NUM_SPLITS,
													std::function<void(const IndexRange<IndexT>&)>	function) const
		{
			const auto	SIZE		= size();
			const auto	AVR_COUNT	= SIZE / NUM_SPLITS;

			IndexRange<IndexT> current(begin(), begin() + AVR_COUNT + SIZE % NUM_SPLITS);

			for(IndexT splitID = 0; splitID < NUM_SPLITS; ++splitID)
			{
				if(current.size() > 0) function(current);
				*current.begin = current.end();
				*current.end += AVR_COUNT;
			}
		}

	private: // functions
		inline void					check_begin(	const IndexT									AMOUNT) const
		{
#ifdef _DEBUG
			if(begin() < AMOUNT)
				throw GeneralException(this, __LINE__, "Begin value cannot go below 0.");
#endif 
		}

		inline void					check_end(		const IndexT									AMOUNT) const
		{
#ifdef _DEBUG
			if(end() < AMOUNT)
				throw GeneralException(this, __LINE__, "End value cannot go below 0.");
#endif 
		}

		inline void					check_size() const
		{
#ifdef _DEBUG
			if(size() == 0)
				throw GeneralException(this, __LINE__, "Range size is equal 0.");
#endif
		}

		inline void					check_swapped() const
		{
#ifdef _DEBUG
			if(begin() > end())
				throw GeneralException(this, __LINE__, "Begin value cannot be greater than end value.");
#endif
		}
	};
}