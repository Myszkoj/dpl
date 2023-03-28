#pragma once


#include <type_traits>
#include <tuple>
#include <stdexcept>
#include <cstddef> // offsetof


namespace dpl
{
	template<typename TupleT, size_t INDEX>
	constexpr size_t tuple_element_byte_offset()
	{
		return ( (::size_t)&reinterpret_cast<char const volatile&>( (std::get<INDEX>(*((TupleT*)0))) ) );
	}


	template<typename... Ts>
	class	TypeList
	{
	private:
		template <typename T, typename Tuple>
		struct HasType;

		template <typename T>
		struct HasType<T, TypeList<>> : std::false_type {};

		template <typename T, typename U, typename... Ts>
		struct HasType<T, TypeList<U, Ts...>> : HasType<T, TypeList<Ts...>> {};

		template <typename T, typename... Ts>
		struct HasType<T, TypeList<T, Ts...>> : std::true_type {};

		template <class T, class TPack>
		struct IndexOf;

		template <class T, class... Types>
		struct IndexOf<T, TypeList<T, Types...>> 
		{
			static constexpr uint32_t INDEX = 0;
		};

		template <class T0, class T1, class... Types>
		struct IndexOf<T0, TypeList<T1, Types...>> 
		{
			static constexpr uint32_t INDEX = 1 + IndexOf<T0, TypeList<Types...>>::INDEX;
		};

		template <uint32_t idx, class... Types> requires (idx < sizeof...( Types ))
		struct Extract
		{
		private:
			template <uint32_t i, uint32_t n, class... Rest>
			struct extract_impl;

			template <uint32_t i, uint32_t n, class T, class... Rest>
			struct extract_impl<i, n, T, Rest...>
			{
				using type = typename extract_impl<i + 1, n, Rest...>::type;
			};

			template <uint32_t n, class T, class... Rest>
			struct extract_impl<n, n, T, Rest...>
			{
				using type = T;
			};

		public:
			using type = typename extract_impl<0, idx, Types...>::type;
		};

		template<typename T>
		struct Increment
		{
			static constexpr size_t VALUE = 1;
		};

		template <typename T, typename... Ps>
		struct MakeUnique 
		{
			using Type = T;
		};

		template <template<class...> class TypeListT, typename... Ps, typename U, typename... Us>
		struct MakeUnique<TypeListT<Ps...>, U, Us...>
				: std::conditional_t<(std::is_same_v<U, Ps> || ...)
				, MakeUnique<TypeListT<Ps...>, Us...>
				, MakeUnique<TypeListT<Ps..., U>, Us...>> {};

		template<typename T, typename ValueT>
		struct Uniform
		{
			ValueT value;

			template<typename... CTOR>
			Uniform(CTOR&&... args)
				: value(std::forward<CTOR>(args)...)
			{

			}
		};

		template <typename... Ts>
		struct Reverse;

		template <>
		struct Reverse<>
		{
			using types = dpl::TypeList<>;
		};

		template <typename T>
		struct Reverse<T>
		{
			using types = dpl::TypeList<T>;
		};

		template <typename T, typename... Ts>
		struct Reverse<T, Ts...>
		{
		private:
			using tail	= typename Reverse<Ts...>::types;
		
		public:
			using types	= tail::template push_back<T>;
		};

		template<template<typename> class, class... Vs >
		static auto get_Subtypes( TypeList<>, TypeList<Vs...> v ) { return v; }

		template<template<typename> class PREDICATE, class U, class... Us, class... Vs >
		static auto get_Subtypes( TypeList<U, Us...>, TypeList<Vs...> ) 
		{ 
			return get_Subtypes<PREDICATE>(TypeList<Us...>{}, std::conditional_t<PREDICATE<U>::value, TypeList<U, Vs...>, TypeList<Vs...> >{}
		);}

	public:
		template<typename T>
		struct	Type
		{
			static constexpr uint32_t INDEX = IndexOf<T, TypeList<Ts...>>::INDEX;
		};

		/*
			Stores ValueT for each type in the type list.
		*/
		template<typename ValueT>
		struct	UniformArray : public std::tuple<Uniform<Ts, ValueT>...>
		{
			using MyBase = std::tuple<Uniform<Ts, ValueT>...>;
			using MyBase::MyBase;
			using MyBase::operator=;

			template<typename T>
			inline ValueT&			get_value()
			{
				return std::get<Uniform<T, ValueT>>(*this).value;
			}

			template<typename T>
			inline const ValueT&	get_value()const
			{
				return std::get<Uniform<T, ValueT>>(*this);
			}
		};

