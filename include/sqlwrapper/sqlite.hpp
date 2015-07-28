//============================================================================
// Name        : sqlite_wrapper
// Author      : Pierre BLAVY
// Version     : 1.0
// Copyright   : LGPL 3.0+ : https://www.gnu.org/licenses/lgpl.txt
// Description : A template sqlite wrapper
// Link with   : libsqlite3
// Include     : <sqlite3.h> and <tuple_tools/tuple_apply.hpp>
// Context     : This file is a specialization of the empty class Db_sqlite
//               Db_sqlite is planned to be a unified interface for various DB.
//               but this job is not done yet.
// Doc         : https://www.sqlite.org/cintro.html
//               http://www.dreamincode.net/forums/topic/122300-sqlite-in-c/
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

#ifndef SQLWRAPPER_SQLITE_HPP_
#define SQLWRAPPER_SQLITE_HPP_

//Internal dependancies
#include "Db_base.hpp"

// External dependancies
#include <tuple_tools/tuple_apply.hpp>
#include <tuple_tools/tuple_function.hpp>
#include <unicont/deque.hpp>
#include <unicont/vector.hpp>

//use sqlite3 as DB backend
#include <sqlite3.h>
//#include <time_tools/Time.hpp>

//standard includes
#include <cassert>
#include <mutex>  //Rowid_t is bullshit if db is not locked, and mutex prevent recursive request
#include <atomic> //for savepoints ids
#include <tuple>
#include <fstream>



namespace sqlwrapper{

	struct Sqlite_tag{}; //use this struct to identify stuff related to sqlite

	template<>
	struct Sql<Sqlite_tag>{typedef std::string type;};

	template<>
	struct Rowid<Sqlite_tag>{typedef decltype(sqlite3_last_insert_rowid(nullptr )) type;};


	template<>
	struct DbConnectInfo<Sqlite_tag>{
		explicit DbConnectInfo(const std::string & s):filepath(s){}
		const std::string filepath;

		template<typename T>
		struct Limit_t{
			Limit_t( int sqlite_id_, const T& default_value_, const T &min_value_, const T &max_value_):
				is_set(false), min_value(min_value_), max_value(max_value_),
				default_value(default_value_), current_value(default_value_),
				sqlite_id(sqlite_id_)
			{
				assert(min_value<=max_value);
				assert(default_value>=min_value);
				assert(default_value<=max_value);
			}

			void set(const T&t){
				assert(t>=min_value);
				assert(t<=max_value);
				current_value=t;
				is_set=true;
			}

			bool is_set;
			const T min_value;
			const T max_value;
			const T default_value;
			T current_value;
			const int sqlite_id;
		};

		//sqlite limits:  https://www.sqlite.org/limits.html
		//first   = is set
		//sercond = value

		typedef Limit_t<int> Limit;
		Limit max_length={SQLITE_LIMIT_LENGTH     ,1000000000,1,2147483647}; //Maximum length of a string or BLOB
		Limit max_column             ={SQLITE_LIMIT_COLUMN     ,2000      ,1,32767}; //Maximum Number Of Columns
		Limit max_sql_length         ={SQLITE_LIMIT_LENGTH     ,1000000   ,1,1073741824};  //Maximum Length Of An SQL Statement
		//static const int    max_table_join         = 64; //Maximum Number Of Tables In A Join : cannot me changed
		Limit max_expr_depth         ={SQLITE_LIMIT_EXPR_DEPTH  ,1000,0,std::numeric_limits<int>::max()-1};   //Maximum Depth Of An Expression Tree
		Limit max_function_arg       ={SQLITE_LIMIT_FUNCTION_ARG,100,1,127}; //Maximum Number Of Arguments On A Function
		Limit max_compound_select    ={SQLITE_LIMIT_COMPOUND_SELECT,500,1,500}; //Maximum Number Of Terms In A Compound SELECT Statement
		Limit max_like_pattern_length={SQLITE_LIMIT_LIKE_PATTERN_LENGTH,50000,1,50000};//Maximum Length Of A LIKE Or GLOB Pattern
		Limit max_variable_number    ={SQLITE_LIMIT_VARIABLE_NUMBER,999,0,999};//Maximum Number Of Host Parameters In A Single SQL Statement
		Limit max_trigger_depth      ={SQLITE_LIMIT_TRIGGER_DEPTH,1000,1,std::numeric_limits<int>::max()-1};//Maximum Depth Of Trigger Recursion
		Limit max_attached           ={SQLITE_LIMIT_ATTACHED,10,1,25};//Maximum Number Of Attached Databases
		//Limit max_page_count         ={SQLITE_LIMIT_PAGE_COUNT,1073741823,1,2147483646};//Maximum Number Of Pages In A Database File
		//static const auto max_numrow = 18446744073709551616 ;//Maximum Number Of Rows In A Table
		//Maximum Database Size : unused too big for existing hardware
		//Maximum Number Of Tables In A Schema : unused idem


	};



