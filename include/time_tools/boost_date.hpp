//Specify the time_tools for boost dates


//NEVER EVER use strftime, strptime, mktime when dealing with times & durations.
//This C time functions are badly defined, badly documented
//and have the worse design possible.

//This function DO NOT WORK IN MULTITHREAD
//Their implementations relies on hidden shared objects
//(a shared locale, and shared static for data returned by functions)

//This function DO NOT WORK AT ALL
//This functions are LOCALE, which means that the same
//program will run differently from one machine to an other
//depending on the time system (i.e., time zone + DST).
//Locale is globally shared :  your program, other programs,
//the OS may change it at any time. Even if you find a way to
//switch to a well defined locale, you are never sure that
//no one change it back.

//There is no clear way to define the locale you want to use.
//LOCALES are implementation defined, without any documentation to
//explain how they are defined.

//This function DO NOT DO WHAT'S WRITTEN IN THE DOC
//The strptime function does not do what it's format documentation says
//for example the %z flag, used to specify a time system (with some obsucre
//not documented identifiers), does in reality nothing and the locale
//time system is used anyway.

//Moreover some bugs exists when converting between tm and time_t.
//This conversion may silently overflow (-->bug) or may used a wired
//time system (probably the locale one).

//Hand crafting tm objects do not work either. For example if you
//set DST to 0, mktime may set it to 1 anyway depending on your locale
//settings.


#ifndef INCLUDE_TIME_TOOLS_BOOST_DATE_HPP_
#define INCLUDE_TIME_TOOLS_BOOST_DATE_HPP_

#include "time_tools.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

#include <string>
#include <sstream>
#include <ostream>

#include "time_tools.hpp"

//doc : http://openclassrooms.com/forum/sujet/dates-temps-et-chaines-de-caracteres
//doc : http://www.boost.org/doc/libs/1_55_0/doc/html/date_time/date_time_io.html#date_time.format_flags


namespace time_tools{

	typedef boost::posix_time::ptime  Boost_date ;

	template<>
	struct iso_format<Boost_date>{
		static const std::string& run(){	static const std::string s("%Y-%m-%d %H:%M:%S"); return s;}
	};

	template<>
	struct to_string_t<Boost_date>{

		typedef boost::posix_time::ptime time_t;
		typedef boost::date_time::time_facet<time_t,char>        date_output_facet_t;
		typedef boost::date_time::time_input_facet<time_t,char>  date_input_facet_t;

		static void run(const time_t & time, const std::string & format, std::string & write_here){
			std::ostringstream ss;
			auto facet = new date_output_facet_t(format.c_str());
			ss.exceptions(std::ios_base::failbit);
			ss.imbue(std::locale(std::locale::classic(),facet));
			try{
				ss << time;
				write_here=ss.str();
			}catch(std::exception &e){
				throw TimeError_convert_to_string("cannot convert from Boost_date to string, format=" +format +" ,exception=" + e.what());
			}catch(...){
				throw TimeError_convert_to_string("cannot convert from Boost_date to string, format=" +format);
			}


			if(write_here=="not-a-date-time"){
				throw TimeError_convert_to_string("cannot convert from Boost_date to string, format=" +format );
			}
		}
	};

	template<>
	struct from_string_t<Boost_date>{
		static void run(const std::string & time_str, const std::string & format, Boost_date &write_here){
			typedef boost::posix_time::ptime time_t;
			//typedef boost::date_time::time_facet<time_t,char>        date_output_facet_t;
			typedef boost::date_time::time_input_facet<time_t,char>  date_input_facet_t;

			std::istringstream ss;
			auto facet = new date_input_facet_t(format.c_str());
			ss.exceptions(std::ios_base::failbit);
			ss.imbue(std::locale(std::locale::classic(),facet));
			ss.str(time_str);
			try{
				ss >> write_here;
			}catch(std::exception &e){
				throw TimeError_convert_from_string("cannot convert from string to Boost_date string="+time_str+", format=" +format +" ,exception=" + e.what());
			}catch(...){
				throw TimeError_convert_from_string("cannot convert from string to Boost_date string="+time_str+", format=" +format);
			}

		}
	};




}


#endif /* INCLUDE_TIME_TOOLS_BOOST_DATE_HPP_ */
