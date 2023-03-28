#pragma once


#include <stdexcept>
#include "dpl_ReadOnly.h"

#pragma warning( disable : 26812 ) // Unscoped enum
#pragma warning( disable : 4293 ) // shift count negative or too big

// set bit functions
namespace dpl
{
	static const int DE_BRUIJN_BIT_TABLE[32] = {
		 0,  9,  1, 10, 13, 21,  2, 29,
		11, 14, 16, 18, 22, 25,  3, 30,
		 8, 12, 20, 28, 15, 17, 24,  7,
		19, 27, 23,  6, 26,  5,  4, 31, };

	// Set all bits except the highest one to 0.
	inline uint32_t			propagate_bits(uint32_t mask)
	{
		mask |= mask >> 1;
		mask |= mask >> 2;
		mask |= mask >> 4;
		mask |= mask >> 8;
		mask |= mask >> 16;
		return mask;
	}

	// Use DeBruijn's Sequence (32Bit version) to find highest set bit.
	inline uint8_t			get_highest_set_bit_index8(uint32_t mask)
	{
		return DE_BRUIJN_BIT_TABLE[(propagate_bits(mask) * 0x07C4ACDDu) >> 27];
	}

	template<uint32_t OFFSET, uint32_t COUNT> requires (OFFSET <= 7u) && (COUNT <= (8u - OFFSET))
		constexpr uint8_t	set_u8_bits()
	{
		return ((0xFFu >> (8u - COUNT)) << OFFSET);
	}

	template<uint32_t OFFSET, uint32_t COUNT> requires (OFFSET <= 15u) && (COUNT <= (16u - OFFSET))
		constexpr uint16_t	set_u16_bits()
	{
		return ((0xFFFFu >> (16u - COUNT)) << OFFSET);
	}

	template<uint32_t OFFSET, uint32_t COUNT> requires (OFFSET <= 31u) && (COUNT <= (32u - OFFSET))
		constexpr uint32_t	set_u32_bits()
	{
		return ((0xFFFFFFFFu >> (32u - COUNT)) << OFFSET);
	}
}

// declarations
namespace dpl
{
	template<typename UIntT>
	concept is_valid_mask_integer	=  std::is_integral_v<UIntT> 
									&& std::is_unsigned_v<UIntT>;


	template<is_valid_mask_integer T, typename EnumT = T>
	class Mask;

	template<typename EnumT>
	using Mask8		= Mask<uint8_t, EnumT>;

	template<typename EnumT>
	using Mask16	= Mask<uint16_t, EnumT>;

	template<typename EnumT>
	using Mask32	= Mask<uint32_t, EnumT>;

	template<typename EnumT>
	using Mask64	= Mask<uint64_t, EnumT>;

	using Mask8_t	= Mask<uint8_t>;
	using Mask16_t	= Mask<uint16_t>;
	using Mask32_t	= Mask<uint32_t>;
	using Mask64_t	= Mask<uint64_t>;
}

// implementation
namespace dpl
{
	template<is_valid_mask_integer UIntT, typename EnumT = UIntT>
	class Mask
	{
	public: // constants
		static const UIntT MAX_NUM_BITS = sizeof(UIntT) * 8u;

	private: // constants
		static const UIntT ONE_BIT		= 1;

	private: // data
		UIntT m_bits;

	public: // lifecycle
		CLASS_CTOR		Mask(			const UIntT						INITIAL_STATE = 0)
			: m_bits(INITIAL_STATE)
		{
			
		}

		CLASS_CTOR		Mask(			std::initializer_list<UIntT>	bits)
		{
			for(auto& bit : bits)
			{
				if(bit < MAX_NUM_BITS)
				{
					set_at(static_cast<EnumT>(bit), true);
				}
			}
		}

		/*inline operator	const UIntT&() const
		{
			return m_bits;
		}*/

