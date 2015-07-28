//============================================================================
// Name        : sqlite_wrapper
// Author      : Pierre BLAVY
// Version     : 1.0
// Copyright   : LGPL 3.0+ : https://www.gnu.org/licenses/lgpl.txt
// Description : Sqlite wrapper example code
// Installation: - Copy tuple_tools and sqlwrapper in a folder
//               - add this forlder to your include paths (gcc flag -I/your_folder/)
//               - #include <sqlwrapper/sqlite.hpp>
//               - compile with c++11   (gcc flag -std=c++11 )
//               - link with libsqlite3 (gcc flag -lsqlite3)
//               - pray the voodoo god of computers
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


#include <sqlwrapper/sqlite.hpp>
#include <sqlwrapper/sqlite_Boost_date.hpp>


#include <iostream>




#include <chrono>
#include <unistd.h> //for usleep

struct Timer{
	Timer(const std::string &message_):message(message_){
		start = std::chrono::system_clock::now();
		std::cout << "BEGIN : "<< message << std::endl;
	}
	~Timer(){end();}

	void end(){
		if(print_end){
			auto end  = std::chrono::system_clock::now();
			auto sec  = std::chrono::duration_cast<std::chrono::seconds>
					                             (end-start).count();
		    std::cout << "END : "<< message <<" : "<< sec <<"s" << std::endl;
		}
		print_end=false;
	}

	bool print_end=true;
	std::chrono::time_point<std::chrono::system_clock> start;
	std::string message;
};



void test_multithread(){

	//create a connection
	sqlwrapper::DbConnectInfo<sqlwrapper::Sqlite_tag>   con("test_multithread.sqlite3");
	auto db = sqlwrapper::make_DbManager(con); //db is a sqlwrapper::DbManager<sqlwrapper::Sqlite_tag>
	auto tr=db.transaction();
	db.execute("drop table if exists test");
	db.execute("create table if not exists test(i integer NOT NULL, s varchar, primary key(i))");

	const size_t test_size = 100000;

	{//populate DB
		Timer t("populate db");
		auto q= db.prepare("insert into test(i,s) values (?,?)");
		for(size_t i = 0; i < test_size; ++i){
			db.execute(q,i,"s"+std::to_string(i));
		}
	}

	//test functions to apply
	auto quick_fn=[](int i, std::string s){assert(s=="s"+std::to_string(i));};
    auto slow_fn =[](int i, std::string s){usleep(100);};

    std::mutex sync_fn_mutex;
    auto sync_fn =[&](int i, std::string s){sync_fn_mutex.lock(); usleep(10); sync_fn_mutex.unlock();};

    std::deque<size_t> test_data; test_data.resize(test_size);
    auto test_fn=[&](int i, std::string s){test_data[(size_t)(i)]=(size_t)(i);};

    //benchmark
    auto select_all = db.prepare("select i,s from test");
    auto benchmark = [&](const std::string &name, auto apply_fn ){
    	{
    		Timer t("not_parallel " + name);
    		db.getApply(select_all,apply_fn);
    	}
    	{
    	    Timer t("parallel " + name);
    	    db.getApply_parallel(select_all,apply_fn);
    	}
    };

    benchmark ("quick",quick_fn);
    benchmark ("slow" ,slow_fn);
    benchmark ("sync" ,sync_fn);
    benchmark ("test" ,test_fn);

    //check that test is correctly filled
    for(size_t i = 0; i <test_size ; ++i){
    	assert(i==test_data[i]);
    }
    std::cout << "parallel check OK"<<std::endl;


	tr.rollback(); //test code, do not change DB
}


struct Fn_example{
	  template<typename T>
	  void run(const T &t,const size_t i)     {std::cout << i << ":" << t << "\n";}
};




