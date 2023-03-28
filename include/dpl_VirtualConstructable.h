#pragma once


#include <functional>
#include <typeindex>
#include <string>
#include <unordered_map>
#include <vector>
#include <regex>
#include "dpl_GeneralException.h"
#include "dpl_NamedType.h"


namespace dpl
{
	template<typename BaseT, typename... CTOR>
	class VirtualConstructable
	{
	public: // subtypes
		using	MyType			= VirtualConstructable<BaseT, CTOR...>;
		using	CTOR_Invoker	= std::function<std::unique_ptr<BaseT>(CTOR&&...)>;

		struct	Generator
		{
			std::type_index	info;
			std::string		className;
			CTOR_Invoker	ctor;

			template<typename DerivedT>
			CLASS_CTOR Generator(	const DerivedT*			DUMMY_POINTER,
									const CTOR_Invoker&		CTOR)
				: info(typeid(DerivedT))
				, ctor(CTOR)
			{
				static const std::regex STRUCT_REGEX("\\s*\\bstruct \\b");
				static const std::regex CLASS_REGEX("\\s*\\bclass \\b");
				className = std::regex_replace(std::regex_replace(info.name(), STRUCT_REGEX, ""), CLASS_REGEX, "");
			}
		};

		// Maps class type with the unique ID.
		using	ClassTypeMap	= std::unordered_map<std::type_index, uint32_t>;

		// Maps class name(undecorated) with unique ID.
		using	ClassNameMap	= std::unordered_map<std::string, uint32_t>;

		// Maps unique ID with the class generator.
		using	ClassGenerators	= std::vector<Generator>;

	public: // constants
		static const uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

	private: // data
		static ClassTypeMap		sm_typeMap;
		static ClassNameMap		sm_nameMap;
		static ClassGenerators	sm_generators;

	public: // lifecycle
		CLASS_CTOR						VirtualConstructable() = default;

		CLASS_CTOR						VirtualConstructable(	const VirtualConstructable&				OTHER) = default;

		CLASS_CTOR						VirtualConstructable(	VirtualConstructable&&					other) noexcept = default;

		VirtualConstructable&			operator=(				const VirtualConstructable&				OTHER) = default;

		VirtualConstructable&			operator=(				VirtualConstructable&&					other) noexcept = default;

		CLASS_DTOR	virtual				~VirtualConstructable() = default;

	public: // static functions
		static uint32_t					get_typeID(				const std::type_index&					TYPE_INDEX)
		{
			auto it = sm_typeMap.find(TYPE_INDEX);
			return (it != sm_typeMap.end()) ? it->second : INVALID_INDEX;
		}

		static uint32_t					get_typeID(				const std::string&						CLASS_NAME)
		{
			auto it = sm_nameMap.find(CLASS_NAME);
			return (it != sm_nameMap.end()) ? it->second : INVALID_INDEX;
		}

		static uint32_t					count_typeIDs()
		{
			return static_cast<uint32_t>(sm_generators.size());
		}

		static bool						is_valid_typeID(		const uint32_t							TYPE_ID)
		{
			return TYPE_ID < (uint32_t)sm_generators.size();
		}

		/*
		*	Returns unique ID of the registered type or INVALID_INDEX if type was not registered.
		*/
		template<typename DerivedT>
		static uint32_t					get_typeID()
		{
			return get_typeID(typeid(DerivedT));
		}

		static const std::type_index	get_typeInfo(			const uint32_t							TYPE_ID)
		{
			return is_valid_typeID(TYPE_ID)? sm_generators[TYPE_ID].info : typeid(void);
		}

		static const std::string&		get_typeName(			const uint32_t							TYPE_ID)
		{
			return get_generator(TYPE_ID).className;
		}

		template<typename DerivedT>
		static const std::string&		get_typeName()
		{
			return get_typeName(get_typeID<DerivedT>());
		}

		static void						for_each_typeID(		const std::function<void(uint32_t)>&	FUNCTION)
		{
			for(auto& it : sm_typeMap)
			{
				FUNCTION(it.second);
			}
		}

		static std::unique_ptr<BaseT>	generate_object(		const uint32_t							TYPE_ID,
																CTOR&&...								args)
		{
			return get_generator(TYPE_ID).ctor(std::forward<CTOR>(args)...);
		}

		template<typename DerivedT>
		static std::unique_ptr<BaseT>	generate_object(		CTOR&&...								args)
		{
			return generate_object(get_typeID<DerivedT>(), std::forward<CTOR>(args)...);
		}

		static std::unique_ptr<BaseT>	generate_object(		const std::string&						CLASS_NAME,
																CTOR&&...								args)
		{
			return generate_object(get_typeID(CLASS_NAME), std::forward<CTOR>(args)...);
		}

		template<typename DerivedT>
		static inline void				register_generator()
		{
			static bool bONCE = [&]()
			{
				const auto UNIQUE_ID = static_cast<uint32_t>(sm_generators.size());

				const DerivedT* DUMMY_PTR = nullptr;
				Generator	newGenerator(DUMMY_PTR, [](CTOR&&... args)
							{
								return std::unique_ptr<BaseT>(new DerivedT(std::forward<CTOR>(args)...));
							});
	
				if(!sm_nameMap.emplace(newGenerator.className, UNIQUE_ID).second)
					throw GeneralException(__FILE__, __LINE__, "Class with that name was already registered. Try renaming your class(note that namespace is ignored during name generation).");
		
				sm_typeMap.emplace(newGenerator.info, UNIQUE_ID);
				sm_generators.emplace_back(newGenerator);
				return true;
			}();
		}

	private: // function
		inline static void				validate_typeID(		const uint32_t							TYPE_ID)
		{
			if(TYPE_ID < (uint32_t)sm_generators.size()) return; // OK
			throw GeneralException(__FILE__, __LINE__, "Unknown typeID: " + std::to_string(TYPE_ID));
		}

		inline static const Generator&	get_generator(			const uint32_t							TYPE_ID)
		{
			validate_typeID(TYPE_ID);
			return sm_generators[TYPE_ID];
		}
	};


	template<typename BaseT, typename... CTOR>
	std::unordered_map<std::type_index, uint32_t>							VirtualConstructable<BaseT, CTOR...>::sm_typeMap;

	template<typename BaseT, typename... CTOR>
	std::unordered_map<std::string, uint32_t>								VirtualConstructable<BaseT, CTOR...>::sm_nameMap;

	template<typename BaseT, typename... CTOR>
	std::vector<typename VirtualConstructable<BaseT, CTOR...>::Generator>	VirtualConstructable<BaseT, CTOR...>::sm_generators;
}