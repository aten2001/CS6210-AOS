#pragma once

#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

#include <iostream>
#include <string>
#include <fstream>

#include "store.grpc.pb.h"
#include "vendor.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>

#include <vector>
#include <queue>

using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Channel;
using store::ProductInfo;
using store::ProductQuery;
using store::ProductReply;
using vendor::BidQuery;
using vendor::BidReply;
using vendor::Vendor;
using grpc::Status;

struct VendorCall
{
	BidReply reply;
	Status status;
	ClientContext context;
	std::unique_ptr<grpc::ClientAsyncResponseReader<BidReply>> rpc;
};

struct Task
{
	std::string query;
	int task_id;
	//pthread_cond_t cond;
	sem_t sem;
	std::vector<VendorCall *> replies;
};

class ThreadPool {
	private:
		int num_threads;
		std::vector<std::pair<pthread_t, int>> workers;
		std::vector<std::shared_ptr<Channel>> channels;
		//std::queue<std::pair<std::string, int>> tasks;
		std::queue<struct Task*> tasks;
		std::vector<struct Task*> fin_tasks;
		std::map<int, int> done;
		pthread_mutex_t mutex;
		pthread_cond_t cond;
		pthread_mutex_t r_mutex;
		pthread_cond_t r_cond;
		int task_cnt;

	public:
		ThreadPool(int nthreads, std::vector<std::string>);
		~ThreadPool();
		Task* addTask(const std::string& query);
		void runTask(Task *t);
		void getVendorReply(Task *t, ProductReply *reply);
		void runWorker(void *);

		static void* runWorkerThread(void *tp)
		{
			reinterpret_cast<ThreadPool *>(tp)->runWorker(tp);
			return 0;
		}
};

class VendorClient
{
	public:
		VendorClient(std::shared_ptr<Channel> channel);
		//void RequestProductBid(const std::string& product_name, BidReply *bid_reply);
		void RequestProductBid(const std::string& product_name, std::vector<VendorCall *> &calls);
		BidReply WaitForProductBidReply();

	private:
		std::unique_ptr<Vendor::Stub> clientStub;
		// Context for the client. It could be used to convey extra information to
		// the server and/or tweak certain RPC behaviors.
		//ClientContext context;
		struct VendorCall *call;

		// The producer-consumer queue we use to communicate asynchronously with the
		// gRPC runtime.
		CompletionQueue cq;

};

