//This file define a generic way to handle Times

#ifndef INCLUDE_TIME_TOOLS_TIME_TOOLS_HPP_
#define INCLUDE_TIME_TOOLS_TIME_TOOLS_HPP_

#include <string>
#include <chrono>
#include <stdexcept>


#ifndef MK_EXCEPTION
#define MK_EXCEPTION_BASE(name)\
	struct name : virtual std::exception{\
    std::string msg;\
    explicit name(const char *s):msg(s){}\
    explicit name(const std::string &s):msg(s){}\
    const char*what() const throw() override{return msg.c_str();}\
	protected:\
	name(){}\
};

#define MK_EXCEPTION(name,base)\
	struct name : virtual base{\
		typedef base base_t;\
		name(const std::string &s){msg=s;}\
		name(const char *s){msg=s;}\
		const char*what() const throw() override{return msg.c_str();}\
		protected:\
		name(){}\
	};
#endif


namespace time_tools{

	MK_EXCEPTION_BASE( TimeError );
	MK_EXCEPTION (TimeError_convert ,TimeError );
	MK_EXCEPTION (TimeError_convert_from_string ,TimeError_convert );
	MK_EXCEPTION (TimeError_convert_to_string   ,TimeError_convert );

    template<typename Time_t>
	struct iso_format;
    //require static const std::string & run();
	//The default format must be the ISO time format :
    //    yyyy-mm-dd hh:mm:ss
    //or  yyyy-mm-dd hh:mm:ss:frac_sec


    //to_string_t
	template<typename Time_t>
	struct to_string_t;//unimplemented
	//require
	//static void run(const Time_t & time, const std::string & format, std::string & write_here);

	template<typename Time_t>
	inline void to_string(const Time_t & time, const std::string & format, std::string & write_here){to_string_t<Time_t>::run(time,format,write_here);}

	template<typename Time_t>
	inline std::string to_string(const Time_t & time, const std::string & format){std::string R; to_string_t<Time_t>::run(time,format,R); return R;}

	//template<typename Time_t>
	//inline std::string to_string(const Time_t & time){std::string R; to_string_t<Time_t>::run(time,write_here,iso_format<Time_t>::run());}




	//from_string
	template<typename Time_t>
	struct from_string_t;//unimplemented
	//require
	//static void run(const std::string & time_str, const std::string & format, Time_t &write_here);

	template<typename Time_t>
	inline  void  from_string(const std::string & time_str, const std::string & format, Time_t &write_here){from_string_t<Time_t>::run(time_str,format,write_here);}

	template<typename Time_t>
	inline void  from_string(const std::string & time_str, Time_t &write_here){from_string_t<Time_t>::run(time_str,iso_format<Time_t>::run(),write_here);}

	template<typename Time_t>
	inline Time_t from_string(const std::string & time_str, const std::string & format){Time_t R; from_string_t<Time_t>::run(time_str,format,R); return R;}

	template<typename Time_t>
	inline Time_t from_string(const std::string & time_str){Time_t R; from_string_t<Time_t>::run(time_str,iso_format<Time_t>::run(),R); return R;}



	//todo
	//to_chrono
	//template<typename Time_t, >
	//struct to_chrono_t;//unimplemented
	//require
	//void run(const Time_t & time, std::


	//todo
	//mk_duration
	//seconds()
	//hours...

}







#endif /* INCLUDE_TIME_TOOLS_TIME_TOOLS_HPP_ */
