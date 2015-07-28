
#include <condition_variable>
#include <atomic>
#include <thread>
#include <sstream>
#include "../mt_impl/mt_JobPool.hpp"

namespace sqlwrapper{


	namespace sqlite_impl{
		template<size_t I>	struct sqlite_coltype_t;
		template<>	struct sqlite_coltype_t<SQLITE_INTEGER>{static const std::string str(){return "integer";}};
		template<>	struct sqlite_coltype_t<SQLITE_FLOAT>  {static const std::string str(){return "float";}};
		template<>	struct sqlite_coltype_t<SQLITE_TEXT>   {static const std::string str(){return "text";}};
		template<>	struct sqlite_coltype_t<SQLITE_BLOB>   {static const std::string str(){return "blob";}};
		template<>	struct sqlite_coltype_t<SQLITE_NULL>   {static const std::string str(){return "null";}};

		inline std::string sqlite_coltype(size_t i){
			switch(i){
			case SQLITE_INTEGER : return sqlite_coltype_t<SQLITE_INTEGER>::str();
			case SQLITE_FLOAT : return sqlite_coltype_t<SQLITE_FLOAT>::str();
			case SQLITE_TEXT : return sqlite_coltype_t<SQLITE_TEXT>::str();
			case SQLITE_BLOB : return sqlite_coltype_t<SQLITE_BLOB>::str();
			case SQLITE_NULL : return sqlite_coltype_t<SQLITE_NULL>::str();
			}
			throw DbError("wrong sqlite_coltype");
		}

	}




	inline DbManager<Sqlite_tag>::DbManager(DbManager_t &&move_me){
		move_me.db_mutex.lock();
		db=move_me.db;
		move_me.db=nullptr;
		savepoint_id .store(  move_me.savepoint_id);
		move_me.db_mutex.unlock();
	}


	inline DbManager<Sqlite_tag>::~DbManager(){
		if(db==nullptr){return;}
		auto status = sqlite3_close_v2(db);
		if(status != SQLITE_OK){throw DbError("sqlite : error when closing sqlite3 connection, error=" + std::to_string(status) );}
	}

	inline DbManager<Sqlite_tag>::DbManager(const DbConnectInfo_t &d):db(nullptr),savepoint_id(0){
		int rc = sqlite3_open(d.filepath.c_str(), &db);
		if(rc!= SQLITE_OK){throw DbError_connect("sqlite : cannot init. File=" + d.filepath + ", error=" + std::to_string(rc));}

		//we expect that a database handle columns
		this->execute("PRAGMA foreign_keys = ON");
		//.mode column
		//.headers on


		//https://www.sqlite.org/c3ref/limit.html
		auto set_limit=[&](const DbConnectInfo_t::Limit&i){
			if(! i.is_set){return;}
			sqlite3_limit(db,i.sqlite_id,i.current_value);
			auto current = sqlite3_limit(db,i.sqlite_id,-1);
			assert(current==i.current_value); //this assertion fails if value is truncated
		};
		set_limit(d.max_length);
		set_limit(d.max_column);
		set_limit(d.max_sql_length);
		set_limit(d.max_expr_depth);
		set_limit(d.max_function_arg);
		set_limit(d.max_compound_select);
		set_limit(d.max_like_pattern_length);
		set_limit(d.max_variable_number);
		set_limit(d.max_trigger_depth);
		set_limit(d.max_attached);
		//set_limit(d.max_page_count);
	}

	inline auto DbManager<Sqlite_tag>::prepare(const Sql_t &sql)->Query_t{
		Query_t Query_t;
		auto status = sqlite3_prepare_v2(db, sql.c_str(), -1, &Query_t.statment, 0);
		if(status !=  SQLITE_OK){throw DbError_query("sqlite : bad Query_t : error=" + std::to_string(status)+ " Query_t=" + sql +", msg="+sqlite3_errmsg(db) );}
		return Query_t;
	}

