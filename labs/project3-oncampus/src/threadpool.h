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

// structure that contains variables to make asynchronous call
struct VendorCall
{
	BidReply reply;			// memory to store reply
	Status status;
	ClientContext context;
	std::unique_ptr<grpc::ClientAsyncResponseReader<BidReply>> rpc;
};

// structure to hold task input, output, id and a semaphore for events
struct Task
{
	std::string query;
	int task_id;
	sem_t sem;
	std::vector<VendorCall *> replies;
};

class ThreadPool {
	private:
		int num_threads;
		std::vector<std::pair<pthread_t, int>> workers;
		std::vector<std::shared_ptr<Channel>> channels;
		std::queue<struct Task*> tasks;
		std::map<int, int> done;
		pthread_mutex_t mutex;		// for tasks
		pthread_cond_t cond;		// for tasks
		int task_cnt;

	public:
		ThreadPool(int nthreads, std::vector<std::string>);
		~ThreadPool();
		Task* addTask(const std::string& query);
		void runTask(Task *t);
		void getThreadEvent(Task *t, ProductReply *reply);
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
		~VendorClient();
		void RequestProductBid(const std::string& product_name, std::vector<VendorCall *> &calls);
		void WaitForProductBidReply();

	private:
		CompletionQueue cq;
		std::unique_ptr<Vendor::Stub> clientStub;
		struct VendorCall *call;

};
