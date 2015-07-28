#ifndef INCLUDE_SQLWRAPPER_SQLITE_IMPL_DBTRANSACTION_TPP_
#define INCLUDE_SQLWRAPPER_SQLITE_IMPL_DBTRANSACTION_TPP_

namespace sqlwrapper{



		inline DbTransaction<Sqlite_tag>::DbTransaction(DbManager<Sqlite_tag> &db_)
		:db(db_) {
			db.execute("BEGIN TRANSACTION");
		}


		inline DbTransaction<Sqlite_tag>::DbTransaction(DbTransaction<Sqlite_tag> &&s)
		:done(s.done),db(s.db){
			s.done=true;
		}

		inline DbTransaction<Sqlite_tag>::~DbTransaction(){rollback();}

		inline void DbTransaction<Sqlite_tag>::rollback(){
			if(done){return;}
			db.execute("ROLLBACK");
			done=true;
		}


		inline void DbTransaction<Sqlite_tag>::commit  (){
			if(done){return;}
			db.execute("COMMIT");
			done=true;
		}


}

#endif