	inline void DbManager<Sqlite_tag>::prepare(Query_t & target, const Sql_t &sql){
		target.clear();
		auto status = sqlite3_prepare_v2(db, sql.c_str(), -1, &target.statment, 0);
		if(status !=  SQLITE_OK){throw DbError_query("sqlite : bad Query_t : error=" + std::to_string(status)+ " Query_t=" + sql +", msg="+sqlite3_errmsg(db));}
	}

	inline auto DbManager<Sqlite_tag>::transaction()->DbTransaction_t{return DbTransaction_t(*this);}
	inline auto DbManager<Sqlite_tag>::savepoint()  ->DbSavepoint_t  {return DbSavepoint_t  (*this);}


	//special version : NO DATA AND string : treat string as multiple queries
	inline void DbManager<Sqlite_tag>::execute(const std::string &s){
		int querry_result =sqlite3_exec(db, s.c_str(), NULL, 0, NULL);
		if (querry_result != SQLITE_OK  ){
			throw DbError_execute(
					"sqlite : error during execute (string) "
					": querry_result=" + std::to_string(querry_result)
			        +", msg="+sqlite3_errmsg(db)
			        +", string=" + s
			);
		}
	}

	//https://www.sqlite.org/c3ref/bind_blob.html
	template<typename... Data>
	void DbManager<Sqlite_tag>::execute(Query_t &query, Data... bind_me){
		Query_guard query_guard(query);

		//bind all arguments
		query.bind(bind_me...);

		//run the Query_t
		int querry_result;
		do{
			 querry_result = sqlite3_step(query.statment);

		 }while(querry_result  == SQLITE_ROW);

		if(querry_result!=SQLITE_DONE){
			throw DbError_execute("sqlite : error during execute : querry_result=" + std::to_string(querry_result)+", sql="+query.sql()+", msg="+sqlite3_errmsg(db)
			);
		}

		//reset (by Query_t guard)

	}


	inline void DbManager<Sqlite_tag>::read(const std::string &path){
		std::ifstream file(path);
		if(!file){throw DbError_file("sqlite : error during read : cannot open,  file=" + path);}
		try{read(file);}
		catch(DbError&e){
			throw DbError_file(e.what() + std::string(",  file=") + path );
		}
	}

	inline void DbManager<Sqlite_tag>::read(std::istream &in){
		using namespace std;
		//https://stackoverflow.com/questions/116038/what-is-the-best-way-to-slurp-a-file-into-a-stdstring-in-c
		std::string buffer(static_cast<stringstream const&>(stringstream() << in.rdbuf()).str());

		try{execute(buffer);}
		catch(DbError&e){
			throw DbError_file(e.what());
		}
	}






	//Column_info
	typedef Column_info<Sqlite_tag> Column_info_t;
	template<template<typename, typename...> class Cont, typename ... Args>
	Cont<Column_info_t>   DbManager<Sqlite_tag>::getColumn_info(Query_t &query, Args ... bind_me ){
		Cont<Column_info_t> R;
		Query_guard query_guard(query);

		//bind all arguments
		query.bind(bind_me...);

		//get number of columns
		auto nb_col =  sqlite3_column_count(query.statment);

		//get columns info
		unicont::reserve(R,nb_col);
		for(int i =0; i < nb_col ; ++i){
			Column_info_t ci;
			//https://www.sqlite.org/c3ref/column_database_name.html
			ci.database_name =  sqlite3_column_database_name(query.statment,i);
			ci.table_name    =  sqlite3_column_table_name(query.statment,i);
			ci.column_name   =  sqlite3_column_origin_name(query.statment,i);
			unicont::move_in(R,std::move(ci));
		}

		return R;

	}


