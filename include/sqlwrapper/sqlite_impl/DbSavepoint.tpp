#ifndef INCLUDE_SQLWRAPPER_SQLITE_IMPL_DBSAVEPOINT_TPP_
#define INCLUDE_SQLWRAPPER_SQLITE_IMPL_DBSAVEPOINT_TPP_




namespace sqlwrapper{


	inline DbSavepoint<Sqlite_tag>::DbSavepoint(DbManager<Sqlite_tag> &db_):db(db_){
		savepoint_id=db.savepoint_newid();
		db.execute("SAVEPOINT " + savepoint_id);
	}


	inline DbSavepoint<Sqlite_tag>::DbSavepoint(DbSavepoint<Sqlite_tag> &&s):
				done(s.done),
				savepoint_id(std::move(s.savepoint_id)),
				db(s.db)
	{s.done=true;}


	inline DbSavepoint<Sqlite_tag>::~DbSavepoint(){if(!done)rollback();}


	inline void  DbSavepoint<Sqlite_tag>::rollback(){
		if(done){return;}
		db.execute("ROLLBACK TO SAVEPOINT " + savepoint_id);
		done=true;
	}


	inline void  DbSavepoint<Sqlite_tag>::release() {
		if(done){return;}
		db.execute("RELEASE SAVEPOINT "     + savepoint_id);
		done=true;
	}


}


#endif
