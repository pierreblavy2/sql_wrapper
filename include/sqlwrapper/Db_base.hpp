//This header contains stuff common to all DB like exceptions and template_stubs.


#ifndef SQWRAPPER_DB_BASE_HPP_
#define SQWRAPPER_DB_BASE_HPP_

#include <type_traits>
#include <string>
#include <stdexcept>
#include <ostream>

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


namespace sqlwrapper{

	//--- database related types ---
	//User must specialize this stuff to create a custom binding to a database system
	//Db_tag is the database type (e.g. Sqlite_tag). That's an empty class
	template<typename Db_tag> struct DbManager;     //specialize : put code to manage database here
	template<typename Db_tag> struct Query;         //specialize : put query managment here
	template<typename Db_tag> struct Rowid;         //specialize : put query managment here
	template<typename Db_tag> struct DbConnectInfo; //specialize : put connection info in this class
	template<typename Db_tag> struct DbTransaction; //specialize : put Transasction object in this class
	template<typename Db_tag> struct DbSavepoint;   //specialize : put Save point  object in this class
	template<typename Db_tag> struct Sql;           //specialize : typedef Sql<Db_tag>::type as SQL data type



	//---extract data and convert stuff ---
	//user may specialize
	//- DbExtract_t to extract from database to custom type
	//- DbBind_t to bind custom types to Query arguments
	template<typename Query_t, typename T> struct DbExtract_t;//Must provide static void run(Query<Db_tag> &db,size_t col_index, T&target)
	template<typename Query_t, typename T> struct DbBind_t;   //Must provide static void run(Query<Db_tag> &db,size_t col_index, const T& bind_me)



	//--- Exception managment ---
	//user can derive from this class to add exception to this hierarchy
	MK_EXCEPTION_BASE(DbError)

	MK_EXCEPTION(DbError_connect,DbError)
	MK_EXCEPTION(DbError_query,DbError)
	MK_EXCEPTION(DbError_time,DbError)
	MK_EXCEPTION(DbError_file,DbError)

	MK_EXCEPTION(DbError_prepare,DbError_query)
	MK_EXCEPTION(DbError_bind,DbError_query)
	MK_EXCEPTION(DbError_execute,DbError_query)
	MK_EXCEPTION(DbError_get,DbError_query)

	MK_EXCEPTION(DbError_get_tooManyLines,DbError_get);
	MK_EXCEPTION(DbError_get_tooFewLines,DbError_get)



	MK_EXCEPTION(DbError_wrongtype,DbError_get) //error in the query for getting data : wrong type



	//===After this point, the user should not change/specialize stuff===


	//---data types that never changes ---
	struct Null{};

	//todo c++14, please use std::optional instead
	template<typename T>
	struct Optional:std::pair<bool,T>{
		typedef std::pair<bool,T> base_t;
		//using base_t::base_t;
		Optional(){this->first=false;}
		Optional(bool b_, const T&t_):base_t(b_,t_){}
		Optional(const Optional<T> & o)=default;
		Optional(Optional<T> &&)=default;
		Optional& operator=(const T&t){this->first=true, this->second=t; return *this;}

	};


	//--- template magic : helper functions ---
	template<typename Db_tag, typename T>
	void dbExtract(Query<Db_tag> &q,size_t col_index, T&target){
		typedef DbExtract_t<Db_tag,T> q_t;
		q_t::run(q,col_index,target);
	}

	template<typename Db_tag, typename T>
	void dbBind(Query<Db_tag> &q,size_t col_index, const T&bind_me){
		typedef DbBind_t<Db_tag,T> b_t;
		b_t::run(q,col_index,bind_me);
	}

	template<typename Db_tag>
	DbManager<Db_tag> make_DbManager(const DbConnectInfo<Db_tag> &con){
		return std::move(DbManager<Db_tag>(con));
	}



	//user may specialize
	template<typename Db_tag>
	struct Column_info{
		std::string column_name;
		std::string table_name;
		std::string database_name;
		void print(std::ostream &out)const{out << database_name<<"."<<table_name<<"."<<column_name;}
	};



}//namespace sqlwrapper

template<typename Db_tag>
inline std::ostream & operator<<(std::ostream &out, const sqlwrapper::Column_info<Db_tag> &d){d.print(out); return out;}




//Automaticaly define types related to a Db_tag
#define SQLWRAPPER_TYPES(tag)\
  typedef tag DbTag_t;\
  typedef ::sqlwrapper::Query<tag>          Query_t;\
  typedef ::sqlwrapper::DbConnectInfo<tag>  DbConnectInfo_t;\
  typedef ::sqlwrapper::DbManager<tag>      DbManager_t;\
  typedef ::sqlwrapper::DbTransaction<tag>  DbTransaction_t;\
  typedef ::sqlwrapper::DbSavepoint<tag>    DbSavepoint_t;\
  typedef typename ::sqlwrapper::Sql<tag>::type    Sql_t;\
  typedef typename ::sqlwrapper::Rowid<tag>::type  Rowid_t;

#endif /* DBMANAGER_HPP_ */
