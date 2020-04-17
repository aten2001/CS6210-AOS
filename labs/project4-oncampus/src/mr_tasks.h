#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

/* CS6210_TASK Implement this data structureas per your implementation.
		You will need this when your worker is running the map task*/
struct BaseMapperInternal {

		/* DON'T change this function's signature */
		BaseMapperInternal();

		/* DON'T change this function's signature */
		void emit(const std::string& key, const std::string& val);

		/* NOW you can add below, data members and member functions as per the need of your implementation*/
		void flush();
		int n_partitions;
		std::vector<std::string> interm_files;
		std::vector<std::pair<std::string, std::string>> kvs;
};

/* CS6210_TASK Implement this function */
inline BaseMapperInternal::BaseMapperInternal() {
	
}

/* CS6210_TASK Implement this function */
inline void BaseMapperInternal::emit(const std::string& key, const std::string& val) {
	// perodically flush to the intermediate files 
	if(kvs.size() > 1000){
		flush();
	}
	kvs.push_back({key, val});
	flush();
}

inline void BaseMapperInternal::flush(){
	for(auto &kv:kvs){
		std::string key = kv.first;
		std::string value = kv.second;
		// select which partition to write based on the hash(key)
		std::size_t hash  = std::hash<std::string>{}(key) % n_partitions;
		std::string f_name = interm_files[hash % n_partitions];
		std::ofstream f;
		f.open(f_name, std::ios::out | std::ios::app);
		f << key << ", " << value << std::endl;
		f.close();
	}
	kvs.clear();
}
/*-----------------------------------------------------------------------------------------------*/


/* CS6210_TASK Implement this data structureas per your implementation.
		You will need this when your worker is running the reduce task*/
struct BaseReducerInternal {

		/* DON'T change this function's signature */
		BaseReducerInternal();

		/* DON'T change this function's signature */
		void emit(const std::string& key, const std::string& val);

		/* NOW you can add below, data members and member functions as per the need of your implementation*/
		std::string filename;
};

/* CS6210_TASK Implement this function */
inline BaseReducerInternal::BaseReducerInternal() {

}


/* CS6210_TASK Implement this function */
inline void BaseReducerInternal::emit(const std::string& key, const std::string& val) {
	std::ofstream f(filename, std::ios::out | std::ios::app);
	f << key << " " << val << std::endl;
	f.close();
}
