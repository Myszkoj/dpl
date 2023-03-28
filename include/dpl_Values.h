#pragma once


#include <functional>
#include "dpl_Mask.h"
#include "dpl_Range.h"
#include "dpl_Binary.h"


#pragma pack(push, 1)

// dirty value
namespace dpl
{
	template<typename StructureT, typename MemberT> 
	const size_t member_offset(const MemberT StructureT::*MEMBER)
	{
		return ((::size_t) &reinterpret_cast<char const volatile&>((((StructureT*)nullptr)->*MEMBER)));
	}


	template<typename ValueT, typename ClassT, typename MaskT, size_t BIT_INDEX, bool bDIRTY_STATE = true>
	class DirtyValue
	{
	public: // relations
		friend ClassT;

	public: // subtypes
		using Type		= DirtyValue<ValueT, ClassT, MaskT, BIT_INDEX, bDIRTY_STATE>;
		using Modify	= std::function<void(ValueT&)>;
		using Update	= std::function<void(ValueT&, const MaskT)>;

		static_assert(BIT_INDEX < sizeof(MaskT) * 8, "Invalid bit index.");

	public: // constants
		static const int64_t					INVALID_OFFSET = std::numeric_limits<int64_t>::max();
		static ReadOnly<int64_t, DirtyValue>	OFFSET_FROM_MASK;

	private: // data
		dpl::ReadOnly<ValueT, DirtyValue> value;

	public: // lifecycle
		template<typename... CTOR>
		CLASS_CTOR				DirtyValue(				const void*				MASK_ADDRESS,
														CTOR&&...				args)
			: value(std::forward<CTOR>(args)...)
		{
			//const int64_t THIS_OFFSET_FROM_MASK = ::member_offset(MASK_ADDRESS) - (reinterpret_cast<const uint8_t*>(this) - reinterpret_cast<const uint8_t*>(CLASS_ADDRESS));
			const int64_t THIS_OFFSET_FROM_MASK = reinterpret_cast<const uint8_t*>(MASK_ADDRESS) - reinterpret_cast<const uint8_t*>(this);
			if(OFFSET_FROM_MASK == INVALID_OFFSET)
			{
				OFFSET_FROM_MASK = THIS_OFFSET_FROM_MASK;
			}
			else if(THIS_OFFSET_FROM_MASK != OFFSET_FROM_MASK)
			{
				throw GeneralException(this, __LINE__, "Reassignment of OFFSET_FROM_MASK.");
			}
		}

		CLASS_CTOR				DirtyValue(				const DirtyValue&		OTHER)
			: value(OTHER.value)
		{
			make_dirty();
		}

		CLASS_CTOR				DirtyValue(				DirtyValue&&			other) noexcept
			: value(std::move(other.value))
		{

		}

	public: // operators
		inline DirtyValue&		operator=(				const DirtyValue&		OTHER)
		{
			set(OTHER.value);
			return *this;
		}

		inline DirtyValue&		operator=(				DirtyValue&&			other) noexcept
		{
			value = std::move(other.value);
			return *this;
		}

		inline DirtyValue&		operator=(				const ValueT&			NEW_VALUE)
		{
			set(NEW_VALUE);
			return *this;
		}

		inline operator			const ValueT&() const
		{
			return value;
		}

	public: // functions
		inline void				swap(					DirtyValue&				other)
		{
			value.swap(other.value);
			this->make_dirty();
			other.make_dirty();
		}

		inline void				set(					const ValueT&			NEW_VALUE)
		{
			value = NEW_VALUE;
			make_dirty();
		}

		inline void				set(					const Modify&			MODIFY)
		{
			MODIFY(*value);
			make_dirty();
		}

		inline const ValueT&	get() const
		{
			return value;
		}

		inline void				validate_mask() const
		{
#ifdef _DEBUG
			if(OFFSET_FROM_MASK == INVALID_OFFSET) // This should never happen.
				throw GeneralException(this, __LINE__, "OFFSET_FROM_MASK is not assigned.");
#endif
		}

		inline const MaskT&		get_mask() const
		{
			validate_mask();
			return *reinterpret_cast<const MaskT*>(reinterpret_cast<const char*>(this) + OFFSET_FROM_MASK());
		}

		inline void				make_dirty()
		{
			get_mask().set_at(BIT_INDEX, bDIRTY_STATE);
		}

	private: // functions
		inline MaskT&			get_mask()
		{
			validate_mask();
			return *reinterpret_cast<MaskT*>(reinterpret_cast<char*>(this) + OFFSET_FROM_MASK());
		}

		/*
			Invokes UPDATE if value was modified, then clears the flag. 
		*/
		inline void				update(					const Update&			UPDATE)
		{
			MaskT& mask = get_mask();
			if(mask.at(BIT_INDEX) == bDIRTY_STATE)
			{
				UPDATE(*value, mask);
				mask.set_at(BIT_INDEX, !bDIRTY_STATE);
			}
		}
	};