	//https://www.sqlite.org/c3ref/bind_blob.html
	template<typename... Data>
	auto DbManager<Sqlite_tag>::insertRow(Query_t &query, Data... data)->Rowid_t{
		Query_guard query_guard(query);

		//bind all arguments
		//assert(sizeof...(data) + Query_t.nb_bind<sqlite3_limit(db,SQLITE_LIMIT_VARIABLE_NUMBER,-1));// "ERROR : too many argument for a SQL request" );
		query.bind(data...);

		//run the Query_t
		int querry_result;
		std::unique_lock<std::mutex> db_lock(db_mutex);
		do{querry_result = sqlite3_step(query.statment);}
		while(querry_result  == SQLITE_ROW);

		//check results
		if(querry_result!=SQLITE_DONE){throw DbError_execute("sqlite : error during execute, querry_result=" + std::to_string(querry_result)+", sql="+query.sql()+", msg="+sqlite3_errmsg(db));}

		auto rowid=sqlite3_last_insert_rowid(db);
		db_lock.unlock();


		return rowid; //and reset Query_t (by query_guard)

	}



	template< typename ...Args >
	void DbManager<Sqlite_tag>::insertTuple(Query_t &query, const std::tuple<Args...>  &tuple){
		Query_guard query_guard(query);
		Tuple_bind_r fn(query);
		tuple_apply(tuple,fn);
		this->execute(query);
	}

	//idem but insert stuff from a container of tuple
	template< template <typename...> class Cont, typename ...Args >
	void DbManager<Sqlite_tag>::insertTable(Query_t &query, const Cont<std::tuple<Args...> >  &data_container){
		for(const auto & data : data_container ){insertTuple(query,data);}
	}


	template<typename...Args > //get a single line. throw if 0 or >=1 data was returned
	void DbManager<Sqlite_tag>::getRow (Query_t &query, Args &... arg){
		Query_guard query_guard(query);
		std::unique_lock<std::mutex> db_lock(db_mutex);
		//bind nothing

		 //check : correct number of cols
		 if(sqlite3_column_count(query.statment) != sizeof...(arg)){
			 throw DbError_get("sqlite : wrong column number in getLine,"
						", expected=" + std::to_string(sizeof...(arg))
					   +", returned=" + std::to_string( sqlite3_column_count(query.statment) )
					   +", sql=" +query.sql());
		 }

		//run the Query_t
		int querry_result;
		char line_count=0;
		while(true){
			querry_result = sqlite3_step(query.statment);
			if(querry_result != SQLITE_ROW){break;}
			query.setFrom(arg...);
			++line_count;
			if(line_count>1){throw DbError_get_tooManyLines("sqlite : too many lines in setFromLine, sql=" + query.sql());}
		}

		if(querry_result!=SQLITE_DONE){
			throw DbError_execute("sqlite : error during execute : querry_result=" + std::to_string(querry_result) + ", sql=" + query.sql()+", msg="+sqlite3_errmsg(db));
		}

		if(line_count==0){throw DbError_get_tooFewLines("sqlite : zero lines in setFromLine, sql=" + query.sql());}
	}



	template<typename...Args > //get a single line. throw if 0 or >=1 data was returned
	bool DbManager<Sqlite_tag>::getRow_optional (Query_t &query, Args &... arg){
		Query_guard query_guard(query);
		std::unique_lock<std::mutex> db_lock(db_mutex);
		//bind nothing

		 //check : correct number of cols
		 if(sqlite3_column_count(query.statment) != sizeof...(arg)){
			 throw DbError_get("sqlite : wrong column number in getLine,"
						", expected=" + std::to_string(sizeof...(arg))
					   +", returned=" + std::to_string( sqlite3_column_count(query.statment) )
					   +", sql=" +query.sql());
		 }

		//run the Query_t
		int querry_result;
		char line_count=0;
		while(true){
			querry_result = sqlite3_step(query.statment);
			if(querry_result != SQLITE_ROW){break;}
			query.setFrom(arg...);
			++line_count;
			if(line_count>1){throw DbError_get("sqlite : too many lines in setFromLine, sql=" + query.sql());}
		}

		if(querry_result!=SQLITE_DONE){
			throw DbError_execute("sqlite : error during execute : querry_result=" + std::to_string(querry_result) + ", sql=" + query.sql()+", msg="+sqlite3_errmsg(db));
		}

		return line_count !=0;
	}





