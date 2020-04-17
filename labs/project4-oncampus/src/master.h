#pragma once

#include "mapreduce_spec.h"
#include "file_shard.h"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <vector>
#include <memory>
#include <sys/stat.h>

#include <grpcpp/grpcpp.h>
#include "masterworker.grpc.pb.h"

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;

using masterworker::File;
using masterworker::ShardPartition;
using masterworker::MapQuery;
using masterworker::MapResult;
using masterworker::MasterWorker;
using masterworker::ReduceQuery;
using masterworker::ReduceResult;


class AsyncClientCall{
	public:
		bool is_map_job;
		ClientContext context;
		Status status;
		std::string worker_ip_addr;
		virtual ~AsyncClientCall() {}
};
class MapCall : public AsyncClientCall{
	public:
		MapResult result;
		std::unique_ptr<ClientAsyncResponseReader<MapResult>> rpc;
};

class ReduceCall : public AsyncClientCall{
	public:
		ReduceResult result;
		std::unique_ptr<ClientAsyncResponseReader<ReduceResult>> rpc;
};

class WorkerClient
{
public:
	WorkerClient(std::string ip_addr, CompletionQueue* cq){
		ip_addr_ = ip_addr;
		stub_ = MasterWorker::NewStub(grpc::CreateChannel(ip_addr, grpc::InsecureChannelCredentials()));
		cq_ = cq;
	}

	void schedule_mapper_job(std::string, int, FileShard);
	void schedule_reducer_job(std::string, int, std::string, std::vector<std::string>);

private:
	std::unique_ptr<MasterWorker::Stub> stub_;
	CompletionQueue* cq_;
	std::string ip_addr_;
};

void WorkerClient::schedule_mapper_job(std::string user_id, int n_partitions, FileShard shard)
{

	MapQuery query;
	query.set_user_id(user_id);
	query.set_n_partitions(n_partitions);
	for (int i = 0; i < shard.file_names.size(); i++){
		ShardPartition *partition = query.add_partitions();
		partition->set_filename(shard.file_names[i]);
		partition->set_start(shard.offsets[i].first);
		partition->set_end(shard.offsets[i].second);
	}

	auto call = new MapCall;
	call->rpc = stub_->PrepareAsyncmapper(&call->context, query, cq_);
	call->is_map_job = true;
	call->worker_ip_addr = ip_addr_;
	call->rpc->StartCall();
	call->rpc->Finish(&call->result, &call->status, (void*)call);
}

void WorkerClient::schedule_reducer_job(std::string user_id, int partition_id, std::string output_dir, std::vector<std::string> files){
	ReduceQuery query;
	query.set_user_id(user_id);
	query.set_partition_id(partition_id);
	query.set_output_dir(output_dir);
	for (int i = 0; i < files.size(); i++){
		File *file = query.add_files();
		file->set_filename(files[i]);
	}

	auto call = new ReduceCall;
	call->rpc = stub_->PrepareAsyncreducer(&call->context, query, cq_);
	call->is_map_job = false;
	call->worker_ip_addr = ip_addr_;
	call->rpc->StartCall();
	call->rpc->Finish(&call->result, &call->status, (void*)call);
}
class Master;
/* CS6210_TASK: Handle all the bookkeeping that Master is supposed to do.
	This is probably the biggest task for this project, will test your understanding of map reduce */
class Master
{

public:
	/* DON'T change the function signature of this constructor */
	Master(const MapReduceSpec &, const std::vector<FileShard> &);

	/* DON'T change this function's signature */
	bool run();

private:
	/* NOW you can add below, data members and member functions as per the need of your implementation*/
	// shared completion queue for all the worker clients
	CompletionQueue* cq_;
	
	std::map<std::string, std::unique_ptr<WorkerClient>> ip_addr_to_worker;
	std::queue<std::string> ready_queue;
	// ip->shard_index or ip->interm_file_index
	std::map<std::string, int> busy_workers;
	int n_workers;
	std::vector<FileShard> shards;
	std::string output_dir;
	std::mutex m;
	std::condition_variable cv;
	std::string user_id;
	std::vector<std::string> interm_files;
	std::vector<std::string> output_files;
	int n_partitions;
	bool map_complete;
	void async_map_reduce();
};

/* CS6210_TASK: This is all the information your master will get from the framework.
	You can populate your other class data members here if you want */
