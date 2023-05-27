#pragma once


#include <string>
#include <vector>
#include <stdexcept>
#include <limits>


namespace dpl
{
	enum VectorConstants : uint64_t
	{
		INVALID_VECTOR_INDEX = std::numeric_limits<uint64_t>::max()
	};


	inline std::wstring	to_wstring(			const std::string_view&						STR)
	{
		return std::wstring(STR.data(), STR.data() + STR.size());
	}


	template<class T>
	inline uint64_t		get_element_index(	const std::vector<T>&						CONTAINER,
											const T*									ELEMENT_ADDRESS)
	{
		if(ELEMENT_ADDRESS < CONTAINER.data())						return dpl::INVALID_VECTOR_INDEX;
		if(ELEMENT_ADDRESS >= CONTAINER.data() + CONTAINER.size())	return dpl::INVALID_VECTOR_INDEX;
		return ELEMENT_ADDRESS - CONTAINER.data();
	}

	template<class T>
	inline uint64_t		get_element_index(	const std::vector<T>&						CONTAINER,
											const T&									ELEMENT)
	{
		return dpl::get_element_index(CONTAINER, &ELEMENT);
	}

	/* 
		Efficiently remove an element from a vector without
		preserving order. If the element is not the last element
		in the vector, transfer the last element into its position
		using a move if possible.
		Regardless, we then shrink the size of the vector deleting
		the element at the end.
		Note that this function invalidates the iterator. 
	*/
	template<class ValueType>
	inline void			fast_remove(		typename std::vector<ValueType>&			container,
											typename std::vector<ValueType>::iterator	it)
	{
		auto last = container.end() - 1;
		if (it != last) *it = std::move(*last);		
		container.pop_back();
	}

	template<class ValueType>
	inline void			fast_remove(		typename std::vector<ValueType>&			container,
											ValueType*									ELEMENT_ADDRESS)
	{
		const auto INDEX = dpl::get_element_index(container, ELEMENT_ADDRESS);
		if(INDEX == dpl::INVALID_VECTOR_INDEX)
			throw std::out_of_range("'fast_remove' failure. Vector element out of range.");

		dpl::fast_remove(container, container.begin() + INDEX);
	}

	template<class ValueType>
	inline void			fast_remove(		typename std::vector<ValueType>&			container,
											ValueType&									ELEMENT_ADDRESS)
	{
		const auto INDEX = dpl::get_element_index(container, &ELEMENT_ADDRESS);
		if(INDEX == dpl::INVALID_VECTOR_INDEX)
			throw std::out_of_range("'fast_remove' failure. Vector element out of range.");

		dpl::fast_remove(container, container.begin() + INDEX);
	}
}