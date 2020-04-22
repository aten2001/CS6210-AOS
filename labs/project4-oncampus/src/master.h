#pragma once

#include "mapreduce_spec.h"
#include "file_shard.h"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <vector>
#include <set>
#include <memory>
#include <chrono>
#include <sys/stat.h>
#include <unistd.h>

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
using masterworker::HeartbeatQuery;
using masterworker::HeartbeatResult;

int msg_count = 0;

class AsyncClientCall{
	public:
		bool is_map_job;
		int id;
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

class HeartbeatCall : public AsyncClientCall{
	public:
		HeartbeatResult result;
		std::unique_ptr<ClientAsyncResponseReader<HeartbeatResult>> rpc;
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
	void send_heartbeat_msg(std::string, CompletionQueue *);
	std::vector<FileShard> map_shards;
	std::vector<int> reduce_partitions;

private:
	std::unique_ptr<MasterWorker::Stub> stub_;
	std::mutex m;
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

	map_shards.push_back(shard);

	unsigned int timeout = 1;

	// Set timeout for API
	std::chrono::system_clock::time_point deadline =
		std::chrono::system_clock::now() + std::chrono::seconds(timeout);


	auto call = new MapCall;
	call->id = msg_count++;
	{
		std::unique_lock<std::mutex> lk(m);
		call->rpc = stub_->PrepareAsyncmapper(&call->context, query, cq_);
		call->is_map_job = true;
	//	call->context.set_deadline(deadline);
		call->worker_ip_addr = ip_addr_;
		call->rpc->StartCall();
		call->rpc->Finish(&call->result, &call->status, (void*)call);
	}
	std::cout << __func__ << ": scheduled shard to " << ip_addr_  << " msg id : " << call->id << "\n";
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

	unsigned int timeout = 1;

	// Set timeout for API
	std::chrono::system_clock::time_point deadline =
		std::chrono::system_clock::now() + std::chrono::seconds(timeout);

	auto call = new ReduceCall;
	call->id = msg_count++;
	{
		std::unique_lock<std::mutex> lk(m);
		call->rpc = stub_->PrepareAsyncreducer(&call->context, query, cq_);
		call->is_map_job = false;
	//	call->context.set_deadline(deadline);
		call->worker_ip_addr = ip_addr_;
		call->rpc->StartCall();
		call->rpc->Finish(&call->result, &call->status, (void*)call);
	}

	std::cout << __func__ << ": scheduled partition " << partition_id << " to " << ip_addr_  << " msg id : " << call->id << "\n";
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
	std::vector<std::string> worker_ips;
	std::vector<std::string> dropped_worker_ips;
	std::queue<std::string> ready_queue;
	// ip->shard_index or ip->interm_file_index
	std::map<std::string, int> busy_workers;
	int n_workers;
	int n_shards;
	std::vector<FileShard> shards;
	std::vector<FileShard> rem_shards;
	std::string output_dir;
	std::mutex m;
	std::mutex m_hbt;
	std::mutex m_op;
	std::condition_variable cv;
	std::condition_variable cv_hbt;
	std::condition_variable cv_op;
	std::string user_id;
	std::set<std::string> interm_files;
	std::vector<std::string> output_files;
	std::vector<int> partitions;
	std::vector<int> rem_partitions;
	int n_partitions;
	bool op_flag;
	bool map_complete;
	bool reduce_complete;
	bool trigger_heartbeat;
	bool kill_heartbeat;
	void async_map_reduce();
	void heartbeat();
	void schedule_heartbeat();
	void handle_missing_worker(std::string);
	int n_mapper_messages = 0;
	int n_reducer_messages = 0;
};

/* CS6210_TASK: This is all the information your master will get from the framework.
	You can populate your other class data members here if you want */
Master::Master(const MapReduceSpec& mr_spec, const std::vector<FileShard>& file_shards) {
	n_workers = mr_spec.n_workers;
	for(auto shard:file_shards)
		shards.push_back(shard);
	n_shards = shards.size();
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
		worker_ips.push_back(mr_spec.worker_ipaddr_ports[i]);
	}
}