void test_classical(){
	//create a connection
		sqlwrapper::DbConnectInfo<sqlwrapper::Sqlite_tag>   con("test.sqlite3");
		con.max_compound_select.set(450);
		auto db = sqlwrapper::make_DbManager(con); //db is a sqlwrapper::DbManager<sqlwrapper::Sqlite_tag>

		//directly execute a query that do not return anything
		db.execute("drop table if exists test");
		db.execute("create table if not exists test(i integer NOT NULL, s varchar, primary key(i))");

		//insert a single row
		db.insertRow("insert into test values(?,?)", 0,"zero");


		//inserted row may contain null values.
		//you can directly pass a sqlwrapper::Db_sqlite::Null() object to represent a null value
		db.insertRow("insert into test values(?,?)",100,sqlwrapper::Null() );

		//or you can use an sqlwrapper::Db_sqlite::Optional<T>, if you've got a value that may be null
		//note Optional will be replace by std::optional in the future
		sqlwrapper::Optional<std::string> may_be_null;
		may_be_null.second="not_used_when_null";
		may_be_null.first = false; //it's null
		db.insertRow("insert into test values(?,?)",101,may_be_null);



		//insert a row from a tuple
		db.insertTuple("insert into test values(?,?)",std::make_tuple(2,"two") );

		//insert many rows from a container of tuple
		std::vector<std::tuple<int,std::string> > v;
		v.emplace_back(3,"three"); v.emplace_back(4,"four");
		db.insertTable("insert into test values(?,?)",v );


		//manage queries by hand (queries can be used instead of raw sql strings in all requests)
		auto query = db.prepare("insert into test values(?,?)");//query is a sqlwrapper::Query<sqlwrapper::Sqlite_tag>
		query.bind(1); query.bind("one");  //or query.bind(1,"one");
		db.execute(query);


		//for the next function that create or append to containers,
		//you can extend the library to use customs a container called MyCont<T,U...>:
		//     - write a specification of template< typename T > struct DbContainer<MyCont,T>
		//     - write this specification in the namespace sqlwrapper
		//     - see the examples for vector and deque in sqlwrapper/DbManager.hpp


		//get data from a single row
		//note that an error is thrown if the request do not return exactly one row
		int get_i;
		std::string get_s;
		db.getRow ("select i,s from test where i=1",get_i,  get_s);
		assert(get_i==1);
		assert(get_s=="one");



		//idem, but put data in a tuple
		auto get_tuple = db.getTuple<int,std::string>("select i,s from test where i=1"); //note get tuple is a std::tuple<int,std::string>
		assert(std::get<0>(get_tuple)==1);
		assert(std::get<1>(get_tuple)=="one");



		//Append data as tuples in an existing container
		// don't forget to use optional if some values can be null
		//(see the next example)
		std::deque< std::tuple<int,std::string> > cont1;
		db.getTable ("select * from test where i<10",cont1);

		// don't forget to use optional if some values can be null
		typedef sqlwrapper::Optional<std::string> Optional_str;
		std::deque< std::tuple<int,  Optional_str  > > cont2;
		db.getTable ("select * from test",cont2);



		//create a new container of tuples
		auto cont3 = db.getTable<std::deque,int,std::string> ("select * from test where i<10");
		//cont3 is a std::deque<std::tuple<int,std::string> >

		// don't forget to use optional if some values can be null
		typedef sqlwrapper::Optional<std::string> Optional_str;
		auto cont4 = db.getTable<std::deque,int,Optional_str> ("select * from test");



		//append a single column to existing container
		std::vector<int> cont5;
		db.getColumn("select i from test where i <?",cont5,2);

		auto cont6=db.getColumn<std::vector,std::string>("select s from test WHERE s NOT NULL");
		//cont6 is std::vector<std::string>;



		//apply a function row by row
		//Use this interface to build general things from select requests
		//rows are fechhed from the sqlite3 lib one by one
		typedef sqlwrapper::Optional<std::string> Optional_str;

		//function (or functor or lambda) arguments must match the requested stuff
		//function return true if we want to fetch the next line
		//this one prints the first 2 lines
		size_t printed = 0;
		auto print_fn=[&printed](int i,Optional_str s)->bool{
			std::cout << i << " ";
			if(s.first){std::cout << s.second;}
			else       {std::cout << "NULL";}
			std::cout << "\n";
			++printed;
			return printed<2;
		};
		std::cout << "--- bool ---\n";
		bool print_ok = db.getApply("select i,s from test",print_fn);
		std::cout<<"has_print_everything="<<print_ok<<"\n\n";





		//idem with a function that return void
		auto print_fn_void=[](int i,Optional_str s)->void{
			std::cout << i << " ";
			if(s.first){std::cout << s.second;}
			else       {std::cout << "NULL";}
			std::cout << "\n";
		};
		std::cout << "--- void ---\n";
		db.getApply("select i,s from test",print_fn_void);




		//idem but in parallel (works only for void function, don't know how exceptions are handled)
		std::mutex cout_mutex;
		auto print_fn_parallel=[&cout_mutex](int i,Optional_str s)->void{
			std::string s_str;
			if(s.first){s_str = s.second;}
			else       {s_str = "NULL";}

			std::unique_lock<std::mutex> l(cout_mutex);
			std::cout << i << " " << s_str  << "\n";
		};
		std::cout << "--- parallel ---\n";
		db.getApply_parallel("select i,s from test",print_fn_parallel);
		std::cout << "------\n";

		//Transactions
		//db.transaction() returns a Transaction RAII Object that
		// - create a transaction  on construction
		// - commit or rollback the transaction the first time the commit() or rollback() functions are called
		// - call rollback() on destruction
		//Note thate this object is implemented by calling (BEGIN TRANSACTION, COMMIT, or ROLLBACK), therefore
		// - DO NOT SHORT CIRCUIT the Transaction object by manual calls to
		//   db.execute("BEGIN TRANSACTION"), db.execute("COMMIT") or ,db.execute("ROLLBACK")
		// - DO NOT NEST TRANSACTIONS (be sure that only one transaction object at a time exists)
		//   USE Savepoint, if you need nesting
		{
			auto transaction = db.transaction(); //begin a  transaction here
			db.execute("delete from test");
		}//transaction is destroyed --> AUTOMATIC ROLLBACK

		{
			auto transaction = db.transaction(); //begin a  transaction here
			db.execute("delete from test");
			transaction.rollback();
			transaction.commit();//do nothing : already rolled back
		}//transaction is destroyed --> //do nothing : already rolled back


		{
			auto transaction=db.transaction();
			db.execute("delete from test where i=100");
			db.execute("delete from test where i=101");
			transaction.commit();   //rows 100 and 101 are destroyed
			transaction.commit();   //was commited before, do nothing.
			transaction.rollback(); //was commited before, do nothing.
		}//transaction is destroyed --> transaction was commited before, do nothing.



		//Savepoints
		// - works like transactions, with the same mechanisms and the same limitations
		//   except that savepoints allow nesting.
		// - Savepoints are automatically named s0, s1, s2 ...
		// - The savepoint counter is stored in the database manager as a size_t
		//   It is never reseted, so it may overflow.
		{
			auto savepoint_1 = db.savepoint();
			db.insertRow("insert into test values(?,?)", 50,"fifty");
			{
				auto savepoint_2 = db.savepoint();
				db.insertRow("insert into test values(?,?)", 51,"fifty-one");
			}//savepoint_2 automatically rollback

			savepoint_1.commit();
		}
		//row 50 was inserted.
		//row 51 was ignored
		db.getApply("select i,s from test",print_fn);

}