Master::Master(const MapReduceSpec& mr_spec, const std::vector<FileShard>& file_shards) {
	n_workers = mr_spec.n_workers;
	for(auto shard:file_shards)
		shards.push_back(shard);
	output_dir = mr_spec.output_dir;
	user_id = mr_spec.user_id;
	n_partitions = mr_spec.n_output_files;
	cq_ = new CompletionQueue;
	map_complete = false;
	// Create a thread for each worker and open a channel to make rpc calls
	for (int i = 0; i < mr_spec.n_workers; i++){
		auto client = std::unique_ptr<WorkerClient> (new WorkerClient (mr_spec.worker_ipaddr_ports[i], cq_));
		ip_addr_to_worker[mr_spec.worker_ipaddr_ports[i]] = std::move(client);
		ready_queue.push(mr_spec.worker_ipaddr_ports[i]);
	}
}


/* CS6210_TASK: Here you go. once this function is called you will complete whole map reduce task and return true if succeeded */
bool Master::run() {

	// Schedule map jobs
	std::thread schedule_map_jobs(&Master::async_map_reduce, this);
	mkdir("intermediate", 0777);
	for (int i = 0; i < shards.size(); i++){
		FileShard shard = shards[i];
		{
			// check if we have a free worker
			std::unique_lock<std::mutex> lk(m);
			cv.wait(lk, [this] { return !ready_queue.empty(); });
			// great we the mutex and atleast one free worker
			std::string worker_ip = ready_queue.front();
			ready_queue.pop();
			busy_workers[worker_ip] = i;
			lk.unlock();
			cv.notify_one();
			auto client = ip_addr_to_worker[(worker_ip)].get();
			client->schedule_mapper_job(user_id, n_partitions, shard);
		}
	}
	// stop all the workers
	schedule_map_jobs.join();
	//std::cout << "Map job done" << std::endl;
	map_complete = true;
	//Schedule reduce jobs
	std::thread schedule_reduce_jobs(&Master::async_map_reduce, this);
	std::unordered_map<int, std::vector<std::string>> files_to_reduce;
	for (auto file : interm_files){
		// intermediate/1_localhost:50001.txt
		int pos = file.find_first_of('/') + 1;
		int end = file.find_first_of('_');
		int idx = std::stoi(file.substr(pos, end-pos));
		if(files_to_reduce.find(idx) == files_to_reduce.end())
			files_to_reduce[idx] = std::vector<std::string>();
		files_to_reduce[idx].push_back(file);
	}
	
	for (int i = 0; i < n_partitions;i++){
		// check if we have a free worker
		std::unique_lock<std::mutex> lk(m);
		cv.wait(lk, [this] { return !ready_queue.empty(); });
		// great we the mutex and atleast one free worker
		std::string worker_ip = ready_queue.front();
		ready_queue.pop();
		busy_workers[worker_ip] = i;
		lk.unlock();
		cv.notify_one();
		auto client = ip_addr_to_worker[(worker_ip)].get();
		auto files = files_to_reduce[i];
		client->schedule_reducer_job(user_id, i, output_dir, files);
	}
	
	// stop all the workers
	schedule_reduce_jobs.join();

	// remove all the intermediate files
	// for(auto file:interm_files)
	// 	std::remove(file.c_str());
	
	return true;
}

void Master::async_map_reduce(){
	    void* got_tag;
        bool ok = false;
	
        while (cq_->Next(&got_tag, &ok)) {
			AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);
			
            GPR_ASSERT(ok);
			if (call->status.ok())
			{
				// restore the worker back to the ready queue
				// and make it free
				{
					std::lock_guard<std::mutex> lk(m);
					ready_queue.push(call->worker_ip_addr);
					busy_workers.erase(call->worker_ip_addr);
					cv.notify_one();
				}
				// based on the job kind, store the result
				if(call->is_map_job){
					MapCall* mcall = dynamic_cast<MapCall*>(call);
					// no need for the lock, since we won't access this in master until we are done with all map jobs
					for (int i = 0; i < mcall->result.files_size(); i++)
						interm_files.push_back(mcall->result.files(i).filename());
				}
				else{
					ReduceCall* rcall = dynamic_cast<ReduceCall*>(call);
					output_files.push_back(rcall->result.file().filename());
				}
			}
			// Once we're complete, deallocate the call object.
            delete call;
			// check if we have received all the required map messages
			// Note in case we are in reduce job this condition is no longer valid
			// So check for map_complete before that
			if(!map_complete && n_partitions * shards.size() == interm_files.size())
				return;
			// in reduce and we have received all the files
			if(map_complete && output_files.size() == n_partitions)
				return;
		}
}