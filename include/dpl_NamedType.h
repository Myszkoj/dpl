#pragma once


#include "dpl_ReadOnly.h"
#include <string>


namespace dpl
{
	template<typename T>
	std::string	undecorate_type_name()
	{
		// TODO: This function should be optimized with string_view!!!
		std::string type_name = typeid(T).name();

		const size_t FOUND_TEMPLATE_START = type_name.find_first_of("<");
		if(FOUND_TEMPLATE_START != std::string::npos)
		{
			type_name.erase(FOUND_TEMPLATE_START, type_name.size() - FOUND_TEMPLATE_START);
		}

		const size_t FOUND_NAMESPACE_END = type_name.find_last_of(":");
		if(FOUND_NAMESPACE_END != std::string::npos)
		{
			type_name.erase(0, FOUND_NAMESPACE_END+1);
		}
		else
		{
			const size_t FOUND_WHITESPACE = type_name.find_last_of(" ");
			if(FOUND_WHITESPACE != std::string::npos)
			{
				type_name.erase(0, FOUND_WHITESPACE+1);
			}
		}
		
		return type_name;
	}


	template<typename T>
	class	NamedType
	{
	public: // data
		static ReadOnly<std::string, NamedType>	typeName;
	};


	template<typename T>
	ReadOnly<std::string,	NamedType<T>>	NamedType<T>::typeName = dpl::undecorate_type_name<T>();
}