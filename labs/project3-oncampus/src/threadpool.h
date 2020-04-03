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

//grpc stuff
using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Channel;
using grpc::Status;

//Store related stuff
using store::ProductInfo;
using store::ProductQuery;
using store::ProductReply;
using store::Store;

//Vendor related stuff
using vendor::BidQuery;
using vendor::BidReply;
using vendor::Vendor;

class CallData;

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
	int task_id;
	std::string query;
	CallData  *cdata;
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
		void addTask(Task *t);
		void runTask(Task *t);
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


class CallData {
	public:
		// Take in the "service" instance (in this case representing an asynchronous
		// server) and the completion queue "cq" used for asynchronous communication
		// with the gRPC runtime.
		CallData(Store::AsyncService* service, ServerCompletionQueue* cq, ThreadPool *pool)
			: service_(service), cq_(cq), responder_(&ctx_), status_(CREATE), pool(pool)
		{
			// Invoke the serving logic right away.
			Proceed();
		}

		void Proceed()
		{
			if (status_ == CREATE) {
				// Make this instance progress to the PROCESS state.
				status_ = PROCESS;

				service_->RequestgetProducts(&ctx_, &request_, &responder_, cq_, cq_, this);

			} else if (status_ == PROCESS) {
				// Spawn a new CallData instance to serve new clients while we process
				// the one for this CallData. The instance will deallocate itself as
				// part of its FINISH state.
				new CallData(service_, cq_, pool);

				// The actual processing.
				// Call the vendor to get all the product info
				Task *t = new Task;

				t->query = request_.product_name();
				t->cdata = this;
				pool->addTask(t);
				status_ = SERVICE;

			} else if (status_ == SERVICE) {
				// And we are done! Let the gRPC runtime know we've finished, using the
				// memory address of this instance as the uniquely identifying tag for
				// the event.
				status_ = FINISH;
				responder_.Finish(reply_, Status::OK, this);

			} else {
				GPR_ASSERT(status_ == FINISH);
				// Once in the FINISH state, deallocate ourselves (CallData).
				delete this;
			}
		}

		void SetReply(ProductReply reply)
		{
			reply_ = reply;
		}

	private:
		// The means of communication with the gRPC runtime for an asynchronous
		// server.
		Store::AsyncService* service_;
		// The producer-consumer queue where for asynchronous server notifications.
		ServerCompletionQueue* cq_;
		// Context for the rpc, allowing to tweak aspects of it such as the use
		// of compression, authentication, as well as to send metadata back to the
		// client.
		ServerContext ctx_;

		// What we get from the client.
		ProductQuery request_;
		// What we send back to the client.
		ProductReply reply_;

		ThreadPool *pool;

		// The means to get back to the client.
		ServerAsyncResponseWriter<ProductReply> responder_;

		// Let's implement a tiny state machine with the following states.
		enum CallStatus { CREATE, PROCESS, SERVICE, FINISH };
		CallStatus status_;  // The current serving state.
};
