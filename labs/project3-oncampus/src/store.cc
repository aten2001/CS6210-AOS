/* Reference: https://github.com/grpc/grpc/blob/master/examples/cpp/helloworld/
*/
#include "threadpool.h"

#include <iostream>
#include <string>
#include <fstream>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>

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
			cq_->Shutdown();
			delete pool;
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
		// This can be run in multiple threads if needed.
		void HandleRpcs()
		{
			// Spawn a new CallData instance to serve new clients.
			new CallData(&service_, cq_.get(), pool);
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
		while(getline(myfile, ip_addr))
			ip_addrresses.push_back(ip_addr);
		myfile.close();
	} else {
		std::cerr << "Failed to open file " << filename << std::endl;
		return EXIT_FAILURE;
	}
	StoreImpl store(ip_addrresses, launch_addr, num_threads);
	store.Run();
	return 0;
}

