#pragma once


#include "dpl_ReadOnly.h"
#include <cstdint>
#include <limits>
#include <stdexcept>


namespace dpl
{
	/*
		Generates up to 4,294,967,295 unique instances of the T.
		IDs of the destroyed instances cannot be assigned again until all instances of T are destroyed.
		IDs are swapped between instances on move-construction/assignment.
	*/
	template<typename T>
	class Unique
	{
	public: // data
		ReadOnly<uint32_t, Unique> ID; // Unique identificator of this instance.

	private: // data
		static ReadOnly<uint32_t, Unique> nextID; // ID that will be assigned to the next instance.
		static ReadOnly<uint32_t, Unique> numInstances; // Total number of instances of this class.

	protected: // lifecycle
		CLASS_CTOR		Unique()
			: ID((*nextID)++)
		{
			if(this->has_free_IDs())
			{
				++(*numInstances);
			}
			else
			{
				throw std::overflow_error("Too many instances of this class.");
			}
		}

		CLASS_CTOR		Unique(		const Unique&	OTHER)
			: Unique()
		{

		}

		CLASS_CTOR		Unique(		Unique&&		other) noexcept
			: Unique()
		{
			std::swap(*ID, *other.ID);
		}

		CLASS_DTOR		~Unique()
		{
			--(*numInstances);
			if(numInstances() == 0)
			{
				nextID = 0; // reset counter
			}
		}

		inline Unique&	operator=(	const Unique&	OTHER)
		{
			return *this; // do nothing
		}

		inline Unique&	operator=(	Unique&&		other) noexcept
		{
			std::swap(*ID, *other.ID);
			return *this;
		}

	public: // functions
		/*
			Returns true if this class can generate next instance.
		*/
		static bool		has_free_IDs()
		{
			return numInstances() != std::numeric_limits<uint32_t>::max();
		}
	};

//============ INITIALIZATION ============//
	template<typename T> ReadOnly<uint32_t, Unique<T>> Unique<T>::nextID		= 0;
	template<typename T> ReadOnly<uint32_t, Unique<T>> Unique<T>::numInstances	= 0;
}