void test_date(){
	//create a connection
		sqlwrapper::DbConnectInfo<sqlwrapper::Sqlite_tag>   con("test.sqlite3");
		con.max_compound_select.set(450);
		auto db = sqlwrapper::make_DbManager(con); //db is a sqlwrapper::DbManager<sqlwrapper::Sqlite_tag>

		//directly execute a query that do not return anything
		db.execute("drop table if exists test");
		db.execute("create table if not exists test(i integer NOT NULL, s varchar, primary key(i))");
		db.execute("delete from test");


		//EXPERIMENTAL SUPPORT OF TIMES with boost
		//store a time as a string in the db
		time_tools::Boost_date t;
		time_tools::from_string("21-01-2014 18:15:17","%d-%m-%Y %H:%M:%S",t);
		db.execute("INSERT INTO test VALUES(null,?)",t); //insert 2014-01-21 18:15:17 (iso format)

		//check the stored string,
		//should be yyyy-mm-dd H:M:S
		std::string date_str;
		db.getRow("select s from test",date_str);
		assert(date_str=="2014-01-21 18:15:17");
		std::cout << "date_test 1 OK" << std::endl;

		//get it as a date
		time_tools::Boost_date date;
		db.getRow("select s from test",date);
		assert(date==t);
		std::cout << "date_test 2 OK" << std::endl;





}


void test_exceptions(){
	//create a connection
		sqlwrapper::DbConnectInfo<sqlwrapper::Sqlite_tag>   con("test.sqlite3");
		con.max_compound_select.set(450);
		auto db = sqlwrapper::make_DbManager(con); //db is a sqlwrapper::DbManager<sqlwrapper::Sqlite_tag>

		//directly execute a query that do not return anything
		db.execute("drop table if exists test");
		db.execute("create table if not exists test(i integer NOT NULL, s varchar, primary key(i))");


		//ERROR : getting null values without optional throw DbError_wrongtype
		try{auto crash_me = db.getTable<std::deque,int,std::string> ("select * from test");
		}catch(sqlwrapper::DbError_wrongtype &e){}

		//ERROR : getting the wrong column type throw DbError_wrongtype
		try{auto crash_me = db.getTable<std::deque,std::string> ("select i from test");	//i is int, not std::string
		}catch(sqlwrapper::DbError_wrongtype &e){}

		//ERROR : getting null values without optional throw DbError_wrongtype
		try{db.execute("invalid query");
		}catch(sqlwrapper::DbError_query &e){}

		//Full list of errors is in sqlwrapper/DbManager.hpp

}









struct Column_info{
	std::string column_name;
	std::string table_name;
	std::string database_name;
};



void test_column_description(){

	sqlwrapper::DbConnectInfo<sqlwrapper::Sqlite_tag>   con("test.sqlite3");
	auto db = sqlwrapper::make_DbManager(con);

	auto c = db.getColumn_info<std::vector>("select * from test where ?",0);

	for(const auto &i : c){
		std::cout << i << "\n";
	}



	/*
	//db
	sqlite3 *db=nullptr;
	sqlite3_open("test.sqlite3", &db);

	//query
	sqlite3_stmt *statment=nullptr;

	auto status = sqlite3_prepare_v2(db, "select * from test WHERE 0", -1, &statment, 0);
	if(status !=  SQLITE_OK){std::cerr <<"bad query"; return;;}


	auto nbcol =  sqlite3_column_count(statment);

	std::cout << "nbcol =" <<nbcol<<std::endl;


	for(int i =0; i < nbcol ; ++i){
		std::cout<< "name=" << sqlite3_column_origin_name(statment,i)<<"\n";
	}*/



}


int main() {

	//test_multithread();
	test_classical();
	test_date();

	test_column_description();
	std::cout << "everything OK"<<std::endl;



}