void Master::schedule_heartbeat()
{
	std::cout << __func__ << ": schedule_heartbeat()\n";

	// ensure op (map/reduce) is complete
	{
		std::unique_lock<std::mutex> lk(m_op);
		op_flag = true;
		cv_op.wait(lk, [this] { return !op_flag; });
	}

	std::cout << __func__ << ": Map or Reduce complete\n";

	// trigger heartbeat
	{
		std::unique_lock<std::mutex> lk(m_hbt);
		trigger_heartbeat = true;
		cv_hbt.notify_one();
		cv_hbt.wait(lk, [this] { return !trigger_heartbeat; });
	}

	std::cout << __func__ << ": Triggered heartbeat " << rem_shards.size() << " " << n_mapper_messages << ", " << rem_partitions.size() << " " << n_reducer_messages << "\n";

	// inform recv thread
	{
		std::unique_lock<std::mutex> lk(m_op);
		if (!map_complete) {
			// workers have dropped. few shards incomplete
			if (rem_shards.size() > 0) {
				n_mapper_messages -= rem_shards.size();
				std::cout << "Decreasing mapper msgs to " << n_mapper_messages << " by " << rem_shards.size() << "\n";
			} else if (n_mapper_messages == n_shards)
				map_complete = true;
		} else if (!reduce_complete) {
			// workers have dropped. few partitions incomplete
			if (rem_partitions.size() > 0) {
				n_reducer_messages -= rem_partitions.size();
				std::cout << "Decreasing reduer msgs to " << n_reducer_messages << " by " << rem_partitions.size() << "\n";
			} else if (output_files.size() == n_partitions)
				reduce_complete = true;
		}
		op_flag = true;
		cv_op.notify_one();
	}

	std::cout << __func__ << ": schedule_heartbeat() complete: " << map_complete << " " << reduce_complete << "\n";

	return;
}


