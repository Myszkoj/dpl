#pragma once


#include "dpl_ClassInfo.h"


namespace dpl
{
	/*
		Stores TRUE/FALSE result of the function and associated data.
	*/
	template<typename DataT = const char*>
	class Result
	{
	private: // data
		DataT	m_data;
		bool	bState;

	public: // lifecycle
		CLASS_CTOR		Result(	const bool	STATE,
								DataT		DATA)
			: m_data(DATA)
			, bState(STATE)
		{

		}

		inline operator bool() const
		{
			return bState;
		}

		inline DataT	get() const
		{
			return m_data;
		}

		inline DataT	operator->()
		{
			return get();
		}
	};
}