	template< typename ...Args >
	void DbManager<Sqlite_tag>::getTuple (Query_t &query   , std::tuple<Args...> &t){
		Query_guard query_guard(query);
		std::unique_lock<std::mutex> db_lock(db_mutex);
		//bind nothing

		 //check : correct number of cols
		 if(sqlite3_column_count(query.statment) != sizeof...(Args)){
			 throw DbError_get("sqlite : wrong column number in getLine,"
						", expected=" + std::to_string(sizeof...(Args))
					   +", returned=" + std::to_string( sqlite3_column_count(query.statment) )
					   +", sql=" +query.sql());
		 }

		 //run the Query_t
		int querry_result;
		char line_count=0;
		Tuple_setFrom_r fn(query); //<----change here
		do{
			 querry_result = sqlite3_step(query.statment);
			 if(querry_result  == SQLITE_ROW){
				 tuple_apply(t,fn);     //<----change here
				 ++line_count;
				 if(line_count>1){throw DbError_get("sqlite : too many lines in setFromLine, sql=" + query.sql());}
			 }
		}while(querry_result  == SQLITE_ROW);

		if(querry_result!=SQLITE_DONE){
			throw DbError_execute("sqlite : error during execute : querry_result=" + std::to_string(querry_result) + ", sql=" + query.sql()+", msg="+sqlite3_errmsg(db));
		}

		if(line_count==0){throw DbError_get("sqlite : too zero lines in setFromLine, sql=" + query.sql());}
	}



	template< template <typename...> class Cont, typename ...Args >
	void DbManager<Sqlite_tag>::getTable (Query_t &query,Cont<std::tuple<Args...> > &target){
		Query_guard query_guard(query);
		std::unique_lock<std::mutex> db_lock(db_mutex);

		 //check : correct number of cols
		 if(sqlite3_column_count(query.statment) != sizeof...(Args)){
			 throw DbError_get("sqlite : wrong column number in getLine,"
						", expected=" + std::to_string(sizeof...(Args))
					   +", returned=" + std::to_string( sqlite3_column_count(query.statment) )
					   +", sql=" +query.sql());
		 }

		//run the Query_t
		int querry_result;
		std::tuple<Args...> tmp_tuple;
		Tuple_setFrom_r fn(query);
		do{
			 querry_result = sqlite3_step(query.statment);
			 if(querry_result  == SQLITE_ROW){
				 tuple_apply(tmp_tuple,fn);
				 unicont::move_in(target,std::move(tmp_tuple));
				 //DbContainer<Container_t>::move_in(target,std::move(tmp_tuple));
			 }
		}while(querry_result  == SQLITE_ROW);

		if(querry_result!=SQLITE_DONE){
			throw DbError_execute("sqlite : error during execute : querry_result=" + std::to_string(querry_result) + ", sql=" + query.sql()+", msg="+sqlite3_errmsg(db));
		}

	}

	//TODO buggy !!!!
	template<typename Cont, typename ... Args>
	void DbManager<Sqlite_tag>::getColumn(Query_t &query, Cont &c, Args ... bind_me ){
		//bind
		Query_guard query_guard(query);
		query.bind(bind_me...);

		//check : correct number of cols
		 if(sqlite3_column_count(query.statment) != 1 ){
			 throw DbError_get("sqlite : wrong column number in getColumn,"
						", expected= 1"
					    ", returned=" + std::to_string( sqlite3_column_count(query.statment) )
					   +", sql=" +query.sql());
		 }

		 //run the Query_t
		 typedef typename Cont::value_type value_t;
		 int querry_result;

		 do{
			 querry_result = sqlite3_step(query.statment);
			 if(querry_result  == SQLITE_ROW){
				 value_t v;
				 query.setFrom(v);
				 //DbContainer<Cont>::move_in(c,std::move(v));
				 unicont::move_in(c,std::move(v));

			 }
		}while(querry_result  == SQLITE_ROW);

		if(querry_result!=SQLITE_DONE){
			throw DbError_execute("sqlite : error during execute : querry_result=" + std::to_string(querry_result) + ", sql=" + query.sql()+", msg="+sqlite3_errmsg(db));
		}
	}



