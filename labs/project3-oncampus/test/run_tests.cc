#include "product_queries_util.h"

#include <thread>
#include <algorithm>

extern bool run_client(const std::string& server_addr, const std::string& product_name, ProductQueryResult&);

bool run_test(std::vector<ProductSpec>& product_specs, const int num_max_threads, const std::string& server_addr);

void thread_task(std::string server_addr, ProductSpec& p_spec, ProductQueryResult& pq_result,  int query_id, bool& is_done);

int main(int argc, char** argv) {
  
  int num_max_threads;
  std::string server_addr;
  if (argc == 3) {
    server_addr = std::string(argv[1]);
    num_max_threads = std::min( 20, std::max(0,atoi(argv[2])));
  }
  else if (argc == 2) {
    server_addr = std::string(argv[1]);
    num_max_threads = 4;
  }
  else {
    server_addr = "0.0.0.0:50053";
    num_max_threads = 4;
  }

  std::vector<ProductSpec> product_specs;
  const std::string filename = "product_query_list.txt";
  if (!read_product_queries(product_specs, filename)) {
  	std::cerr << "Failed to extract product queries from: " << filename << std::endl;
  	return EXIT_FAILURE;
  }

  return run_test(product_specs, num_max_threads, server_addr) 
      ? EXIT_SUCCESS : EXIT_FAILURE;
}


bool warm_up(bool & is_done){
  is_done = true;
}

bool run_test(std::vector<ProductSpec>& product_specs, const int num_max_threads, const std::string& server_addr) {
  bool threads_is_done[num_max_threads];
  std::fill_n(threads_is_done, num_max_threads, false);
  std::vector<std::thread*> threads(num_max_threads, NULL);

  for (int i = 0; i < num_max_threads; ++i) {
    threads[i] = new std::thread(warm_up, std::ref(threads_is_done[i]));
  }
  
  std::vector<ProductQueryResult> test_results(product_specs.size());

  size_t thread_ind = 0;
  for (int i = 0; i < product_specs.size(); ++i) {
    while (!threads_is_done[thread_ind]) {
      thread_ind = (thread_ind + 1) % num_max_threads;
    }
    
    threads[thread_ind]->join();
    delete threads[thread_ind];
    
    threads[thread_ind] = new std::thread(thread_task, 
          server_addr, std::ref(product_specs[i]), std::ref(test_results[i]), i, std::ref(threads_is_done[i]));
    
    thread_ind = (thread_ind + 1) % num_max_threads;
  }

  for (int i = 0; i < num_max_threads; ++i) {
    threads[i]->join();
  }

  for (int i = 0; i < test_results.size(); ++i) {
    const auto bids = test_results[i].bids_;
    if (bids.size()) {
      std::cout << "\n\nQuery id: " << i << ", product: " << product_specs[i].name_ << " is -->  ";
      for (const auto bid : bids) {
        std::cout << "(price: " << bid.price_ << ", vendor_id: " << bid.vendor_id_ << ") ";
      }
    }
    else {
      std::cout << "\n\nQuery id: " << i << ", product: " << product_specs[i].name_ << " -->  Didn't receive any bids";
    }
  }
  std::cout << std::endl;
  
  return true;
}

void thread_task(const std::string server_addr, ProductSpec& p_spec, ProductQueryResult& pq_result, int query_id, bool& is_done) {

  if (!run_client(server_addr, p_spec.name_, pq_result)) {
    std::cout << "\nStore failed to receive reply for: " << p_spec.name_ << ", query id: " << query_id <<  std::endl;
  }
}

