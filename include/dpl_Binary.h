#pragma once


#include <iostream>
#include "dpl_TypeTraits.h"


// concepts
namespace dpl
{
	template<typename ContainerT>
	concept has_value_type		=  dpl::is_type_complete_v<typename ContainerT::value_type>;

	template<typename ContainerT>
	concept has_size_type		=  dpl::is_type_complete_v<typename ContainerT::size_type>;

	template<typename IteratorT>
	concept has_random_access	=  std::is_same_v<typename IteratorT::iterator_category, std::random_access_iterator_tag>;

	template<typename ContainerT>
	concept is_container		=  has_value_type<ContainerT> 
								&& has_size_type<ContainerT> 
								&& has_random_access<typename ContainerT::iterator>;
}

namespace dpl
{
	template<typename T>
	inline const uint8_t*	to_bytes(					const T*const			ADDRESS)
	{
		return reinterpret_cast<const uint8_t*const>(ADDRESS);
	}

	template<typename BaseT, typename DerivedT>
	inline intptr_t			base_offset()
	{
		static const uintptr_t	DUMMY_ADDRESS	= 0x00000000FFFFFFFF;
		static DerivedT*const	DUMMY_PTR		= (DerivedT*)DUMMY_ADDRESS;
		static const intptr_t	BASE_NUM		= (intptr_t)dpl::to_bytes<BaseT>(DUMMY_PTR);
		static const intptr_t	DERIVED_NUM		= (intptr_t)dpl::to_bytes<DerivedT>(DUMMY_PTR);
		return BASE_NUM - DERIVED_NUM;
	}

	template<typename T>
	inline void				import_t(					std::istream&			binary,
														T&						data)
	{
		if constexpr (std::is_trivially_destructible_v<T>)
		{
			binary.read(reinterpret_cast<char*>(&data), sizeof(T));
		}
		else // Force operator
		{
			data << binary;
		}
	}

	template<typename T>
	inline void				import_t(					std::istream&			binary,
														const size_t			SIZE,
														T*						data)
	{
		if constexpr (std::is_trivially_destructible_v<T>)
		{
			binary.read(reinterpret_cast<char*>(data), SIZE * sizeof(T));
		}
		else // Force operator
		{
			for(uint32_t index = 0; index < SIZE; ++index)
			{
				data[index] << binary;
			}
		}
	}

	template<typename T>
	inline T				import_t(					std::istream&			binary)
	{
		thread_local T variable;
		dpl::import_t(binary, variable);
		return variable;
	}

	template<typename T>
	inline void				export_t(					std::ostream&			binary,
														const T&				DATA)
	{
		if constexpr (std::is_trivially_destructible_v<T>)
		{
			binary.write(reinterpret_cast<const char*>(&DATA), sizeof(T));
		}
		else // Force operator
		{
			DATA >> binary;
		}
	}

	template<typename T>
	inline void				export_t(					std::ostream&			binary,
														const size_t			SIZE,
														const T*				DATA)
	{
		if constexpr (std::is_trivially_destructible_v<T>)
		{
			binary.write(reinterpret_cast<const char*>(DATA), SIZE * sizeof(T));
		}
		else // Force operator
		{
			for(uint32_t index = 0; index < SIZE; ++index)
			{
				DATA[index] >> binary;
			}
		}
	}

	template<typename ContainerT>
	inline void				import_static_container(	std::istream&			binary,
														ContainerT&				container)
	{
		dpl::import_t<typename ContainerT::value_type>(binary, import_t<typename ContainerT::size_type>(binary), container.data());
	}

	template<typename ContainerT>
	inline void				import_dynamic_container(	std::istream&			binary,
														ContainerT&				container)
	{
		const auto SIZE = import_t<typename ContainerT::size_type>(binary);
		container.resize(SIZE);
		dpl::import_t<typename ContainerT::value_type>(binary, SIZE, container.data());
	}
	
	template<typename ContainerT>
	inline void				export_container(			std::ostream&			binary,
														const ContainerT&		CONTAINER)
	{
		const auto SIZE = CONTAINER.size();
		dpl::export_t(binary, SIZE);
		dpl::export_t<typename ContainerT::value_type>(binary, SIZE, CONTAINER.data());
	}
}