	template<typename Fn>
	bool  DbManager<Sqlite_tag>::getApply_bool(Query_t &query  , Fn applied_fn){
		using namespace tuple_tools;
		Query_guard query_guard(query);
		std::unique_lock<std::mutex> db_lock(db_mutex);

		typedef typename tuple_arguments<Fn>::type Tuple_t;
		const size_t tuple_size = std::tuple_size<Tuple_t>::value;

		 //check : correct number of cols
		 if(sqlite3_column_count(query.statment) !=tuple_size){
			 throw DbError_get("sqlite : wrong column number in getLine,"
						", expected=" + std::to_string(tuple_size)
					   +", returned=" + std::to_string( sqlite3_column_count(query.statment) )
					   +", sql=" +query.sql());
		 }

		//run the Query_t
		int querry_result;
		Tuple_t tmp_tuple;
		Tuple_setFrom_r fn(query);
		bool ok;
		do{
			 querry_result = sqlite3_step(query.statment);
			 if(querry_result  == SQLITE_ROW){
				 tuple_apply(tmp_tuple,fn); //get tuple
				 ok = tuple_function(applied_fn,tmp_tuple);//call fn from tuple
				 if(!ok){return false;}
			 }
		}while(querry_result  == SQLITE_ROW);

		if(querry_result!=SQLITE_DONE){
			throw DbError_execute("sqlite : error during execute : querry_result=" + std::to_string(querry_result) + ", sql=" + query.sql()+", msg="+sqlite3_errmsg(db));
		}

		return ok;
	}

	template<typename Fn>
	void DbManager<Sqlite_tag>::getApply_void(Query_t &query  , Fn applied_fn){
		using namespace tuple_tools;
		Query_guard query_guard(query);
		std::unique_lock<std::mutex> db_lock(db_mutex);

		typedef typename tuple_arguments<Fn>::type Tuple_t;
		const size_t tuple_size = std::tuple_size<Tuple_t>::value;

		 //check : correct number of cols
		 if(sqlite3_column_count(query.statment) !=tuple_size){
			 throw DbError_get("sqlite : wrong column number in getLine,"
						", expected=" + std::to_string(tuple_size)
					   +", returned=" + std::to_string( sqlite3_column_count(query.statment) )
					   +", sql=" +query.sql());
		 }

		//run the Query_t
		int querry_result;
		Tuple_t tmp_tuple;
		Tuple_setFrom_r fn(query);
		do{
			 querry_result = sqlite3_step(query.statment);
			 if(querry_result  == SQLITE_ROW){
				 tuple_apply(tmp_tuple,fn); //get tuple
				 tuple_function(applied_fn,tmp_tuple);//call fn from tuple
			 }
		}while(querry_result  == SQLITE_ROW);

		if(querry_result!=SQLITE_DONE){
			throw DbError_execute("sqlite : error during execute : querry_result=" + std::to_string(querry_result) + ", sql=" + query.sql()+", msg="+sqlite3_errmsg(db));
		}
	}


	template<>
	struct DbManager<Sqlite_tag>::getApply_dispatch<true>{
		template<typename Fn>
		static bool run(DbManager &db, Query_t &query  , Fn fn){
			return db.getApply_bool(query,fn);
		}
	};

	template<>
	struct DbManager<Sqlite_tag>::getApply_dispatch<false>{
		template<typename Fn>
		static void run(DbManager &db, Query_t &query  , Fn fn){
			db.getApply_void(query,fn);
		}
	};

	template<typename Fn>
	auto DbManager<Sqlite_tag>::getApply(Query_t &query  , Fn fn)
	-> typename tuple_tools::return_type<Fn>::type{
		const bool return_bool=std::is_same<typename tuple_tools::return_type<Fn>::type,bool>::value;
		return getApply_dispatch<return_bool>::run(*this,query,fn);
	}

	template<typename Fn>
	auto DbManager<Sqlite_tag>::getApply(const Sql_t &sql, Fn fn)
		->typename  tuple_tools::return_type<Fn>::type{
		Query_t q=this->prepare(sql);
		return getApply(q,fn);
	}





