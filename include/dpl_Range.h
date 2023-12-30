#pragma once


#include <functional>
#include <ratio>
#include <iostream>
#include <type_traits>
#include "dpl_ReadOnly.h"
#include "dpl_GeneralException.h"


namespace dpl
{
	template<typename T>
	class Range
	{
	public:		// [SUBTYPES]
		using Invoke = std::function<void(T)>;

	public:		// [DATA]
		ReadOnly<T, Range> min;
		ReadOnly<T, Range> max;

	public:		// [LIFECYCLE]
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

	public:		// [OPERATORS]
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

	public:		// [FUNCTIONS]
		void					set_min(	const T			NEW_MIN)
		{
			min = NEW_MIN;
			set_max(max());
		}

		void					set_max(	const T			NEW_MAX)
		{
			max = (min() > NEW_MAX) ? min() : NEW_MAX;
		}

		void					reset(		const T			NEW_MIN,
											const T			NEW_MAX)
		{
			min = NEW_MIN;
			set_max(NEW_MAX);
		}

		T						size() const
		{
			return max() - min();
		}

		T						center() const
		{
			return (min() + max()) / (T)2;
		}

		T						clamp(		const T			VALUE) const
		{
			if(VALUE < min()) return min();
			if(VALUE > max()) return max();
			return VALUE;
		}

		bool					contains(	const T			VALUE) const
		{
			return (min() <= VALUE) && (VALUE >= max()); 
		}

		float					to_factor(	T				value) const
		{
			value = clamp(value);
			return (float)(value-min())/(size());
		}

		T						from_factor(float			factor) const
		{
			if(factor < 0.f) factor = 0.f;
			if(factor > 1.f) factor = 1.f;
			return min() + (T)(factor * size());
		}

		template <std::integral U = T>
		void					for_each(	const Invoke&	INVOKE) const
		{
			for(uint32_t index = min(); index <= max(); ++index)
			{
				INVOKE(index);
			}
		}
	};


	template<std::unsigned_integral IndexT = uint32_t>
	class IndexRange
	{
	public:		// [CONSTANTS]
		static const IndexRange WHOLE;

	public:		// [SUBTYPES]
		using InvokeAt			= std::function<void(IndexT)>;
		using InvokeSubrange	= std::function<void(const IndexRange<IndexT>&)>;

	public:		// [DATA]
		ReadOnly<IndexT, IndexRange> begin;
		ReadOnly<IndexT, IndexRange> end;

	public:		// [LIFECYCLE]
		CLASS_CTOR				IndexRange()
			: begin(0)
			, end(0)
		{

		}

		CLASS_CTOR				IndexRange(		const IndexT			BEGIN)
			: begin(BEGIN)
			, end(BEGIN)
		{

		}

		CLASS_CTOR				IndexRange(		const IndexT			BEGIN,
												const IndexT			END)
			: begin(BEGIN)
			, end(END)
		{
			check_swapped();
		}

	public:		// [FUNCTIONS]
		IndexT					size() const
		{
			return end() - begin();
		}

		bool					empty() const
		{
			return size() == 0;
		}

		IndexT					front() const
		{
			check_size();
			return begin();
		}

		IndexT					back() const
		{
			check_size();
			return end() - 1;
		}

		void					set_begin(		const IndexT			NEW_BEGIN)
		{
			begin	= NEW_BEGIN;
			end		= (NEW_BEGIN > end()) ? NEW_BEGIN : end();
		}

		void					set_end(		const IndexT			NEW_END)
		{
			begin	= (begin() < NEW_END) ? begin() : NEW_END;
			end		= NEW_END;
		}

		void					reset()
		{
			begin	= 0;
			end		= 0;
			check_swapped();
		}

		void					reset(			const IndexT			NEW_BEGIN,
												const IndexT			NEW_END)
		{
			begin	= NEW_BEGIN;
			end		= NEW_END;
			check_swapped();
		}

		void					increase_front(	const IndexT			AMOUNT = 1)
		{
			check_begin(AMOUNT);
			*begin -= AMOUNT;
		}

		void					increase_back(	const IndexT			AMOUNT = 1)
		{
			*end += AMOUNT;
		}

		void					decrease_front(	const IndexT			AMOUNT = 1)
		{
			*begin += AMOUNT;
			check_swapped();
		}

		void					decrease_back(	const IndexT			AMOUNT = 1)
		{
			check_end(AMOUNT);
			*end -= AMOUNT;
			check_swapped();
		}

		void					shift_front(	const IndexT			AMOUNT = 1)
		{
			check_begin(AMOUNT);
			*begin	-= AMOUNT;
			*end	-= AMOUNT;
		}

		void					shift_back(		const IndexT			AMOUNT = 1)
		{
			*begin	+= AMOUNT;
			*end	+= AMOUNT;
		}

		IndexRange				intersection(	const IndexRange&		OTHER) const
		{
			IndexRange result = *this;

			if(OTHER.begin() > this->begin())
				result.set_begin(OTHER.begin());

			if(OTHER.end() < this->end())
				result.set_end(OTHER.end());

			return result;
		}

		bool					contains_index(	const IndexT			INDEX) const
		{
			return begin() <= INDEX && INDEX < end();
		}

		void					for_each(		const InvokeAt&			INVOKE) const
		{
			for(IndexT index = begin(); index < end(); ++index)
			{
				INVOKE(index);
			}
		}

		void					for_each_split(	const IndexT			NUM_SPLITS,
												const InvokeSubrange&	INVOKE) const
		{
			const auto	SIZE		= size();
			const auto	AVR_COUNT	= SIZE / NUM_SPLITS;

			IndexRange<IndexT> subrange(begin(), begin() + AVR_COUNT + SIZE % NUM_SPLITS);

			for(IndexT splitID = 0; splitID < NUM_SPLITS; ++splitID)
			{
				if(subrange.size() > 0) INVOKE(subrange);
				*subrange.begin = subrange.end();
				*subrange.end += AVR_COUNT;
			}
		}

	private:	// [FUNCTIONS]
		void					check_begin(	const IndexT			AMOUNT) const
		{
#ifdef _DEBUG
			if(begin() < AMOUNT)
				throw GeneralException(this, __LINE__, "Begin value cannot go below 0.");
#endif 
		}

		void					check_end(		const IndexT			AMOUNT) const
		{
#ifdef _DEBUG
			if(end() < AMOUNT)
				throw GeneralException(this, __LINE__, "End value cannot go below 0.");
#endif 
		}

		void					check_size() const
		{
#ifdef _DEBUG
			if(size() == 0)
				throw GeneralException(this, __LINE__, "Range size is equal 0.");
#endif
		}

		void					check_swapped() const
		{
#ifdef _DEBUG
			if(begin() > end())
				throw GeneralException(this, __LINE__, "Begin value cannot be greater than end value.");
#endif
		}
	};


	template<std::unsigned_integral IndexT>
	const IndexRange<IndexT> IndexRange<IndexT>::WHOLE(0, std::numeric_limits<IndexT>::max());
}