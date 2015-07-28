//============================================================================
// Name        : JobPool
// Author      : Pierre BLAVY
// Version     : 1.0
// Copyright   : LGPL 3.0+ : https://www.gnu.org/licenses/lgpl.txt
// Description : A multithread job pool  the main thread fetch data
//               - the main thread fetches data
//               - the main thread spawn child threads to handle the data
//               - some data is locally cached in pages (i.e. vectors)
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


#ifndef INCLUDE_SQLWRAPPER_MULTITHREAD_IMPL_MT_JOBPOOL_HPP_
#define INCLUDE_SQLWRAPPER_MULTITHREAD_IMPL_MT_JOBPOOL_HPP_



#include <vector>
#include <deque>
#include <atomic>
#include <future>
#include <type_traits>
#include <memory>

namespace sqlwrapper{
namespace mt_impl{



template<
  typename Data_t,
  typename Fetch_t, //ex : bool fetch_fn (std::vector<Data_t> & write_here);
  typename Process_t//ex : void process_fn(Page &p);
>
struct JobPool_t{

	static size_t guess_thread_number(){
		size_t i = std::thread::hardware_concurrency();
		if(i>2){i=i-2;}
		else{i=1;}
		return i;
	}

	static size_t guess_page_number(){return guess_thread_number()*10;}

	JobPool_t(
			Fetch_t   fetch_fn_,
			Process_t process_fn_,
			size_t max_pages_=  guess_page_number(),
			size_t max_thread_= guess_thread_number()
	):
		max_pages(max_pages_),
		max_thread(max_thread_),
		fetch_fn(fetch_fn_),
		process_fn(process_fn_),
		future_pool(max_thread_){}

	typedef std::vector<Data_t> Page;

private:
	typedef JobPool_t<Data_t, Fetch_t, Process_t> This_t;

	typedef void(*process_wrapper)(std::unique_ptr<Page> &&p);
	typedef decltype(
		std::async(std::launch::async,
		static_cast<process_wrapper>(nullptr),
		std::unique_ptr<Page>(nullptr)
	)) future_t;





public:

	void reset( //never call while running
			Fetch_t   fetch_fn_,
			Process_t process_fn_,
			size_t max_pages_ = guess_page_number(),
			size_t max_thread_= guess_thread_number()

	){
		set_fetch_fn   (fetch_fn_);
		set_process_fn (process_fn_);
		set_max_pages  (max_pages_);
		set_max_threads(max_thread_);
	}

	size_t    get_max_pages()   const{return max_pages;}
	size_t    get_max_threads() const{return max_thread;}
	Fetch_t   get_fetch_fn()    const{return fetch_fn;}
	Process_t get_process_fn()  const{return process_fn;}

	void set_max_pages  (const size_t    &s){max_pages=s;}
	void set_max_threads(const size_t    &s){max_thread=s;future_pool.resize(s);}
	void set_fetch_fn   (const Fetch_t   &s){fetch_fn=s;}
	void set_process_fn (const Process_t &s){process_fn=s;}

	void run(){
		//if(max_pages<max_thread){max_pages=max_thread;}

		bool more_data=true;
		do{


			//wait if queue is full
			if(data.size()+1>=max_pages){
				//wait until at least one thread finishes
				thread_available_mt.lock();
			}


			//fetch some data, set more_data
			if(more_data and data.size()<max_pages){
				data.emplace_back(new Page);
				more_data = fetch_fn(*data.back()) ;
			}

			//then try to re-launch each finished thread
			//note that when a thread finishes, it unlocks thread_available_mt
			if(!data.empty()){
			for(size_t i = 0; i <future_pool.size() ; ++i ){

				bool launch;
				if( future_pool[i].valid()){
					launch=future_pool[i].wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
				}else{
					launch=true;
				}


				if(launch){
					if(!data.empty()){
						std::unique_ptr<Page> tmp(nullptr);
						std::swap(data.back(),tmp);
						data.pop_back();

						auto process_wrapper = [&](std::unique_ptr<Page> &&p){
							std::unique_ptr<Page> local_copy;
							std::swap(p,local_copy);
							process_fn(*local_copy);
							thread_available_mt.unlock();
						};
						future_pool[i]=std::async(std::launch::async,process_wrapper,std::move(tmp)); //todo
					}else{break;}
				}
			}
			}
		}while(more_data or !data.empty());

		//be sure that every thread finishes
		for(size_t i = 0; i <future_pool.size() ; ++i ){
			if(future_pool[i].valid()){future_pool[i].get();}
		}
	}

private:
	void static_check(){
		typedef decltype(process_fn(Page())) process_fn_return_t;
		static_assert(std::is_same<process_fn_return_t,bool>::value, "process_fn must return bool");
	}

	size_t max_pages ;
	size_t max_thread;

	Fetch_t   fetch_fn  =nullptr;
	Process_t process_fn=nullptr;

	std::deque< std::unique_ptr<Page> > data;
	std::vector<future_t> future_pool;
	std::mutex thread_available_mt;
};



template<typename Data_t>
struct JobPool_run{
	template<
	  typename Fetch_t, //ex : bool fetch_fn (std::vector<Data_t> & write_here);
	  typename Process_t//ex : void process_fn(Page &p);
	>
	static void run(
			Fetch_t fetch_fn,
			Process_t process_fn,
			size_t max_pages =JobPool_t<Data_t,Fetch_t,Process_t>::guess_page_number(),
			size_t max_thread=JobPool_t<Data_t,Fetch_t,Process_t>::guess_thread_number()

	){
		JobPool_t<Data_t,Fetch_t,Process_t> p(
				fetch_fn,
				process_fn,
				max_pages,
				max_thread
		);
		p.run();
	}

	typedef std::vector<Data_t> Page;

};



}//end namepsace sqlwrapper
}//end namespace mt_impl


/*example

#include <iostream>
#include <string>
#include <unistd.h>


bool fetch_example(JobPool_run<int>::Page &write_here){

	static std::atomic<int> i(0);
	std::cout << "fetch" << i <<":"<<&write_here<<  std::endl;


	for(size_t j = 0; j<100; ++j){
		write_here.emplace_back(i*100+j);
	}

	++i;
	return i<30;
}

void process_example(JobPool_run<int>::Page &process_me){

	static std::atomic<int> p_count(0);
	static std::mutex m;

	std::string s;

	if(p_count>10){sleep(3);}

	for(const auto &x : process_me){
		s+=" "+std::to_string(x);
	}

	std::unique_lock<std::mutex> l(m);
	std::cout <<"process "<<p_count<<":"<<&process_me << ":"<< s << std::endl;;
	++p_count;
}





void example()
{
	JobPool_run<int>::run(fetch_example,process_example,3,7);
}
*/



#endif /* INCLUDE_SQLWRAPPER_MULTITHREAD_IMPL_MT_JOBPOOL_HPP_ */
