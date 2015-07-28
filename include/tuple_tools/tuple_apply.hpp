//============================================================================
// Name        : tuple_apply
// Author      : Pierre BLAVY
// Version     : 1.0
// Copyright   : LGPL 3.0+ : https://www.gnu.org/licenses/lgpl.txt
// Description : Apply (a template) function to each element of a tuple
// Context     : Stand alone library
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


#ifndef TUPLE_TOOLS_HPP_
#define TUPLE_TOOLS_HPP_
#include <tuple>

///\param t a tuple
///\param fn
///fn is a class with a familly of void run(something) functions
///run can be a template function, or a set of overload
///run is applied to each member of the tuple.
///\brief Apply the fn.run(xx,size_t i) to each element xx of index i of a tuple
///\code{.cpp}
///struct Fn_example{
///	  template<typename T>
///	  void run(const T &t,const size_t i)     {std::cout << i << ":" << t << "\n";}
///};
///
///void example(){
///  auto t=std::make_tuple(0,1.0,"two");
///  Fn_example e;
///  tuple_apply(t,e);
///}

template< class Tuple, class Fn_t> void tuple_apply(      Tuple &t, Fn_t & fn);
//template< class Tuple, class Fn_t> void tuple_apply(const Tuple &t, Fn_t & fn);


//--- Implementation ---


/*Implementation details for fill_tuple*/
namespace tuple_apply_impl{

    //stuff to handle template recursion
    //counter is initialized at tuple_size
    //get use therfore counter -1
    //when counter==0 : do nothing

	template<std::size_t> struct static_counter{};

	template<typename Tuple, std::size_t I, typename Data_t>
	static void process_tuple(Tuple & t, static_counter<I>, Data_t & dat){
		process_tuple(t, static_counter<I-1>(),dat);
		dat.run ( std::get<I-1>(t) , I-1);
	}

	template<typename Tuple, typename Data_t>
	static void process_tuple(Tuple & t, static_counter<0>,  Data_t & dat){}

	//clone when tuple is const
	template<typename Tuple, std::size_t I, typename Data_t>
	static void process_tuple(const Tuple & t, static_counter<I>, Data_t & dat){
		process_tuple(t, static_counter<I-1>(),dat);
		dat.run ( std::get<I-1>(t) , I-1);
	}

	template<typename Tuple, typename Data_t>
	static void process_tuple(const Tuple & t, static_counter<0>,  Data_t & dat){}
}



template< class Tuple, class Fn_t>
void tuple_apply(Tuple &t, Fn_t & dat){
	constexpr size_t tuple_size = std::tuple_size< Tuple >::value;
	static_assert(tuple_size!=0,"logical error empty tuples must be catched by a template specialization");

	tuple_apply_impl::static_counter<tuple_size> tuple_counter; /*fill_tuple_impl::static_counter<tuple_size-1>()*/
	tuple_apply_impl::process_tuple<>(t,tuple_counter,dat );
}

//const clone
/*
template< class Tuple, class Fn_t>
void tuple_apply(const Tuple &t, Fn_t & dat){
	constexpr size_t tuple_size = std::tuple_size< Tuple >::value;
	static_assert(tuple_size!=0,"logical error empty tuples must be catched by a template specialization");
	tuple_apply_impl::static_counter<tuple_size> tuple_counter; //fill_tuple_impl::static_counter<tuple_size-1>()
	tuple_apply_impl::process_tuple<>(t,tuple_counter,dat );
}*/

//empty tuple : do nothing!
template<class Fn_t> void tuple_apply(const std::tuple<>&t, Fn_t & dat){}



#endif /* TUPLE_TOOLS_HPP_ */