	inline      DbManager<Sqlite_tag>::Query_guard::~Query_guard(){query.reset_binding();}

	template<typename T>
	inline void DbManager<Sqlite_tag>::Tuple_bind_r::run(const T &t,const size_t I){query.bind(t);}

	template<typename T>
	inline void DbManager<Sqlite_tag>::Tuple_setFrom_r::run(T &t ,const size_t I){query.extract_r(I,t);}




	template<typename... Args>
	DbManager<Sqlite_tag>::GetData_apply<std::tuple<Args...> >::GetData_apply(Query_t &q):fn(q),query(q){
		//check todo optimize : do it for 1st Query_t only
		if(sqlite3_column_count(query.statment) != sizeof...(Args)){
			 throw DbError_get("sqlite : wrong column number in apply (table number of columns doesn't match function number of args),"
						", function_arg=" + std::to_string(sizeof...(Args))
					   +", table_col="    + std::to_string( sqlite3_column_count(query.statment) )
					   +", sql=" +query.sql());
		}
	}


	template<typename... Args>
	std::tuple<Args...> DbManager<Sqlite_tag>::GetData_apply<std::tuple<Args...> >::get_data(){
		std::tuple <Args...> tmp_tuple;
		tuple_apply(tmp_tuple,fn);
		return tmp_tuple;
	}

	template<typename... Args>
	bool DbManager<Sqlite_tag>::GetData_apply<std::tuple<Args...> >::have_data()const{
		//try to launch the request
		querry_result = sqlite3_step(query.statment);
		if(querry_result == SQLITE_ROW) {return true;}
		if(querry_result == SQLITE_DONE){return false;}
		throw DbError_execute("sqlite : error in apply during execute : querry_result=" + std::to_string(querry_result) + ", sql=" + query.sql()+", msg="+sqlite3_errmsg(db));
	}




	//apply fn in parallel
	//This code has high synchronization costs. Running fn in parallel
	//is faster only if fn is slower than getting data out of the db
	//for best performance, write a benchmark with parallel v.s. not parallel
	template<typename Fn>
	auto DbManager<Sqlite_tag>::getApply_parallel(const Sql_t &sql, Fn fn,const size_t cache_size)-> void{
		auto q = this->prepare(sql);
		getApply_parallel(q,fn,cache_size);
	}




	template<typename Fn>
	auto DbManager<Sqlite_tag>::getApply_parallel(Query_t &query  , Fn applied_fn, const size_t cache_size)-> void{
		using namespace sqlwrapper::mt_impl;
		using namespace tuple_tools;

		typedef typename tuple_arguments<Fn>::type Tuple_t;
		typedef typename JobPool_run<Tuple_t>::Page Page_t;

		//few thread -> single thread version
		unsigned int hardware_thread = std::thread::hardware_concurrency();
		if(hardware_thread<2){this->getApply(query,applied_fn); return;}

		const size_t tuple_size = std::tuple_size<Tuple_t>::value;


		Query_guard query_guard(query);
		std::unique_lock<std::mutex> db_lock(db_mutex);

		//check : correct number of cols
		 if(sqlite3_column_count(query.statment) !=tuple_size){
			 throw DbError_get("sqlite : wrong column number in getLine,"
						", expected=" + std::to_string(tuple_size)
					   +", returned=" + std::to_string( sqlite3_column_count(query.statment) )
					   +", sql=" +query.sql());
		 }

		auto fetch_fn=[&](Page_t &write_here)->bool{
			size_t row_count =0;
			write_here.reserve(cache_size);

			int querry_result;

			Tuple_setFrom_r fn(query);
			do{
				 querry_result = sqlite3_step(query.statment);
				 if(querry_result  == SQLITE_ROW){
					 write_here.emplace_back();
					 tuple_apply(write_here.back(),fn);
				 }
				 ++row_count;
			}while(querry_result  == SQLITE_ROW and row_count <cache_size );

			if(querry_result!=SQLITE_DONE and querry_result!=SQLITE_ROW){
				throw DbError_execute("sqlite : error during execute : querry_result=" + std::to_string(querry_result) + ", sql=" + query.sql()+", msg="+sqlite3_errmsg(db));
			}

			return querry_result==SQLITE_ROW;
		};

		auto process_fn=[&applied_fn](Page_t &process_me)->void{
			for(auto &t : process_me){
				tuple_function(applied_fn,t);
			};
		};

		JobPool_run<Tuple_t>::run(fetch_fn,process_fn);

	}



