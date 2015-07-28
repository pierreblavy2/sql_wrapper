//support for common base type line ints, double or string

#include <string>




namespace sqlwrapper{
//--- extract from DB ---

	template<>
	struct DbExtract_t<Sqlite_tag,double>{
		static void run(Query<Sqlite_tag> &query,size_t I, double &t){
			const auto coltype=sqlite3_column_type(query.statment,I);
			if(coltype!=SQLITE_FLOAT){throw DbError_wrongtype("sqlite : wrong type cannot get double ,column="+std::to_string(I)+", sql="+query.sql() + ", type=" + sqlite_impl::sqlite_coltype(coltype));}
			t=sqlite3_column_double(query.statment,I);
		}
	};



	template<>
	struct DbExtract_t<Sqlite_tag,int>{
		static void run(Query<Sqlite_tag> &query,size_t I, int &t){
			const auto coltype=sqlite3_column_type(query.statment,I);
				if(coltype !=SQLITE_INTEGER){throw DbError_wrongtype("sqlite : wrong type cannot get int ,column="+std::to_string(I)+", sql="+query.sql()+ ", type=" + sqlite_impl::sqlite_coltype(coltype));}
				t=sqlite3_column_int  (query.statment,I);
		}
	};


	template<>
	struct DbExtract_t<Sqlite_tag,sqlite3_int64>{
		static void run(Query<Sqlite_tag> &query,size_t I, sqlite3_int64 &t){
			const auto coltype=sqlite3_column_type(query.statment,I);
			if(coltype!=SQLITE_INTEGER){throw DbError_wrongtype("sqlite : wrong type cannot get int64 ,column="+std::to_string(I)+", sql="+query.sql()+ ", type=" + sqlite_impl::sqlite_coltype(coltype));}
			t=sqlite3_column_int64 (query.statment,I);
		}
	};

	template<>
	struct DbExtract_t<Sqlite_tag,size_t>{
		static void run(Query<Sqlite_tag> &query,size_t I, size_t &t){
			const auto coltype=sqlite3_column_type(query.statment,I);
			if(coltype!=SQLITE_INTEGER){throw DbError_wrongtype("sqlite : wrong type cannot get size_t ,column="+std::to_string(I)+", sql="+query.sql()+ ", type=" + sqlite_impl::sqlite_coltype(coltype));}
			t=sqlite3_column_int64 (query.statment,I);
		}
	};


	template<>
	struct DbExtract_t<Sqlite_tag,std::string>{
		static void run(Query<Sqlite_tag> &query,size_t I, std::string &t){
			//doc : https://stackoverflow.com/questions/804123/const-unsigned-char-to-stdstring
			const auto coltype=sqlite3_column_type(query.statment,I);
			if(coltype!=SQLITE_TEXT){throw DbError_wrongtype("sqlite : wrong type cannot get string ,column="+std::to_string(I)+", sql="+query.sql()+ ", type=" + sqlite_impl::sqlite_coltype(coltype));}
			t=static_cast<std::string>( reinterpret_cast<const char*>(sqlite3_column_text  (query.statment,I)) );

		}
	};

	/*
	template<>
	struct DbExtract_t<Sqlite_tag,time_tools::Time>{
		static void run(Query<Sqlite_tag> &query,size_t I, time_tools::Time &t){
			std::string s;
			DbExtract_t<Sqlite_tag,std::string>::run(query,I,s);
			try{time_tools::Time t2(s); t=t2;}
			catch(time_tools::TimeFormat_error &e){
				  throw  DbError_bind(std::string("sqlite : time_error") + e.what()+" ,column="+std::to_string(I)+", sql="+query.sql());
			}//no other exception should happen.
		}
	};
	*/


	template<>
	struct DbExtract_t<Sqlite_tag,bool>{
		static void run(Query<Sqlite_tag> &query,size_t I, bool &t){
			int d;
			dbExtract(query,I,d);
			t= (d!=0);
		}
	};

