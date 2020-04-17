#pragma once

#include <iostream> 
#include <fstream>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <vector>

inline void split_string_to_string_list(const std::string& input, std::vector<std::string>& res){
	int last = 0;
	int curr = input.find_first_of(',');
	while (curr != std::string::npos){
		res.push_back(input.substr(last, curr - last));
		last = curr + 1;
		curr = input.find_first_of(',', last);
	}
	res.push_back(input.substr(last, curr - last));
}

inline bool file_exists (const std::string& name) {
	struct stat st;
	return (stat(name.c_str(), &st) == 0);
}

inline bool is_dir(const std::string& name){
	struct stat st;
	if(stat(name.c_str(), &st) == 0)
		if(st.st_mode & S_IFDIR )
			return true;
	return false;
}

/* CS6210_TASK: Create your data structure here for storing spec from the config file */
struct MapReduceSpec {
	int n_workers;
	std::vector<std::string> worker_ipaddr_ports;
	std::vector<std::string> input_files;
	std::string output_dir;
	int n_output_files;
	int map_kilobytes;
	std::string user_id;
};

/* CS6210_TASK: Populate MapReduceSpec data structure with the specification from the config file */
inline bool read_mr_spec_from_config_file(const std::string& config_filename, MapReduceSpec& mr_spec) {
	try{
		std::ifstream fin(config_filename.c_str());
		std::string line;
		while (getline(fin, line))
		{
			// Split line to extract the config
			int curr = line.find_first_of('=');
			std::string key = line.substr(0, curr);
			std::string value = line.substr(curr + 1, std::string::npos);

			if(key == "n_workers"){
				mr_spec.n_workers = std::stoi(value);
			}
			else if(key == "worker_ipaddr_ports"){
				// Expecting comma separated list
				std::vector<std::string> ip_addrs;
				split_string_to_string_list(value, ip_addrs);
				mr_spec.worker_ipaddr_ports = ip_addrs;
			}
			else if(key == "input_files"){
				std::vector<std::string> inp_vec;
				split_string_to_string_list(value, inp_vec);
				mr_spec.input_files = inp_vec;
			}
			else if(key == "output_dir"){
				mr_spec.output_dir = value;
			}
			else if(key == "n_output_files"){
				mr_spec.n_output_files = std::stoi(value);
			}
			else if(key == "map_kilobytes"){
				mr_spec.map_kilobytes = std::stoi(value);
			}
			else if(key == "user_id"){
				mr_spec.user_id = value;
			}
		}
		fin.close();
	}
	catch(...){
		return false;
	}
		
	return true;
}


/* CS6210_TASK: validate the specification read from the config file */
inline bool validate_mr_spec(const MapReduceSpec& mr_spec) {

	if(mr_spec.n_workers <= 0)
		return false;
	if(mr_spec.worker_ipaddr_ports.size() != mr_spec.n_workers)
		return false;
	if(mr_spec.input_files.size() == 0)
		return false;
	for(auto f:mr_spec.input_files)
		if (file_exists(f) == 0)
			return false;
		
	if (mr_spec.output_dir.empty() || !is_dir(mr_spec.output_dir))
		return false;
	if(mr_spec.n_output_files <= 0 || mr_spec.map_kilobytes <= 0)
		return false;
	if(mr_spec.user_id.empty())
		return false;
	return true;
}