	template<>
	struct DbManager<Sqlite_tag>{
		SQLWRAPPER_TYPES(Sqlite_tag);

		friend Query_t;
		friend DbSavepoint_t;

		explicit DbManager(const DbConnectInfo_t &c);
		DbManager(DbManager_t &&move_me);

		~DbManager();


		//typedef time_tools::Time Time;

		//type of optional value (i.e., values that can be null)
		template<typename T> struct Optional;  	//todo c++14, use std::optional instead

		//make a query
		Query_t prepare(const Sql_t &sql);
		void    prepare(Query_t &target, const Sql_t &sql);

		//make a transaction. Don't forget to create a local transaction object
		DbTransaction_t transaction();
		DbSavepoint_t   savepoint();


		//execute is a general function for sql when we do not care about returned data or Rowid_t, the most performant one
		//data is automatically bound to the query (if data not empty)
		                            void execute(const std::string &); //execute (multiple) queries
		template<typename... data>	void execute(Query_t &query   , data...); //prepare and execute query
		template<typename... data>	void execute(const Sql_t & sql, data...d){Query_t q=prepare(sql); this->execute(q,d...);}//execute a prepared query

		//insert a line, reuturn its Rowid_t
		template<typename... data>	Rowid_t insertRow (Query_t &query  , data...);
		template<typename... data>	Rowid_t insertRow (const Sql_t &sql, data... d){Query_t q = prepare(sql); return this->insertRow(q,d...);}

		//idem for a tuple
		template< typename ...Args > void insertTuple(Query_t &query  , const std::tuple<Args...>  &tuple);
		template< typename ...Args > void insertTuple(const Sql_t &sql, const std::tuple<Args...>  &tuple){Query_t q = prepare(sql); this->insertTuple(q,tuple);}

		//idem for a single column
		template<typename Container> void insertColumn(Query_t &query  , const Container &c){for( const auto &i : c){execute(query,i);}}
		template<typename Container> void insertColumn(const Sql_t &sql, const Container &c){Query_t q=prepare(sql); this->insertColumn(q,c);}

		//idem but insert stuff from a container of tuple
		template< template <typename...> class Cont, typename ...Args >
		void insertTable(Query_t &query, const Cont<std::tuple<Args...> >  &data_container);

		template< template <typename...> class Cont, typename ...Args >
		void insertTable(const Sql_t &sql, const Cont<std::tuple<Args...> >  &data_container){Query_t q = prepare(sql); this->insertTable(q,data_container);}


		///get a single unique line
		///set the Args... to get values
		///    note : use Optional<T> to handle possibly null values
		//throw if the request do not exactly return 1 row
		template<typename ...Args >	void  getRow (Query_t &query  , Args &... set_this_values);
		template<typename ...Args >	void  getRow (const Sql_t &sql, Args &... set_this_values){Query_t q = prepare(sql); this->getRow(q,set_this_values...);}

		//idem, but with a line that can be missing. Returns false and do not change values if line is empty
		template<typename ...Args >	bool  getRow_optional (Query_t &query  , Args &... set_this_values);
		template<typename ...Args >	bool  getRow_optional (const Sql_t &sql, Args &... set_this_values){Query_t q = prepare(sql); return this->getRow_optional(q,set_this_values...);}


		//idem, for a tuple
		template< typename ...Args > std::tuple<Args...>  getTuple (Query_t &query)   {std::tuple<Args...> R;getTuple(query,R);return R;}
		template< typename ...Args > std::tuple<Args...>  getTuple (const Sql_t & sql){Query_t q = prepare(sql);return this->getTuple<Args...>(q);}

		template< typename ...Args > void getTuple (Query_t &query   , std::tuple<Args...> &t);
		template< typename ...Args > void getTuple (const Sql_t & sql, std::tuple<Args...> &t){Query_t q = prepare(sql);return this->getTuple(q,t);}



