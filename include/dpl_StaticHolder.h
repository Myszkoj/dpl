#pragma once


#include "dpl_ReadOnly.h"


#pragma pack(push, 1)

namespace dpl
{
	template<typename T>
	concept is_default_constructible_static_data = std::is_default_constructible_v<T>;

	/*
		This simple template declares and initializes a static variable that belongs to the ClassT, 
		so in case of the header-only class, you don't have to create a dedicated .cpp file just to initialize it.
	*/
	template<is_default_constructible_static_data T, typename ClassT>
	class StaticHolder
	{
	protected: // data
		static T data;

	public: // lifecycle
		CLASS_CTOR		StaticHolder() = default;

		CLASS_CTOR		StaticHolder(	StaticHolder&&				other) noexcept = default;

		CLASS_CTOR		StaticHolder(	const StaticHolder&			OTHER) = default;

		StaticHolder&	operator=(		StaticHolder&&				other) noexcept = default;

		StaticHolder&	operator=(		const StaticHolder&			OTHER) = default;
	};


	template<is_default_constructible_static_data T, typename ClassT>
	T StaticHolder<T, ClassT>::data;
}

#pragma pack(pop)