	template<typename ValueT, typename ClassT, typename MaskT, size_t BIT_INDEX, bool bDIRTY_STATE>
	ReadOnly<int64_t, DirtyValue<ValueT, ClassT, MaskT, BIT_INDEX, bDIRTY_STATE>> DirtyValue<ValueT, ClassT, MaskT, BIT_INDEX, bDIRTY_STATE>::OFFSET_FROM_MASK = DirtyValue<ValueT, ClassT, MaskT, BIT_INDEX, bDIRTY_STATE>::INVALID_OFFSET;
}

// ranged value
namespace dpl
{
	/*
		Stores given value T and keeps it within the <MIN, MAX> range.
	 
		[PROBLEM]: 
		- When T is a double, do not use std::numeric_limits<double>::max()! There is something wrong with template values of that magnitude

		[SOLUTION]
		- Use std::numeric_limits<float>::max() and cast to double
	*/
	template<typename T, T MIN, T MAX, T DEFAULT = MIN>
	class RangedValue
	{
	public: // constants
		static const Range<T> RANGE;

	private: // data
		T value;

	public: // lifecycle
		CLASS_CTOR				RangedValue()
			: value(clamp(DEFAULT))
		{
			
		}

		CLASS_CTOR				RangedValue(		const T				VALUE)
			: value(RangedValue::clamp(VALUE))
		{

		}

	public: // operators
		inline operator			const T() const
		{
			return value;
		}

		inline bool				operator==(			const T				OTHER) const
		{
			return value == OTHER;
		}

		inline bool				operator!=(			const T				OTHER) const
		{
			return value != OTHER;
		}

		inline bool				operator==(			const RangedValue&	OTHER) const
		{
			return value == OTHER;
		}

		inline bool				operator!=(			const RangedValue&	OTHER) const
		{
			return value != OTHER;
		}

		friend std::istream&	operator>>(			std::istream&		stream, 
													RangedValue&		ranged) 
		{
			stream >> ranged.value;
			ranged.clamp();
			return stream;
		}

		friend std::ostream&	operator<<(			std::ostream&		stream, 
													const RangedValue&	RANGED)
		{
			stream << RANGED.value;
			return stream;
		}

	public: // functions
		inline void				swap(				RangedValue&		other) noexcept
		{
			std::swap(value, other.value);
		}

		inline void				set(				const T				NEW_VALUE)
		{
			value = RangedValue::clamp(NEW_VALUE);
		}

		inline void				set_default()
		{
			set(DEFAULT);
		}

		bool					control_set(		T					newValue)
		{
			newValue = RangedValue::clamp(newValue);
			if(newValue == value) return false;
			value = newValue;
			return true;
		}

		inline const T			get() const
		{
			return value;
		}

		inline void				minimize()
		{
			value = RANGE.min();
		}

		inline void				maximize()
		{
			value = RANGE.max();
		}

	private: // functions
		inline void				clamp()
		{
			if(value < RANGE.min()) value = RANGE.min();
			if(value > RANGE.max()) value = RANGE.max();
		}

		static inline T			clamp(				const T				VALUE)
		{
			if(VALUE < RANGE.min()) return RANGE.min();
			if(VALUE > RANGE.max()) return RANGE.max();
			return VALUE;
		}
	};


	template<typename T, T MIN, T MAX, T DEFAULT>
	const Range<T> RangedValue<T, MIN, MAX, DEFAULT>::RANGE = Range<T>(MIN, MAX);
}

// common value
namespace dpl
{
	template<typename T, T DEFAULT>
	class CommonValue
	{
	private: // data
		static T value;

	public: // lifecycle
		CLASS_CTOR		CommonValue() = default;

		CLASS_CTOR		CommonValue(	const CommonValue&	OTHER) = default;

		CLASS_CTOR		CommonValue(	CommonValue&&		other) noexcept = default;

		CommonValue&	operator=(		const CommonValue&	OTHER) = default;

		CommonValue&	operator=(		CommonValue&&		other) noexcept = default;

	public: // operators
		inline operator const T&() const
		{
			return value;
		}

		inline const T& operator()() const
		{
			return value;
		}

		inline bool		operator==(		const T&			OTHER) const
		{
			return value == OTHER;
		}

		inline bool		operator!=(		const T&			OTHER) const
		{
			return value != OTHER;
		}

		inline bool		operator==(		const CommonValue&	OTHER) const
		{
			return value == OTHER;
		}

		inline bool		operator!=(		const CommonValue&	OTHER) const
		{
			return value != OTHER;
		}

	public: // functions
		inline void		swap(		CommonValue&		other) noexcept
		{
			std::swap(value, other.value);
		}

		inline void		set(		const T				NEW_VALUE)
		{
			value = NEW_VALUE;
		}

		inline void		set_default()
		{
			set(DEFAULT);
		}

		inline const T& get() const
		{
			return value;
		}
	};


	template<typename T, T DEFAULT>
	T CommonValue<T, DEFAULT>::value = DEFAULT;
}

#pragma pack(pop)