/* CS6210_TASK: Here you go. once this function is called you will complete whole map reduce task and return true if succeeded */
bool Master::run() {
	// Schedule heartbeat thread
	std::thread heartbeat_job(&Master::heartbeat, this);

	// Schedule map jobs
	std::thread schedule_map_jobs(&Master::async_map_reduce, this);
	mkdir("intermediate", 0777);

	bool shards_done = false;
	while (shards_done == false) {
		int num = 0;
		for (int i = 0; i < shards.size(); i++){
			FileShard shard = shards[i];
			{
				// check if we have a free worker
				std::unique_lock<std::mutex> lk(m);
				std::cout << __func__ << ": waiting for worker thread for shard " << i << "\n";
				cv.wait(lk, [this] { return !ready_queue.empty(); });
				// great we got the mutex and atleast one free worker
				std::string worker_ip = ready_queue.front();
				ready_queue.pop();
				if (std::find(dropped_worker_ips.begin(), dropped_worker_ips.end(), worker_ip) != dropped_worker_ips.end()) {
					std::cout << __func__ << ": dropped worker in ready queue " << worker_ip << "\n";
					continue;
				}
				busy_workers[worker_ip] = i;
				lk.unlock();
				cv.notify_one();
				std::cout << __func__ << ": got worker thread for shard " << i << " " << worker_ip << "\n";
				auto client = ip_addr_to_worker[(worker_ip)].get();
				client->schedule_mapper_job(user_id, n_partitions, shard);
				num++;
			}
		}

		std::cout << __func__ << ": ---> About to trigger map heartbeat " << shards.size() << ":" << num << "\n";
		// check if all map jobs are complete
		schedule_heartbeat();
		std::cout << __func__ << ": ---> map Heartbeat serviced " << rem_shards.size() << "\n";
		if ((rem_shards.size() == 0) && (map_complete == true)) {
			shards.clear();
			shards_done = true;
		}

		// some shards haven't been processed
		if (rem_shards.size() > 0) {
			// clear shards
			shards.clear();
			// add rem_shards to shards
			for (auto shard = rem_shards.begin(); shard != rem_shards.end(); ++shard)
				shards.push_back(*shard);
			// clear rem_shards
			rem_shards.clear();
			std::cout << __func__ << " " << shards.size() << " shards were remaining. going again\n";
		}
	}
	std::cout << __func__ << ": Map job left" << std::endl;

	// stop all the workers
	schedule_map_jobs.join();
	std::cout << __func__ << ": Map job done" << std::endl;
	map_complete = true;

	std::cout << __func__ << ": Interm files : " << interm_files.size() << "\n";
	for (auto file = interm_files.begin(); file != interm_files.end(); file++)
		std::cout << __func__ << *file << "\n";

	//Schedule reduce jobs
	std::thread schedule_reduce_jobs(&Master::async_map_reduce, this);
	std::unordered_map<int, std::vector<std::string>> files_to_reduce;
	for (auto file : interm_files){
		// intermediate/1_50001.txt
		int pos = file.find_first_of('/') + 1;
		int end = file.find_first_of('_');
		int idx = std::stoi(file.substr(pos, end-pos));
		if(files_to_reduce.find(idx) == files_to_reduce.end())
			files_to_reduce[idx] = std::vector<std::string>();
		files_to_reduce[idx].push_back(file);
	}

	std::cout << __func__ << ": 1\n";
	for (auto o = files_to_reduce.begin(); o != files_to_reduce.end(); o++)
		partitions.push_back(o->first);
	std::cout << __func__ << ": 2\n";

	bool partitions_done = false;
	while (partitions_done == false) {

		int num = 0;
		for (int i = 0; i < partitions.size(); i++){
			// check if we have a free worker
			std::unique_lock<std::mutex> lk(m);
			cv.wait(lk, [this] { return !ready_queue.empty(); });
			// great we got the mutex and atleast one free worker
			std::string worker_ip = ready_queue.front();
			ready_queue.pop();
			busy_workers[worker_ip] = partitions[i];
			lk.unlock();
			cv.notify_one();
			auto client = ip_addr_to_worker[(worker_ip)].get();
			auto files = files_to_reduce[partitions[i]];
			client->schedule_reducer_job(user_id, partitions[i], output_dir, files);
			num++;
		}

		// check if all reduce jobs are complete
		std::cout << __func__ << ": ---> About to trigger reduce heartbeat " << shards.size() << ":" << num << "\n";
		schedule_heartbeat();
		std::cout << __func__ << ": ---> reduce Heartbeat serviced " << rem_shards.size() << "\n";
		if ((rem_partitions.size() == 0) && (reduce_complete == true)) {
			partitions.clear();
			partitions_done = true;
		}

		// some shards haven't been processed
		if (rem_partitions.size() > 0) {
			// clear shards
			partitions.clear();
			// add rem_shards to shards
			for (auto rp = rem_partitions.begin(); rp != rem_partitions.end(); ++rp)
				partitions.push_back(*rp);
			// clear rem_shards
			rem_partitions.clear();
			std::cout << __func__ << files_to_reduce.size() << " partitions were remaining. going again\n";
		}
	}
	
	std::cout << __func__ << ": Reduce job left" << std::endl;

	// stop all the workers
	schedule_reduce_jobs.join();
	std::cout << __func__ << ": Reduce job done" << std::endl;

	// kill heartbeat thread
	{
		std::unique_lock<std::mutex> lk(m_hbt);
		kill_heartbeat = true;
		trigger_heartbeat = true;
		cv_hbt.notify_one();
	}
	std::cout << __func__ << ": Heartbeat thread notified of impending death" << std::endl;

	// wait for heartbeat thread to exit cleanly
	heartbeat_job.join();
	std::cout << __func__ << ": Heartbeat ended" << std::endl;

	// remove all the intermediate files
	std::cout << __func__ << ": Removing intermediate files: " << interm_files.size() << "\n";
	for(auto file:interm_files)
		std::remove(file.c_str());
	rmdir("intermediate");
	
	return true;
}