		//return a query result, stored as tuples in a container
		//to use a custom container, please specify unicont::reserve_t and unicont::move_in_t
		template< template <typename...> class Cont, typename ...Args >
		void getTable (Query_t &query,Cont<std::tuple<Args...> > &target);

		template< template <typename...> class Cont, typename ...Args >
		void getTable (const Sql_t &sql,Cont<std::tuple<Args...> > &target){Query_t q = prepare(sql); this->getTable(q, target);}

		template< template <typename...> class Cont, typename ...Args >
		Cont<std::tuple<Args...>> getTable(Query_t &query){Cont<std::tuple<Args...>> R; getTable(query,R); return R;}

		template< template <typename...> class Cont, typename ...Args>
		Cont<std::tuple<Args...>> getTable(const Sql_t &sql){Query_t q = prepare(sql); return this->getTable<Cont,Args...>(q);}



		//append a single column in a container
		template<typename Cont, typename ... Args>
		void getColumn(Query_t &q, Cont &append_here, Args ... bind_me );

		template<typename Cont, typename ... Args>
		void getColumn(const std::string &sql, Cont &append_here, Args ... bind_me ){Query_t q = prepare(sql); this->getColumn(q,append_here,bind_me...);}

		//idem but returns a container
		template<template<typename, typename...> class Cont, typename T,  typename ... Args>
		Cont<T> getColumn(Query_t &q, Args ... bind_me ){Cont<T> cont;this->getColumn(q,cont,bind_me...);return cont;}

		template<template<typename, typename...> class Cont, typename T,  typename ... Args>
		Cont<T> getColumn(const std::string &sql, Args ... bind_me ){Query_t q = prepare(sql); return this->getColumn<Cont,T,Args...>(q,bind_me...);}


		//getApply applies a function line by line
		//if Fn returns a bool, the function is applied while Fn returns true. It returns true if Fn returns true for each line, else it returns false
		//if Fn returns void, the function is applied to each line.
		template<typename Fn> auto getApply(Query_t &query  , Fn fn)-> typename tuple_tools::return_type<Fn>::type;
		template<typename Fn> auto getApply(const Sql_t &sql, Fn fn)-> typename tuple_tools::return_type<Fn>::type;


		//applied F in parallel
		//WARNING stuff is popped into a queue without size limit, so the full table may be popped into memory
		template<typename Fn> auto getApply_parallel(Query_t &query  , Fn fn,const size_t cache_size=128)-> void;
		template<typename Fn> auto getApply_parallel(const Sql_t &sql, Fn fn,const size_t cache_size=128)-> void;


		//Read a sql file, and execute it.
		void read(const std::string &path);
		void read(std::istream &in);


		//Get informations about columns


		typedef Column_info<Sqlite_tag> Column_info_t;

		template<template<typename, typename...> class Cont, typename ... Args>
		Cont<Column_info_t>  getColumn_info(Query_t &q, Args ... bind_me );

		template<template<typename, typename...> class Cont, typename ... Args>
		Cont<Column_info_t> getColumn_info(const std::string &sql, Args ... bind_me ){Query_t q = prepare(sql); return this->getColumn_info<Cont,Args...>(q,bind_me...);}





		private:
		struct Query_guard;
		//struct Tuple_bind_r;
		//struct Tuple_setFrom_r;


		//RAII helper to be sure to reset a query after using it.
		struct Query_guard{
			explicit Query_guard(Query_t &q):query(q){}
			~Query_guard();
			private:
			Query_t &query;
		};


		//tuple helpers
		struct Tuple_bind_r{
			explicit Tuple_bind_r(Query_t &q):query(q){}
			template<typename T>
			void run(const T &t,const size_t I);
			Query_t & query;
		};

		struct Tuple_setFrom_r{
			explicit Tuple_setFrom_r(Query_t &q):query(q){}
			template<typename T>
			void run(T &t ,const size_t I);
			Query_t &query;
		};

		//apply helpers
		template<typename T> struct GetData_apply;

		template<typename... Args>
		struct GetData_apply<std::tuple<Args...> >{
			GetData_apply(Query_t &q);
			std::tuple<Args...> get_data();//if have data, get it!
			bool have_data()const;

			private:
			mutable int querry_result;                 //initialize on first use
			Tuple_setFrom_r fn;
			bool have_data_b=true;
			Query_t &query;
		};



