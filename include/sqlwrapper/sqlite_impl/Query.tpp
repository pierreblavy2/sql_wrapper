#include <cstring> //for strlen

namespace sqlwrapper{

	inline void Query<Sqlite_tag>::clear(){
		if(statment==nullptr){return;}
		nb_bind=0;
		sqlite3_clear_bindings(statment);
		sqlite3_finalize(statment);
		statment=nullptr;
	}


	inline Query<Sqlite_tag>::Query(Query &&q)
	:nb_bind(q.nb_bind){
		std::swap(q.statment,statment);
	}


	inline Query<Sqlite_tag>::~Query(){ sqlite3_finalize(statment);}


	inline  auto Query<Sqlite_tag>::sql()const -> const Sql_t{
		return sqlite3_sql(statment);
	}


	template<typename... Data>
	inline void  Query<Sqlite_tag>::bind(Data... data){
		bind_r(1+nb_bind,data...);
		nb_bind+=sizeof...(Data);
	}


	template<typename... Data>
	inline void  Query<Sqlite_tag>::setFrom(Data&... data){
		extract_r(0,data...);
	}


	inline void  Query<Sqlite_tag>::reset_binding(){
		if(statment==nullptr){return;}
		nb_bind=0;
		sqlite3_clear_bindings(statment);
		sqlite3_reset(statment);
	}


	template<typename T, typename... Args>
	inline void  Query<Sqlite_tag>::reset_binding(const T &t, Args... args){
		reset_binding();
		bind(t,args...);
	}


	template<typename T, typename... Args>
	inline void Query<Sqlite_tag>::bind_r( size_t i, const T&t, Args... rest){
		sqlwrapper::dbBind(*this,i,t);
		bind_r(i+1,rest...);
	}

	template<typename T, typename... Args>
	inline void Query<Sqlite_tag>::extract_r(size_t index,  T&t, Args&... rest){
		sqlwrapper::dbExtract(*this,index ,t);
		extract_r(index+1,rest...);
	}













}//end namespace sqlwrapper
