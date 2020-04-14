#pragma once

#include <mr_task_factory.h>
#include "mr_tasks.h"
#include <grpcpp/grpcpp.h>
#include "masterworker.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using masterworker::File;
using masterworker::FileShard;
using masterworker::MapQuery;
using masterworker::MasterWorker;

class MapperServiceImpl final : public MasterWorker::Service{
	Status mapper(ServerContext* context, const MapQuery* request, File* reply){
		// mapper implementation
		return Status::OK;
	}
};

class ReducerServiceImpl final : public MasterWorker::Service{
	
	Status reducer(ServerContext* context, const File* request, File* reply){
		// reducer implementation
		return Status::OK;
	}
};

/* CS6210_TASK: Handle all the task a Worker is supposed to do.
	This is a big task for this project, will test your understanding of map reduce */
class Worker {

	public:
		/* DON'T change the function signature of this constructor */
		Worker(std::string ip_addr_port);

		/* DON'T change this function's signature */
		bool run();

	private:
		ServerBuilder builder;
		MapperServiceImpl map_service;
		ReducerServiceImpl red_service;

		/* NOW you can add below, data members and member functions as per the need of your implementation*/

};


/* CS6210_TASK: ip_addr_port is the only information you get when started.
	You can populate your other class data members here if you want */
Worker::Worker(std::string ip_addr_port) {
	builder.AddListeningPort(ip_addr_port, grpc::InsecureServerCredentials());

}

extern std::shared_ptr<BaseMapper> get_mapper_from_task_factory(const std::string& user_id);
extern std::shared_ptr<BaseReducer> get_reducer_from_task_factory(const std::string& user_id);

/* CS6210_TASK: Here you go. once this function is called your woker's job is to keep looking for new tasks 
	from Master, complete when given one and again keep looking for the next one.
	Note that you have the access to BaseMapper's member BaseMapperInternal impl_ and 
	BaseReduer's member BaseReducerInternal impl_ directly, 
	so you can manipulate them however you want when running map/reduce tasks*/
bool Worker::run() {
	/*  Below 5 lines are just examples of how you will call map and reduce
		Remove them once you start writing your own logic */ 
	
	builder.RegisterService(&map_service);
	builder.RegisterService(&red_service);

	std::unique_ptr<Server> server(builder.BuildAndStart());

	server->Wait();
	/*
	std::cout << "worker.run(), I 'm not ready yet" <<std::endl;
	auto mapper = get_mapper_from_task_factory("cs6210");
	mapper->map("I m just a 'dummy', a \"dummy line\"");
	auto reducer = get_reducer_from_task_factory("cs6210");
	reducer->reduce("dummy", std::vector<std::string>({"1", "1"}));
	
	*/
	return true;
}
