//============================================================================
// Name        : tuple_function
// Author      : Pierre BLAVY
//             : code written from publically available code
//             : https://stackoverflow.com/questions/7858817/unpacking-a-tuple-to-call-a-matching-function-pointer
// Version     : 1.0
// Copyright   : LGPL 3.0+ : https://www.gnu.org/licenses/lgpl.txt
// Description : tuple_arguments<Fn>::type extract Fn arguments into a tuple
//               return_type<Fn>::type     is the type returned by Fn
//               tuple_function(fn, t)     calls the function fn by passing tuple t content as function arguments.
//============================================================================

/*
This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see
    <https://www.gnu.org/licenses/lgpl-3.0.en.html>.
*/


//interface
//template<typename Fn, typename Tuple>
//auto tuple_function(Fn fn, Tuple &t)-> return type of Fn;



#ifndef TUPLE_FUNCTION_HPP_
#define TUPLE_FUNCTION_HPP_




#include <utility>
#include <type_traits>
#include <tuple>


namespace tuple_tools{


//https://stackoverflow.com/questions/7858817/unpacking-a-tuple-to-call-a-matching-function-pointer
namespace tuple_function_impl{
	template<int ...>struct seq { };

	template<int N, int ...S>
	struct gens : gens<N-1, N-1, S...> { };

	template<int ...S>
	struct gens<0, S...> {
	  typedef seq<S...> type;
	};

	template<typename F,  typename Tuple, int ...S>
	auto callFunc(F func, Tuple &t,  seq<S...>)
	->decltype(func(std::get<S>(t) ...)){
		return func(std::get<S>(t) ...);
	}


	template<class R, class C, class... Args>
	std::tuple<Args...> param_objet(R (C::*)(Args...));

	template<class R, class C, class... Args>
	std::tuple<Args...> param_objet(R (C::*)(Args...) const);
}



template<class T>
struct tuple_arguments
{ typedef decltype(tuple_function_impl::param_objet(&T::operator())) type; };

template<class R, class... Args>
struct tuple_arguments<R(*)(Args...)>
{ typedef std::tuple<Args...> type; };

template<class R, class... Args>
struct tuple_arguments<R(Args...)>
{ typedef std::tuple<Args...> type; };



//call a function from a tuple
//pass tuple elements as function argument
template<typename Fn, typename Tuple>
auto tuple_function(Fn fn, Tuple &t)
-> decltype(
		tuple_function_impl::callFunc(
			fn,
			t,
			typename tuple_function_impl::gens<  std::tuple_size<Tuple>::value >::type()  )
		)
{
	using namespace tuple_function_impl;
	const size_t size = std::tuple_size<Tuple>::value;
	typedef typename gens< size >::type seq_t;
    return callFunc(fn, t, seq_t() );
}


//easily get the return type of a callable
template<typename Fn>
struct return_type{
	typedef typename tuple_arguments<Fn>::type tuple_arguments;
	typedef tuple_arguments* tuple_arguments_ptr;
	typedef decltype(tuple_function(std::declval<Fn>() , *static_cast<tuple_arguments_ptr>(nullptr) ) )type;
};


template<class R, class... Args>
struct return_type<R(*)(Args...)>
{ typedef R type; };


template<class R, class... Args>
struct return_type<R(Args...)>
{ typedef R type; };










namespace tuple_function_impl{

	class Check_tuple_arguments{
	static void check(){
		static_assert(std::is_same< tuple_arguments<void(int)  >::type , std::tuple<int> >::value,"int");
		static_assert(std::is_same< tuple_arguments<void(int*) >::type , std::tuple<int*>>::value,"int*");
		static_assert(std::is_same< tuple_arguments<void(int&) >::type , std::tuple<int&>>::value,"int&");
		static_assert(std::is_same< tuple_arguments<void(int&&)>::type , std::tuple<int&&>>::value,"int&&");

		//static_assert(std::is_same< tuple_arguments<void(const int)  >::type , std::tuple<const int> >::value,"const int"); //fail because  void f(const int) is the same as void f(int)
		static_assert(std::is_same< tuple_arguments<void(const int*) >::type , std::tuple<const int*>>::value,"const int*");
		static_assert(std::is_same< tuple_arguments<void(const int&) >::type , std::tuple<const int&>>::value,"const int&");
		static_assert(std::is_same< tuple_arguments<void(const int&&)>::type , std::tuple<const int&&>>::value,"const int&&");
	}
	};

	class Check_return_type{
		static bool fn_bool(int){return true;}
		struct Functor_int{int operator()(int){return 15;}};

		static void check(){
			static_assert(std::is_same<return_type< int&(int,double)>::type,int&>::value, "const int(int,double)"  );

			auto l=[&](int)->double{return 42.0;};
			typedef decltype(l) l_type;
			static_assert(std::is_same<return_type<l_type>::type,double>::value, "lambda"  );

			static_assert(std::is_same<return_type<decltype(fn_bool)>::type,bool>::value, "function"  );
			static_assert(std::is_same<return_type<Functor_int>::type,int>::value, "Functor"  );
		}
	};

}

}//end namespace tuple_tools


#endif /* TUPLE_FUNCTION_HPP_ */
