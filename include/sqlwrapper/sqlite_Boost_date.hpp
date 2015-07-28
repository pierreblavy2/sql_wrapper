//Optional support for boost_dates
#ifndef INCLUDE_SQLWRAPPER_SQLITE_BOOST_DATE_HPP_
#define INCLUDE_SQLWRAPPER_SQLITE_BOOST_DATE_HPP_


#include <sqlwrapper/sqlite.hpp>
#include <time_tools/boost_date.hpp>

namespace sqlwrapper{


template<>
struct DbExtract_t<Sqlite_tag,time_tools::Boost_date>{
	static void run(Query<Sqlite_tag> &query,size_t I,time_tools::Boost_date &t){
		std::string s;
		DbExtract_t<Sqlite_tag,std::string>::run(query,I,s);

		time_tools::from_string(
				s//time string
				,time_tools::iso_format<time_tools::Boost_date>::run() //format
				,t//write here
		);

		//try{time_tools::Time t2(s); t=t2;}
		//catch(time_tools::TimeFormat_error &e){
		//	  throw  DbError_bind(std::string("sqlite : time_error") + e.what()+" ,column="+std::to_string(I)+", sql="+query.sql());
		//}//no other exception should happen.
	}
};


template<>
struct DbBind_t<Sqlite_tag, time_tools::Boost_date >{
	static void run(Query<Sqlite_tag> &query,size_t i, const  time_tools::Boost_date &d){


		const std::string s = time_tools::to_string(
				d//time
				,time_tools::iso_format<time_tools::Boost_date>::run() //format
		);

		DbBind_t<Sqlite_tag,std::string>::run(query,i,s);

	}
};


}



#endif /* INCLUDE_SQLWRAPPER_SQLITE_IMPL_SQLITE_BOOST_DATE_HPP_ */
