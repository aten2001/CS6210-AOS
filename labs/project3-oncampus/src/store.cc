/* Reference: https://github.com/grpc/grpc/blob/master/examples/cpp/helloworld/
*/
#include "threadpool.h"

#include <iostream>
#include <string>
#include <fstream>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;
using store::ProductInfo;
using store::ProductQuery;
using store::ProductReply;
using store::Store;

//Vendor related stuff
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Channel;
using vendor::BidQuery;
using vendor::BidReply;
using vendor::Vendor;

using namespace std;

class StoreImpl
{
	public:
		StoreImpl(std::vector<std::string> vendors, std::string launch_addr, int num_threads) 
			:vendors_(vendors), launch_addr_(launch_addr)
		{
			cout << "storeimpl constructor\n";
			pool = new ThreadPool(num_threads, vendors);
		}
		~StoreImpl()
		{
			server_->Shutdown();
			// Always shutdown the completion queue after the server.
			cq_->Shutdown();
		}

		// There is no shutdown handling in this code.
		void Run()
		{
				ServerBuilder builder;

				// Listen on the given address without any authentication mechanism.
				builder.AddListeningPort(launch_addr_, grpc::InsecureServerCredentials());
				// Register "service_" as the instance through which we'll communicate with
				// clients. In this case it corresponds to an *asynchronous* service.
				builder.RegisterService(&service_);
				// Get hold of the completion queue used for the asynchronous communication
				// with the gRPC runtime.
				cq_ = builder.AddCompletionQueue();
				// Finally assemble the server.
				server_ = builder.BuildAndStart();
				std::cout << "Server listening on " << launch_addr_ << std::endl;

				// Proceed to the server's main loop.
				HandleRpcs();
		}
	private:
		// Class encompasing the state and logic needed to serve a request.
		class CallData {
			public:
				// Take in the "service" instance (in this case representing an asynchronous
				// server) and the completion queue "cq" used for asynchronous communication
				// with the gRPC runtime.
				CallData(Store::AsyncService* service, ServerCompletionQueue* cq, std::vector<std::string> vendors, ThreadPool *pool)
					: service_(service), cq_(cq), responder_(&ctx_), status_(CREATE), vendors_(vendors), pool(pool)
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
						new CallData(service_, cq_, vendors_, pool);

						// The actual processing.
						// Call the vendor to get all the product info

						Task *t = pool->addTask(request_.product_name());

						pool->getThreadEvent(t, &reply_);

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
				enum CallStatus { CREATE, PROCESS, FINISH };
				CallStatus status_;  // The current serving state.
				std::vector<std::string> vendors_;
		};

		// This can be run in multiple threads if needed.
		void HandleRpcs()
		{
			// Spawn a new CallData instance to serve new clients.
			new CallData(&service_, cq_.get(), vendors_, pool);
			void* tag;  // uniquely identifies a request.
			bool ok;
			while (true) {
				// Block waiting to read the next event from the completion queue. The
				// event is uniquely identified by its tag, which in this case is the
				// memory address of a CallData instance.
				// The return value of Next should always be checked. This return value
				// tells us whether there is any kind of event or cq_ is shutting down.
				GPR_ASSERT(cq_->Next(&tag, &ok));
				GPR_ASSERT(ok);
				static_cast<CallData*>(tag)->Proceed();
			}
		}

		std::unique_ptr<ServerCompletionQueue> cq_;
		Store::AsyncService service_;
		std::unique_ptr<Server> server_;
		std::vector<std::string> vendors_;
		std::string launch_addr_;
		ThreadPool *pool;
};

int main(int argc, char** argv)
{
	std::string launch_addr("0.0.0.0:50053");
	std::string filename = "vendor_addresses.txt";
	int num_threads = 8;

	if (argc == 4) {
		launch_addr = std::string(argv[1]);
		num_threads = atoi(argv[2]);
		filename = std::string(argv[3]);
	} if (argc == 3) {
		launch_addr = std::string(argv[1]);
		num_threads = atoi(argv[2]);
	} else if (argc == 2) {
		launch_addr = std::string(argv[1]);
	}
	// Read the vendor ports
	std::vector<std::string> ip_addrresses;
	std::ifstream myfile(filename);
	if(myfile.is_open()) {
		std::string ip_addr;
		while(getline(myfile, ip_addr)) {
			ip_addrresses.push_back(ip_addr);
			std::cout << "Here-> " << ip_addr << "\n";
		}
		myfile.close();
	} else {
		std::cerr << "Failed to open file " << filename << std::endl;
		return EXIT_FAILURE;
	}
	StoreImpl store(ip_addrresses, launch_addr, num_threads);
	store.Run();
	return 0;
}