void Master::async_map_reduce(){
	    void* got_tag;
        bool ok = false;
		while (cq_->Next(&got_tag, &ok))
		{
			AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);
			
            //GPR_ASSERT(ok);
			if (call->status.ok())
			{
				std::cout << __func__ << ": got normal response. curr: " << n_mapper_messages << " : " << n_reducer_messages << "\n";
				// check if msg if from a missing worker
				if (std::find(worker_ips.begin(), worker_ips.end(), call->worker_ip_addr) == worker_ips.end()) {
					std::cout << __func__ << ": Dropping response from " << call->worker_ip_addr << "\n";
					if (call->is_map_job) n_mapper_messages++;
					else n_reducer_messages++;

				} else {
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
						n_mapper_messages++;
						MapCall *mcall = dynamic_cast<MapCall *>(call);
						// no need for the lock, since we won't access this in master until we are done with all map jobs
						for (int i = 0; i < mcall->result.files_size(); i++)
							interm_files.insert(mcall->result.files(i).filename());
					}
					else{
						n_reducer_messages++;
						ReduceCall* rcall = dynamic_cast<ReduceCall*>(call);
						output_files.push_back(rcall->result.file().filename());
					}
				}
			} else {
				std::cout << __func__ << ": recv error: " << call->status.error_message() << " for worker " << call->worker_ip_addr << "\n";
			}		

			std::cout << __func__ << ": Received message from " << call->worker_ip_addr << " " << n_mapper_messages << " : " << n_reducer_messages << " msg id: " <<  call->id << "\n";

			// Once we're complete, deallocate the call object.
            delete call;
			// check if we have received all the required map messages
			// Note in case we are in reduce job this condition is no longer valid
			// So check for map_complete before that
			bool do_it = false;
			{
				std::unique_lock<std::mutex> lk(m_op);
				if ((dropped_worker_ips.size() == 0) && (n_mapper_messages == n_shards) && !map_complete)
					do_it = true;
				else if ((dropped_worker_ips.size() == 0) && (n_reducer_messages == n_partitions) && !reduce_complete)
					do_it = true;
				else if (dropped_worker_ips.size() > 0)
					do_it = true;
				bool is_map_op = false;
				if (op_flag == true && do_it == true) {
					cv_op.notify_one();
					std::cout << "notifying run thread for map/reduce\n";
					op_flag = false;
					if (!map_complete)
						is_map_op = true;
					else
						is_map_op = false;
					cv_op.wait(lk, [this] { return op_flag; });
					op_flag = false;
					std::cout << "finished notifying run thread for map/reduce and completed heartbeat too perhaps\n";
				}
				if (map_complete && is_map_op)
					return;
				if (reduce_complete && !is_map_op)
					return;
			}
			std::cout << "back in completion queue \n";

/*
			if(!map_complete && n_mapper_messages == n_shards) {
				{
					
					// ensure op (map) is complete
					std::unique_lock<std::mutex> lk(m_op);
					std::cout << __func__ << ": Notifying end of map\n";
					cv_op.notify_one();
					cv_op.wait(lk, [this] { return map_complete; });
				}
				if (map_complete)
					return;
			}
			// in reduce and we have received all the files
			if(map_complete && output_files.size() == n_partitions) {
				{
					// ensure op (reduce) is complete
					std::cout << __func__ << ": Notifying end of reduce\n";
					std::unique_lock<std::mutex> lk(m_op);
					cv_op.notify_one();
					cv_op.wait(lk, [this] { return reduce_complete; });
				}
				if (reduce_complete)
					return;
			}
*/
		}
}

