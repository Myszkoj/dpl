#pragma once


#include "dpl_ClassInfo.h"


namespace dpl
{
	/*
		Reference wrapper for swap operators.
	*/
	template<typename T>
	class Swap
	{
	private: // data
		T& obj;

	public: // lifecycle
		CLASS_CTOR	Swap(		T&			object) 
			: obj(object)
		{

		}

		CLASS_CTOR	Swap(		const Swap&	OTHER) = default;

		CLASS_CTOR	Swap(		Swap&&		other) noexcept = default;

		Swap&		operator=(	const Swap& OTHER) = default;

		Swap&		operator=(	Swap&&		other) noexcept = default;

		inline T&	operator*()
		{
			return obj;
		}

		inline T*	operator->()
		{
			return &obj;
		}

		inline T*	get()
		{
			return &obj;
		}
	};
}