		//apply an function object to each row.
		//the function object must return true to continue fetching data
		//if false is returned, fetching data stops and false is returned
		template<typename Fn> bool getApply_bool(Query_t &query  , Fn fn);
		template<typename Fn> bool getApply_bool(const Sql_t &sql, Fn fn){Query_t q=prepare(sql); return this->getApply_bool<Fn>(q,fn);}

		//idem but with a function that returns void
		//the function is applyed anyway
		template<typename Fn> void getApply_void(Query_t &query  , Fn fn);
		template<typename Fn> void getApply_void(const Sql_t &sql, Fn fn){Query_t q=prepare(sql); this->getApply_void<Fn>(q,fn);}

		template<bool return_bool>        struct getApply_dispatch;
		template<bool return_bool> friend class  getApply_dispatch;

		//Savepoint stuff
		typedef Sql_t Savepoint_id_t;
		Savepoint_id_t savepoint_newid(){return "s"+std::to_string(savepoint_id++);}

		private:
		sqlite3 *db;
		std::mutex db_mutex;
		std::atomic<size_t> savepoint_id;
	};











	//---- Query ---

	template<>
	struct Query<Sqlite_tag>{
		SQLWRAPPER_TYPES(Sqlite_tag);

		 Query()=default;
		 Query(Query_t &&q);
		~Query();

		Query(const Query_t &source)      =delete;
		Query_t & operator=(const Query_t &)=delete;

		const Sql_t sql()const;

		template<typename... Data> void bind(Data... data);

		template<typename... Data> void setFrom(Data&... data);

		template<typename T, typename... Args>
		void reset_binding(const T &t, Args... args); //reset binding, then bind args
		void reset_binding();                         //reset binding, then bind nothing




	private:
		sqlite3_stmt *statment=nullptr;
		size_t nb_bind=0;

		void clear();

		//handle varidaic args recursively
		//doc : http://nerdparadise.com/forum/openmic/5712/
		template<typename T, typename... Args>
		void bind_r( size_t i, const T&t, Args... rest);
		void bind_r( size_t i){}

		template<typename T, typename... Args>
		void extract_r(size_t index,  T&t, Args&... rest);
		void extract_r(const size_t){}

		//friends
		template<typename T,typename U> friend class ::sqlwrapper::DbExtract_t;
		template<typename T,typename U> friend class ::sqlwrapper::DbBind_t;

		friend DbManager<Sqlite_tag>::Tuple_bind_r;
		friend DbManager<Sqlite_tag>::Tuple_setFrom_r;

		friend DbManager<Sqlite_tag>::Query_guard;
		friend DbManager<Sqlite_tag>;

	};//end Query




	//---- DbTransaction ---
	template<>
	struct DbTransaction<Sqlite_tag>{
		explicit DbTransaction(DbManager<Sqlite_tag> &db_);
		         DbTransaction(DbTransaction<Sqlite_tag> &&s);
		         ~DbTransaction();

		//not copyable
		DbTransaction(const DbTransaction<Sqlite_tag> &)=delete;
		DbTransaction<Sqlite_tag>& operator=(const DbTransaction<Sqlite_tag> &)=delete;

		void rollback();
		void commit  ();

		private:
		bool done=false;
		DbManager<Sqlite_tag> &db;
	};




	//---- DbSavepoint ---
	template<>
	struct DbSavepoint<Sqlite_tag>{
		explicit DbSavepoint(DbManager<Sqlite_tag> &db_);
		         DbSavepoint(DbSavepoint<Sqlite_tag> &&s);
		         ~DbSavepoint();

		 //not copyable
		DbSavepoint(const DbSavepoint<Sqlite_tag> &)=delete;
		DbSavepoint<Sqlite_tag>& operator=(const DbSavepoint<Sqlite_tag> &)=delete;

		void rollback();
		void release() ;
		void commit(){release();}

		private:
		bool done=false;
		DbManager<Sqlite_tag> ::Savepoint_id_t savepoint_id;
		DbManager<Sqlite_tag>  &db;
	};





}//namespace sqlwrapper


#include <sqlwrapper/sqlite_impl/DbManager.tpp>
#include <sqlwrapper/sqlite_impl/Query.tpp>
#include <sqlwrapper/sqlite_impl/DbSavepoint.tpp>
#include <sqlwrapper/sqlite_impl/DbTransaction.tpp>
#include <sqlwrapper/sqlite_impl/Types_base.tpp>

#endif /* SQLWRAPPER_SQLITE_HPP_ */
