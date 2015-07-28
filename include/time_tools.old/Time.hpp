//C++ support for time and duration is akward.
//std::chrono is able to make clear and easy computations on times
//but there is no usable support for string -> date -> string conversion
//this library is an attempt to make this conversion work


//if you have linker errors, use -lboost_date_time
//but for now the code seems to work without this dependancy



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



#ifndef TIME_HPP_
#define TIME_HPP_


#define TIME_CHRONO_SUPPORT
//#include <boost/date_time/gregorian/gregorian.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <string>
#include <sstream>
#include <ostream>


#ifdef TIME_CHRONO_SUPPORT
  #include <chrono>
#endif

namespace time_tools{


struct Time_error      :std::runtime_error{  typedef std::runtime_error base_t; using base_t::base_t;};
struct TimeFormat_error:Time_error        {typedef Time_error base_t          ; using base_t::base_t;};

#ifdef TIME_CHRONO_SUPPORT
  template<typename Source, typename Target>
  struct convert_to;
#endif

struct Time{

	typedef boost::posix_time::ptime       date_t;
	typedef decltype(date_t() - date_t() ) duration_t;


             Time()=default;
	explicit Time(const std::string &s)                      {from_string(s,default_timeFormat());}
	explicit Time(const std::string &s, const std::string &f){from_string(s,f);}
	explicit Time(const date_t &d):data(d){}
	//explicit Time(const time_point &p):data(p){}

	std::string to_string()                    const{return to_string(default_timeFormat());}
	std::string to_string(const std::string &f)const;
	date_t      to_date()const{return data;}

	      date_t & date(){return data;}
	const date_t & date()const{return data;}


	void from_string(const std::string &s){from_string(s,default_timeFormat());};
	void from_string(const std::string &s, const std::string &);
	void from_date  (const date_t & d){data=d;}

	template<typename T> static duration_t seconds(const T&t);
	template<typename T> static duration_t minutes(const T&t);
	template<typename T> static duration_t hours  (const T&t);
	template<typename T> static duration_t hms    (const T&h, const T&m,const T&s){return hours(h)+minutes(m)+seconds(s);}

	#ifdef TIME_CHRONO_SUPPORT
		template < class Clock, class Duration>
		using time_point = std::chrono::time_point<Clock, Duration>;

		template < class Clock, class Duration>
		std::chrono::time_point<Clock, Duration> to_chrono()const{return convert_to<time_point<Clock, Duration>, date_t>::apply(data);}

		template < class Clock, class Duration>
		void from_chrono(const time_point<Clock, Duration> &t){data=convert_to< date_t, time_point<Clock, Duration> >::apply(t);}

		template < class Clock, class Duration>
		explicit Time(const time_point<Clock, Duration> &d){from_chrono(d);}

		//default : use system_clock
		typedef std::chrono::system_clock::time_point system_time_point;
		typedef std::chrono::system_clock::duration   system_duration;
		typedef std::chrono::system_clock             system_clock;
		system_time_point to_chrono()const {return to_chrono<system_clock,system_duration>();}
	#else
		template < class T, class U> void to_chrono()            =delete;//Unimplemented, please add #define TIME_CHRONO_SUPPORT
		template < class T, class U> void from_chrono(const T &t)=delete;//Unimplemented, please add #define TIME_CHRONO_SUPPORT
	#endif


	static const std::string& default_timeFormat(){	static const std::string s("%Y-%m-%d %H:%M:%S"); return s;}
	static const std::string& implementation_id (){	static const std::string s("boost::posix_time"); return s;}



private:
	typedef boost::date_time::time_facet<date_t,char>        date_output_facet_t;
	typedef boost::date_time::time_input_facet<date_t,char>  date_input_facet_t;
	date_t data;
};

inline std::ostream& operator<<(std::ostream &out, const Time &t){out << t.to_string(); return out;}


//---Time implementation---

	inline void Time::from_string(const std::string &s, const std::string &format){

		std::istringstream ss(s);
		auto facet = new date_input_facet_t(format.c_str());
		ss.exceptions(std::ios_base::failbit);
		ss.imbue(std::locale(std::locale::classic(),facet));
		ss >> data;

	}


	inline std::string Time::to_string(const std::string &format)const {
		std::ostringstream ss;
		auto facet = new date_output_facet_t(format.c_str());
		ss.exceptions(std::ios_base::failbit);
		ss.imbue(std::locale(std::locale::classic(),facet));
		ss << data;
		return ss.str();
	}


	template<typename T>
	auto Time::seconds(const T&t)->duration_t{
		static_assert(std::is_integral<T>::value,"Error in time::seconds(t), t must be an integral type");
		return boost::posix_time::seconds(t);
	}

	template<typename T>
	auto Time::minutes(const T&t)->duration_t{
		static_assert(std::is_integral<T>::value,"Error in time::minutes(t), t must be an integral type");
		return boost::posix_time::minutes(t);
	}


	template<typename T>
	auto Time::hours(const T&t)->duration_t{
		static_assert(std::is_integral<T>::value,"Error in time::hours(t), t must be an integral type");
		return boost::posix_time::hours(t);
	}


	//--- from/to chrono ---
	//publicly available code : https://stackoverflow.com/questions/4910373/interoperability-between-boostdate-time-and-stdchrono

#ifdef TIME_CHRONO_SUPPORT
	template < class Clock, class Duration>
	struct convert_to<boost::posix_time::ptime, std::chrono::time_point<Clock, Duration> > {
        //typedef std::chrono::time_point<Clock, Duration> time_point_t;

	    inline static boost::posix_time::ptime apply(const std::chrono::time_point<Clock, Duration>& from)
	    {
	    	using namespace boost;
	    	using namespace std;
	        typedef chrono::nanoseconds duration_t;
	        typedef duration_t::rep rep_t;
	        rep_t d = chrono::duration_cast<duration_t>(from.time_since_epoch()).count();
	        rep_t sec = d/1000000000;
	        rep_t nsec = d%1000000000;
	        return boost::posix_time::from_time_t(0)+
	        boost::posix_time::seconds(static_cast<long>(sec))+
	        #ifdef BOOST_DATE_TIME_HAS_NANOSECONDS
	        boost::posix_time::nanoseconds(nsec);
	        #else
	        boost::posix_time::microseconds((nsec+500)/1000);
	        #endif
	    }
	};

	template < class Clock, class Duration>
	struct convert_to<std::chrono::time_point<Clock, Duration>, boost::posix_time::ptime> {
	    inline static std::chrono::time_point<Clock, Duration> apply(const boost::posix_time::ptime& from)
	    {
	    	using namespace boost;
	        using namespace std;
	    	boost::posix_time::time_duration const time_since_epoch=from-boost::posix_time::from_time_t(0);
	        chrono::time_point<Clock, Duration>    t=chrono::system_clock::from_time_t(time_since_epoch.total_seconds());
	        long nsec=time_since_epoch.fractional_seconds()*(1000000000/time_since_epoch.ticks_per_second());
	        return t+chrono::nanoseconds(nsec);

	    }
	};
#endif

}



#endif /* TIME_HPP_ */