	/*
	template<typename Fn>
	auto DbManager<Sqlite_tag>::getApply_parallel(Query_t &query  , Fn applied_fn, const size_t cache_size)-> void{



		assert(cache_size>2);
		unsigned int hardware_thread = std::thread::hardware_concurrency();
		if(hardware_thread<2){this->getApply(query,applied_fn); return;}


		typedef typename tuple_tools::tuple_arguments<Fn>::type Tuple_t;

		std::deque<Tuple_t>     data_queue;
		std::mutex              data_mutex;
		std::condition_variable data_cond;
		std::condition_variable data_full;

		std::atomic<bool>       data_todo(true);

		auto thread_getdata=[&](){
			using namespace tuple_tools;
			Query_guard query_guard(query);
			std::unique_lock<std::mutex> db_lock(db_mutex);

			const size_t tuple_size = std::tuple_size<Tuple_t>::value;

			 //check : correct number of cols
			 if(sqlite3_column_count(query.statment) !=tuple_size){
				 throw DbError_get("sqlite : wrong column number in getLine,"
							", expected=" + std::to_string(tuple_size)
						   +", returned=" + std::to_string( sqlite3_column_count(query.statment) )
						   +", sql=" +query.sql());
			 }

			//run the Query_t
			int querry_result;
			Tuple_t tmp_tuple;
			Tuple_setFrom_r fn(query);
			do{
				 querry_result = sqlite3_step(query.statment);
				 if(querry_result  == SQLITE_ROW){
					 tuple_apply(tmp_tuple,fn); //get tuple

					 //queue is full, wait
					 //aquire lock
					 std::unique_lock<std::mutex> l(data_mutex);
					 data_full.wait(l, [&]{ return data_queue.size()<cache_size;} );

					 //store tuple in data queue
					 data_queue.emplace_back(tmp_tuple);
					 data_cond.notify_one();

				 }
			}while(querry_result  == SQLITE_ROW);

			if(querry_result!=SQLITE_DONE){
				throw DbError_execute("sqlite : error during execute : querry_result=" + std::to_string(querry_result) + ", sql=" + query.sql()+", msg="+sqlite3_errmsg(db));
			}

			data_todo=false;
			data_cond.notify_all();
		};

		auto thread_process=[&](){
			while(true){
				//break
				if(!data_todo){
					std::unique_lock<std::mutex> l(data_mutex);
					if(data_queue.empty()){break;}
				}

				//get data
				std::unique_lock<std::mutex> l(data_mutex);
				data_cond.wait(l, [&]{ return !data_queue.empty() or !data_todo ;} );
				if(!data_queue.empty()){
					const Tuple_t data = std::move(data_queue.front());
					data_queue.pop_front();
					l.unlock();
					data_full.notify_one();
					tuple_tools::tuple_function(applied_fn,data);
				}else{
					l.unlock();
					data_full.notify_one();
				}
			}
		};

		//spawn thread_getdata.
		size_t nb_process = hardware_thread-1;
		std::thread thread_get1(thread_getdata);



		//spawn thread_process MULTIPLE times
		std::vector<std::thread> thread_process_vector;
		thread_process_vector.reserve(nb_process);

		for(size_t i = 0; i < nb_process ; ++i){ //TODO 1 *-> nb_process
			thread_process_vector.emplace_back(thread_process);
		}

		//join
		thread_get1.join();
		for(auto & t : thread_process_vector){t.join();}

		assert(data_queue.empty());



	}
*/






}//end namespace sqlwrapper