void WorkerClient::send_heartbeat_msg(std::string worker_port, CompletionQueue *cq2_)
{
	// Connection timeout in seconds
	unsigned int timeout = 1;

	auto call = new HeartbeatCall;
	call->id = 0;

	// Set timeout for API
	std::chrono::system_clock::time_point deadline =
		std::chrono::system_clock::now() + std::chrono::seconds(timeout);

	call->context.set_deadline(deadline);

//	std::cout << __func__ << ": sending heartbeat message to " << worker_port << "\n";

	HeartbeatQuery query;
	query.set_id(worker_port);

	{
		std::unique_lock<std::mutex> lk(m);
		call->rpc = stub_->PrepareAsyncheartbeat(&call->context, query, cq2_);
		call->worker_ip_addr = worker_port;
		call->is_map_job = false;
		call->rpc->StartCall();
		call->rpc->Finish(&call->result, &call->status, (void*)call);
	}
}


std::string recv_heartbeat_msg(std::string in, CompletionQueue *cq2_, bool *is_error)
{
	void* got_tag;
	bool ok = false;
	std::string ret;

	GPR_ASSERT(cq2_->Next(&got_tag, &ok));
	HeartbeatCall* call = static_cast<HeartbeatCall*>(got_tag);
	//GPR_ASSERT(ok);
	if (!call->status.ok()) {
		std::cout << __func__ << ": recv error: " << call->status.error_message() << " for worker " << call->worker_ip_addr << "\n";
		*is_error = true;
		ret = call->worker_ip_addr;
	} else {
		ret = call->result.id();
//		std::cout << __func__ << ": received heartbeat message from " << ret << "\n";
		*is_error = false;
	}
	
	delete call;
	return ret;
}

void Master::handle_missing_worker(std::string worker_ip)
{

	if (!map_complete) {
		std::cout << __func__ << ": Map Worker " << worker_ip << " has gone missing\n";
		{
			std::lock_guard<std::mutex> lk(m);
			auto client = ip_addr_to_worker[worker_ip].get();
			if (!client) {
				std::cout << __func__ << ": : " << worker_ip << " already died\n";
				return;
			}
			std::cout << __func__ << ": : " << worker_ip << " " << client->map_shards.size() << "\n";
			for (int i = 0; i < client->map_shards.size(); i++)
				rem_shards.push_back(client->map_shards[i]);
			std::cout << __func__ << ": shards added: " << client->map_shards.size() << "; remaining shards: " << rem_shards.size() << "\n";

			busy_workers.erase(worker_ip);
			ip_addr_to_worker.erase(worker_ip);
			for (auto ip = worker_ips.begin(); ip != worker_ips.end(); ip++) {
				if (strcmp(worker_ip.c_str(), (*ip).c_str()) == 0) {
					std::cout << __func__ << ": Deleting from worker_ip list : " << worker_ip << "\n";
					worker_ips.erase(ip);
					break;
				}
			}

			int pos = worker_ip.find_first_of(':') + 1;
			std::string interm_file_substr = worker_ip.substr(pos);
			for (auto file = interm_files.begin(); file != interm_files.end(); file++) {
				if (file->find(interm_file_substr) != std::string::npos) {
					std::cout << __func__ << ": Removing interm file " << file->c_str() << "\n";
					std::remove(file->c_str());
					interm_files.erase(*file);
				}
			}
			dropped_worker_ips.push_back(worker_ip);
			n_workers--;
		}
	} else {

		std::cout << __func__ << ": Reduce Worker " << worker_ip << " has gone missing\n";
		{
			std::lock_guard<std::mutex> lk(m);
			auto client = ip_addr_to_worker[worker_ip].get();
			for (int i = 0; i < client->reduce_partitions.size(); i++) {
				rem_partitions.push_back(client->reduce_partitions[i]);
				std::string output_file = output_dir + "/" + std::to_string(client->reduce_partitions[i]);
				std::remove(output_file.c_str());
				std::cout << __func__ << ": Worker " << worker_ip << " output file " << output_file << " deleted\n";
			}
			std::cout << __func__ << ": partitions added: " << client->reduce_partitions.size() << "; remaining partitions: " << rem_partitions.size() << "\n";

			n_reducer_messages -= client->reduce_partitions.size();

			busy_workers.erase(worker_ip);
			ip_addr_to_worker.erase(worker_ip);
			for (auto ip = worker_ips.begin(); ip != worker_ips.end(); ip++) {
				if (strcmp(worker_ip.c_str(), (*ip).c_str()) == 0) {
					std::cout << __func__ << ": Deleting from worker_ip list : " << worker_ip << "\n";
					worker_ips.erase(ip);
					break;
				}
			}
			dropped_worker_ips.push_back(worker_ip);
			n_workers--;
		}

	}

	return;
}

