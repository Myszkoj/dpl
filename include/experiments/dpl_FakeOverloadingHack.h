#pragma once


#include <type_traits>
#include <string>


// using-overloading hack example
namespace dpl
{
	template<typename T>
	concept is_Real		= std::is_floating_point_v<T>;

	template<typename T>
	concept is_Integer	= std::is_integral_v<T>;

	template<typename T>
	concept not_Numeric = !is_Real<T> && !is_Integer<T>;


	struct RealHandler
	{
		template<is_Real T>
		void do_something()
		{

		}
	};

	struct IntegerHandler
	{
		template<is_Integer T>
		void do_something()
		{

		}
	};

	struct NotNumericHandler
	{
		template<not_Numeric T>
		void do_something()
		{

		}
	};


	struct AnyHandler	: private RealHandler
						, private IntegerHandler
						, private NotNumericHandler
	{
		using RealHandler::do_something;
		using IntegerHandler::do_something;
		using NotNumericHandler::do_something;
	};

	struct MyBase : public RealHandler
	{

	};

	struct MyDerived	: private MyBase
						, public IntegerHandler
	{
		using MyBase::do_something;
		using IntegerHandler::do_something;
	};

	struct MyDerived2	: private MyDerived
						, public NotNumericHandler
	{
		using MyDerived::do_something;
		using NotNumericHandler::do_something;
	};


	void test_fake_overloading()
	{
		AnyHandler	any;
					any.do_something<float>();
					any.do_something<int>();
					any.do_something<std::string>();

		MyDerived2	obj;
					obj.do_something<float>();
					obj.do_something<int>();
					obj.do_something<std::string>();
	}
}