		using	Unique			= typename MakeUnique<TypeList<>, Ts...>::Type;
		
		template<uint32_t INDEX>
		using	At				= typename Extract<INDEX, Ts...>::type;

		template <template <typename> class PREDICATE>
		using	Subtypes		= decltype(TypeList::get_Subtypes<PREDICATE>( TypeList<Ts...>{}, TypeList<>{} ));

		template<typename T>
		using	push_front		= dpl::TypeList<T, Ts...>;

		template<typename T>
		using	push_back		= dpl::TypeList<Ts..., T>;

		template<bool DUMMY = true> //<-- without a dummy, this would result in a infinite loop of definitions
		using	reverse			= typename Reverse<Ts...>::types;

		using	DataPack_r		= std::tuple<Ts...>;
		using	PtrPack_r		= std::tuple<Ts*...>;
		using	ConstPtrPack_r	= std::tuple<const Ts*...>;
		using	DataPack		= reverse<true>::DataPack_r;
		using	PtrPack			= reverse<true>::PtrPack_r;
		using	ConstPtrPack	= reverse<true>::ConstPtrPack_r;

		static constexpr size_t SIZE		= (0 + ... + Increment<Ts>::VALUE);
		static constexpr bool	ALL_UNIQUE	= std::is_same_v<Unique, TypeList<Ts...>>;

		template <typename T>
		static constexpr bool		has_type()
		{
			return HasType<T, TypeList<Ts...>>::value;
		}

		template<typename T>
		static constexpr uint32_t	index_of()
		{
			return IndexOf<T, TypeList<Ts...>>::INDEX;
		}

		template <template <typename> class PREDICATE>
		static constexpr bool		all()
		{
			return std::conjunction_v<PREDICATE<Ts>...>;
		}

		template <template <typename> class PREDICATE>
		static constexpr bool		any()
		{
			return std::disjunction_v<PREDICATE<Ts>...>;
		}

		template<typename T>
		static constexpr size_t		data_byte_offset()
		{
			using ReversedType = reverse<true>;
			return dpl::tuple_element_byte_offset<DataPack, ReversedType::template index_of<T>()>();
		}

		template<typename T>
		static constexpr size_t		rdata_byte_offset()
		{
			return dpl::tuple_element_byte_offset<DataPack_r, index_of<T>()>();
		}
	};

	template<template<typename> class UCond, typename... Ts>
	struct	FilterOut
	{
	private:
		template<typename T, typename... Ps>
		struct	Test 
		{
			using Type = T;
		};

		template <template<class...> class TypeListT, typename... Ps, typename U, typename... Us>
		struct	Test<TypeListT<Ps...>, U, Us...>
				: std::conditional_t< UCond<U>::value
									, Test<TypeListT<Ps...>, Us...>
									, Test<TypeListT<Ps..., U>, Us...>> {};

	public: 
		using Types	= typename Test<dpl::TypeList<>, Ts...>::Type;
	};

	template <typename...> 
	struct	merge;

	template<typename...Types>
	struct	merge<TypeList<Types...>> // Specialization for single type list.
	{
		using type = TypeList<Types...>;
	};

	template<typename... ATypes, typename... BTypes>
	struct	merge<TypeList<ATypes...>, TypeList<BTypes...>> // Specialization for two type lists.
	{
		using type = TypeList<ATypes..., BTypes...>;
	};

	template<typename... ATypes, typename... BTypes, typename... Tail>
	struct	merge<TypeList<ATypes...>, TypeList<BTypes...>, Tail...> // Specialization for more than two type lists.
	{
		using type = merge<TypeList<ATypes..., BTypes...>, Tail...>::type;
	};

	template<typename... TypeLists>
	using	merge_t = typename merge<TypeLists...>::type;

	template<typename, typename = void>
	constexpr bool is_type_complete_v = false;

	template<typename T>
	constexpr bool is_type_complete_v<T, std::void_t<decltype(sizeof(T))>> = true;


	template<typename Test, template<typename...> class Ref>
	struct IsSpecialization : std::false_type {};

	template<template<typename...> class Ref, typename... Args>
	struct IsSpecialization<Ref<Args...>, Ref>: std::true_type {};

	template<typename T, template<typename...> class Ref>
	constexpr bool is_specialization_v = IsSpecialization<T, Ref>::value;

	template<typename ClassT, typename OtherT>
	struct HasAssignmentOperator
	{
	private:
		template<typename T>
		static constexpr auto test(T*) -> typename std::is_same<ClassT&, decltype( std::declval<T>() = std::declval<OtherT>() )>::type;

