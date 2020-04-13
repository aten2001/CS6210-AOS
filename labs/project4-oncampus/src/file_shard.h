#pragma once

#include <vector>
#include <iostream> 
#include <fstream>
#include <string>
#include <sstream>
#include "mapreduce_spec.h"

inline long get_file_size(const std::string& name){
    struct stat st;
    stat(name.c_str(), &st);
    return st.st_size;
}

inline long get_approx_split(const std::string& name, long offset, long approx_size){
     long f_size = get_file_size(name);
     if (offset + approx_size > f_size) return (f_size - offset);

     long res_size = approx_size;
     
     std::ifstream f;
     f.open(name.c_str());
     // search for the nearest \n
     bool found = false;
     long offset_pos = offset + approx_size;
     
     while (f.eof() || found){
          char buffer[50];
          f.seekg(offset_pos);
          f.read(buffer, sizeof(buffer));
          for (int i = 0; i < f.gcount(); i++){
               res_size ++;
               if(buffer[i] == '\n'){
                    found = true;
                    break;
               }
          }
          offset_pos = offset + res_size;
     }
     
     return res_size;
}
/* CS6210_TASK: Create your own data structure here, where you can hold information about file splits,
     that your master would use for its own bookkeeping and to convey the tasks to the workers for mapping */
struct FileShard {
     std::vector<std::string> file_names;
     std::vector<std::pair<long, long>> offsets; // (start, end)
};

/* CS6210_TASK: Create fileshards from the list of input files, map_kilobytes etc. using mr_spec you populated  */ 
inline bool shard_files(const MapReduceSpec& mr_spec, std::vector<FileShard>& fileShards) {
     auto input_files = mr_spec.input_files;
     long shard_size = mr_spec.map_kilobytes;

     FileShard curr_shard;
     // What portion of shard is still to be filled
     long rem_shard_size = shard_size;
     for (auto file : input_files){
          long f_size = get_file_size(file);
          long rem_f_size = f_size;
          long f_offset = 0;
          while (rem_f_size > 0){
               if (rem_shard_size >= rem_f_size){
                    curr_shard.file_names.push_back(file);
                    curr_shard.offsets.push_back({f_offset, f_offset + rem_f_size});
                    rem_shard_size -= rem_f_size;
                    rem_f_size = 0;
               }
               else{
                    // we can only add a fraction of current file in this shard
                    // try getting teh approx split and append to the global collection
                    long used_size = get_approx_split(file, f_offset, rem_shard_size);
                    curr_shard.file_names.push_back(file);
                    curr_shard.offsets.push_back({f_offset, f_offset + used_size});
                    // push curr shard and start a new one
                    fileShards.push_back(curr_shard);
                    // start a new shard to hold the remaining file
                    curr_shard = FileShard();
                    rem_shard_size = shard_size;
                    rem_f_size -= used_size;
                    f_offset += used_size;
               }
          }
     }
     return true;
}