	public: // operators
		inline Mask		operator~() const
		{
			return Mask(~m_bits);
		}

		inline bool		operator==(		const Mask&				OTHER) const
		{
			return m_bits == OTHER.m_bits;
		}

		inline bool		operator!=(		const Mask&				OTHER) const
		{
			return m_bits != OTHER.m_bits;
		}

		inline Mask		operator|(		const Mask&				OTHER) const
		{
			return Mask(m_bits | OTHER.m_bits);
		}

		inline Mask		operator&(		const Mask&				OTHER) const
		{
			return Mask(m_bits & OTHER.m_bits);
		}

		inline Mask&	operator|=(		const Mask&				OTHER)
		{
			m_bits |= OTHER.m_bits;
			return *this;
		}

		inline Mask&	operator&=(		const Mask&				OTHER)
		{
			m_bits &= OTHER.m_bits;
			return *this;
		}

		inline UIntT	operator|(		const UIntT				BITS) const
		{
			return m_bits | BITS;
		}

		inline UIntT	operator&(		const UIntT				BITS) const
		{
			return m_bits & BITS;
		}

	public: // functions
		uint32_t		count_set_bits() const
		{
			uint32_t	count	= 0;
			UIntT		tmp		= m_bits;
			while (tmp) 
			{
				count += tmp & ONE_BIT;
				tmp >>= ONE_BIT;
			}
			return count;
		}

		inline void		reset(			const UIntT				BITS)
		{
			m_bits = BITS;
		}

		/*
		*	Sets specified bit to true or false.
		*/
		inline void		set_at(			const EnumT				BIT_INDEX,
										const bool				bVALUE = true)
		{
			validate_at(BIT_INDEX);
			bVALUE	? m_bits |= (ONE_BIT<<BIT_INDEX) 
					: m_bits &= ~(ONE_BIT<<BIT_INDEX);
		}

		/*
		*	Sets specified bit to true or false.
		*/
		inline void		toggle_at(		const EnumT				BIT_INDEX)
		{
			validate_at(BIT_INDEX);
			m_bits ^= (ONE_BIT<<BIT_INDEX);
		}

		/*
		*	Returns state of a bit at specified index.
		*/
		inline bool		at(				const EnumT				BIT_INDEX) const
		{
			validate_at(BIT_INDEX);
			return m_bits & (ONE_BIT<<BIT_INDEX);
		}

		/*
			Sets given bits to true or false.
		*/
		inline void		set(			const EnumT				BITS,
										const bool				bVALUE)
		{
			bVALUE	? m_bits |= BITS 
					: m_bits &= ~BITS;
		}

		inline bool		none() const
		{
			return m_bits == 0;
		}

		inline bool		any() const
		{
			return m_bits != 0;
		}

		/*
		*	Returns true if at least one of the following bits is true.
		*/
		inline bool		any_of(			const UIntT				BITS) const
		{
			return (m_bits & BITS) > 0;
		}

		inline bool		any_of(			const Mask				OTHER) const
		{
			return any_of(OTHER.m_bits);
		}

		/*
		*	Returns true if mask and BITS share the same true bits.
		*/
		inline bool		fits(			const UIntT				BITS) const
		{
			return (m_bits & BITS) == BITS;
		}

		/*
		*	Sets all bits to 0.
		*/
		inline void		clear()
		{
			m_bits = UIntT(0);
		}

		/*
		*	Returns state of all bits.
		*/
		inline UIntT	get() const
		{
			return m_bits;
		}

		/*
		*	Returns reference to the bits.
		*/
		inline UIntT&	access()
		{
			return m_bits;
		}

	private: // functions
		inline void		validate_at(	const EnumT				BIT_INDEX) const
		{
#ifdef _DEBUG
			if(static_cast<UIntT>(BIT_INDEX) >= MAX_NUM_BITS)
				throw std::out_of_range("Mask: Invalid BIT_INDEX");
#endif
		}
	};
}