		template<typename>
		static constexpr auto test(...) -> std::false_type;

	public:
		static constexpr bool value = decltype(test<ClassT>(0))::value;
	};

	template<typename ClassT, typename OtherT>
	struct HasShiftLeftOperator
	{
	private:
		template<typename T>
		static constexpr auto test(T*) -> typename std::is_same<ClassT&, decltype( std::declval<T>() << std::declval<OtherT>() )>::type;

		template<typename>
		static constexpr auto test(...) -> std::false_type;

	public:
		static constexpr bool value = decltype(test<ClassT>(0))::value;
	};

	template<typename ClassT, typename OtherT>
	struct HasShiftRightOperator
	{
	private:
		template<typename T>
		static constexpr auto test(T*) -> typename std::is_same<ClassT&, decltype( std::declval<T>() >> std::declval<OtherT>() )>::type;

		template<typename>
		static constexpr auto test(...) -> std::false_type;

	public:
		static constexpr bool value = decltype(test<ClassT>(0))::value;
	};


	namespace experimental
	{
		template<typename Tuple, typename Indices = std::make_index_sequence<std::tuple_size<Tuple>::value>>
		struct runtime_get_func_table;

		template<typename Tuple, size_t ... Indices>
		struct runtime_get_func_table<Tuple,std::index_sequence<Indices...>>
		{
			using return_type	=	typename std::tuple_element<0,Tuple>::type&;
			using get_func_ptr	=	return_type (*)(Tuple&) noexcept;

			static constexpr get_func_ptr table[std::tuple_size<Tuple>::value] = {
				&std::get<Indices>...
			};
		};

		template<typename Tuple, size_t ... Indices>
		constexpr typename
		runtime_get_func_table<Tuple, std::index_sequence<Indices...>>::get_func_ptr
		runtime_get_func_table<Tuple, std::index_sequence<Indices...>>::table[std::tuple_size<Tuple>::value];


		template<typename Tuple>
		constexpr typename std::tuple_element<0, typename std::remove_reference<Tuple>::type>::type&
		runtime_get(Tuple&& t, size_t index)
		{
			using tuple_type = typename std::remove_reference<Tuple>::type;

			if(index < std::tuple_size<tuple_type>::value) return runtime_get_func_table<tuple_type>::table[index](t);
			throw std::runtime_error("Out of range");
		}
	}
}

namespace dpl
{
	// WARNING: Fails with multiple inheritance .
	template < template <typename...> class base,typename derived>
	struct	is_base_of_template_impl
	{
		template<typename... Ts>
		static constexpr std::true_type  test(const base<Ts...> *);
		static constexpr std::false_type test(...);
		using type = decltype(test(std::declval<derived*>()));
	};

	// WARNING: Fails with multiple inheritance.
	template < template <typename...> class base,typename derived>
	using	is_base_of_template_t = typename is_base_of_template_impl<base, derived>::type;

	// WARNING: Fails with multiple inheritance.
	template < template <typename...> class base, typename derived>
	concept is_base_of_template = is_base_of_template_t<base, derived>::value;
}

// concepts
namespace dpl
{
	template<typename TList>
	concept is_TypeList					= dpl::is_specialization_v<TList, dpl::TypeList>;

	template<template<typename...> class Ref, typename... Ts>
	concept all_types_specialized		= (dpl::is_specialization_v<Ts, Ref> && ...);

	template<typename... Ts>
	concept all_types_unique			= dpl::TypeList<Ts...>::ALL_UNIQUE;

	template<typename T, typename TYPE_LIST>
	concept is_one_of					= dpl::is_TypeList<TYPE_LIST> && TYPE_LIST::template has_type<T>();

	template<typename T>
	concept is_type_complete			= dpl::is_type_complete_v<T>;

	template<typename T, template<typename...> class Ref>
	concept is_specialization			= IsSpecialization<T, Ref>::value;

	template<typename ClassT, typename OtherT> // OBSOLETE: Now part of the std lib.
	concept has_assignment_operator		= HasAssignmentOperator<ClassT, OtherT>::value;

	template<typename ClassT, typename OtherT>
	concept has_shift_left_operator		= HasShiftLeftOperator<ClassT, OtherT>::value;

	template<typename ClassT, typename OtherT>
	concept has_shift_right_operator	= HasShiftRightOperator<ClassT, OtherT>::value;

	template<typename ClassT, typename OtherT>
	concept has_bidirectional_shift		= has_shift_left_operator<ClassT, OtherT> && has_shift_right_operator<ClassT, OtherT>;
}