void Master::heartbeat() {
	// async wait for responses and check worker id
	// if all workers responded sleep for heartbeat interval
	// if one or many workers did not respond with "heartbeat interval",
	// signal who and figure out the rest
	//
	int n = 0, wait_time = 1;
	CompletionQueue cq2_;

	kill_heartbeat = false;

	while (!kill_heartbeat) {
		std::map<std::string, bool> msg_rcvd;

		auto start = std::chrono::system_clock::now();

		//std::cout << __func__ << ": sending all heartbeats. workers: " << n_workers << "\n";
		// iterate over workers
		// async send message to each worker
		for (auto ip = worker_ips.begin(); ip != worker_ips.end(); ip++) {
			auto client = ip_addr_to_worker[*ip].get();
			client->send_heartbeat_msg(*ip, &cq2_);
			msg_rcvd[*ip] = false;
		}

		//std::cout << __func__ << ": sent all heartbeats\n";

		// iterate over workers
		// async wait for responses and check worker id
		for (int i = 0; i < n_workers; i++) {
			bool error = false;
			auto client = ip_addr_to_worker[worker_ips[i]].get();
			std::string recv_worker_ip = recv_heartbeat_msg(worker_ips[i], &cq2_, &error);
			if (error == false) {
				msg_rcvd[recv_worker_ip] = true;
			} else {
				msg_rcvd[recv_worker_ip] = false;
				handle_missing_worker(recv_worker_ip);
			}
		}

		std::cout << __func__ << ": received all heartbeats and/or errors\n";

		// heartbeat was scheduled. inform end of heartbeat
		if (trigger_heartbeat == true) {
			{
				std::unique_lock<std::mutex> lk(m_hbt);
				trigger_heartbeat = false;
				std::cout << __func__ << __func__ << ": Notifying heartbeat request\n";
				cv_hbt.notify_one();
			}
		}

		// determine current time
		auto end = std::chrono::system_clock::now();
		long start_val = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count();
		long end_val = std::chrono::duration_cast<std::chrono::milliseconds>(end.time_since_epoch()).count();

		// sleep if time spent less than heartbeat timeout
		long diff = end_val - start_val;
		if (diff < 1000) {
			//std::cout << __func__ << ": Heartbeat thread sleeping " << n << " " << start_val << " " << end_val << " " << diff << "\n";
			{
				// conditional sleep to wakeup thread when schedule_heartbeat() is called
				std::unique_lock<std::mutex> lk(m_hbt);
				cv_hbt.wait(lk, [this] {return trigger_heartbeat;});
				//cv_hbt.wait_for(lk, std::chrono::milliseconds(1001 - diff), [this] { return trigger_heartbeat; });
			}
			std::cout << __func__ << ": Heartbeat thread waking up " << n << " : " << trigger_heartbeat << " " << kill_heartbeat << "\n";
		}
	}

	// heartbeat was scheduled. inform end of heartbeat
	if (trigger_heartbeat == true) {
		{
			std::unique_lock<std::mutex> lk(m_hbt);
			trigger_heartbeat = false;
			cv_hbt.notify_one();
		}
	}
	std::cout << __func__ << ": Heartbeat done\n";
	return;
}
