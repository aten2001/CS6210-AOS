#include <memory>
#include <iostream>
#include <functional>

#include <grpc++/grpc++.h>

#include "vendor.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using vendor::BidQuery;
using vendor::BidReply;
using vendor::Vendor;

class VendorService final : public Vendor::Service {

  public:
    VendorService(const std::string& server_address) : id_("Vendor_" + server_address) {}

  private:
    Status getProductBid(ServerContext* context, const BidQuery* request,
                    BidReply* reply) override {
      reply->set_price(hasher_(id_ + request->product_name()) % 100);
      reply->set_vendor_id(id_);
      return Status::OK;
    }

    std::hash<std::string> hasher_;
    const std::string id_;
};

void run_server(const std::string server_address)   {
  std::string server_addressess(server_address);
  VendorService service(server_address);

  ServerBuilder builder;
  builder.AddListeningPort(server_addressess, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_addressess << std::endl;

  server->Wait();
}