	template<>
	struct DbExtract_t<Sqlite_tag,Null >{
		static void run(Query<Sqlite_tag> &query,size_t I, const Null &t){}
	};



	template<typename T>
	struct DbExtract_t<Sqlite_tag,Optional<T> >{
		static void run(Query<Sqlite_tag> &query,size_t I, Optional<T> &t){
			if(sqlite3_column_type(query.statment,I)==SQLITE_NULL){t.first=false; return;}
			t.first=true;
			dbExtract(query,I,t.second);
		}
	};




	//---data binding : bind to DB ---

	template<>
	struct DbBind_t<Sqlite_tag,double >{
		static void run(Query<Sqlite_tag> &query,size_t i, const double &d){
			auto status = sqlite3_bind_double(query.statment,i,d);
			if(status != SQLITE_OK){throw DbError_bind("sqlite : cannot dbBind double, index=" + std::to_string(i) +", value=" + std::to_string(d) + ",  error="+ std::to_string(status)+", sql="+query.sql());}

		}
	};

	template<>
	struct DbBind_t<Sqlite_tag,sqlite3_int64 >{
		static void run(Query<Sqlite_tag> &query,size_t i, const sqlite3_int64 &d){
			auto status = sqlite3_bind_int64(query.statment,i,d);
			if(status != SQLITE_OK){throw DbError_bind("sqlite : cannot dbBind int64, index=" + std::to_string(i) +", value=" + std::to_string(d) + ",  error="+ std::to_string(status)+", sql="+query.sql());}
		}
	};

	template<>
	struct DbBind_t<Sqlite_tag,int>{
		static void run(Query<Sqlite_tag> &query,size_t i, const int &d){
			dbBind(query,i,static_cast<sqlite3_int64>(d));
		}
	};

	template<>
	struct DbBind_t<Sqlite_tag,size_t>{
		static void run(Query<Sqlite_tag> &query,size_t i, const size_t &d){
			dbBind(query,i,static_cast<sqlite3_int64>(d));
		}
	};


	template<>
	struct DbBind_t<Sqlite_tag, std::string >{
		static void run(Query<Sqlite_tag> &query,size_t i, const  std::string &d){
			auto status = sqlite3_bind_text(query.statment,i,d.c_str(),d.size(),SQLITE_TRANSIENT);
			if(status != SQLITE_OK){throw DbError_bind("sqlite : cannot dbBind text, index=" + std::to_string(i) +", value=" + d + ",  error="+ std::to_string(status)+", sql="+query.sql());}
		}
	};


	template<>
	struct DbBind_t<Sqlite_tag, const char* >{
		static void run(Query<Sqlite_tag> &query,size_t i, const  char* c){
			auto status = sqlite3_bind_text(query.statment,i,c,strlen(c),SQLITE_TRANSIENT);
			if(status != SQLITE_OK){throw DbError_bind("sqlite : cannot dbBind const char*, index=" + std::to_string(i) +", value=" + std::string(c) + ",  error="+ std::to_string(status)+", sql="+query.sql());}
		}
	};

	template<>
	struct DbBind_t<Sqlite_tag,Null >{
		static void run(Query<Sqlite_tag> &query,size_t i, const  Null&d){
			auto status = sqlite3_bind_null(query.statment,i);
			if(status != SQLITE_OK){throw DbError_bind("sqlite : cannot dbBind null, index=" + std::to_string(i) + ",  error="+ std::to_string(status)+", sql="+query.sql());}
		}
	};



	template<>
	struct DbBind_t<Sqlite_tag, bool >{
		static void run(Query<Sqlite_tag> &query,size_t i, const bool d){
			  int x; if(d){x=1;}else{x=0;}
			  dbBind(query,i,x);
		}
	};

	template<typename T>
	struct DbBind_t<Sqlite_tag, Optional<T> >{
		static void run(Query<Sqlite_tag> &query,size_t i, const Optional<T> &t){
			if(t.first){dbBind(query,i,t.second);}
			else{Null n;  dbBind(query,i,n);}
		}
	};

}//end namespace sqlwrapper
