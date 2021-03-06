
#include "threadpool.h"

#define FREE	0
#define BUSY	1

static thread_local int state = FREE;
static thread_local std::vector<VendorClient *> vendor_clients;
static thread_local int thread_id;

using grpc::Status;

ThreadPool::ThreadPool(int nthreads, std::vector<std::string> vendors)
{
	int i;


	task_cnt = 0;
	num_threads = nthreads;

	pthread_cond_init(&cond, NULL);
	pthread_mutex_init(&mutex, NULL);

	std::vector<std::string>::iterator it;
	for (it = vendors.begin(); it != vendors.end(); it++) {
		std::shared_ptr<Channel> ch = grpc::CreateChannel(*it, grpc::InsecureChannelCredentials());
		channels.push_back(ch);
	}

	pthread_mutex_lock(&mutex);
	for (i = 0; i < nthreads; i++) {
		pthread_t tid;
		pthread_create(&tid, NULL, &runWorkerThread, (void *)this);
		workers.push_back(std::pair<pthread_t, int>(tid, i));
	}
	pthread_mutex_unlock(&mutex);
}

ThreadPool::~ThreadPool()
{
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}

void ThreadPool::runWorker(void *tp)
{
	// Create channels to each vendor
	std::vector<std::shared_ptr<Channel>>::iterator it;
	for (it = channels.begin(); it != channels.end(); it++) {
		VendorClient *vc = new VendorClient(*it);
		vendor_clients.push_back(vc);
	}

	// get thread ID
	pthread_t tid = pthread_self();
	std::vector<std::pair<pthread_t, int>>::iterator w;
	std::pair<pthread_t, int> p;

	pthread_mutex_lock(&mutex);
	for (w = workers.begin(); w != workers.end(); w++) {
		p = *w;
		if (p.first == tid) {
			thread_id = p.second;
			break;
		}
	}
	pthread_mutex_unlock(&mutex);

	// schedule thread to task
	while (true) {
		state = FREE;
		pthread_mutex_lock(&mutex);

		while (tasks.empty())
			pthread_cond_wait(&cond, &mutex);

		Task *task = tasks.front();
		tasks.pop();
		state = BUSY;

		pthread_mutex_unlock(&mutex);

		runTask(task);
	}

	return;
}

void ThreadPool::addTask(Task *t)
{
	int task_id;

	pthread_mutex_lock(&mutex);

	t->task_id = task_cnt++;

	tasks.push(t);

	pthread_mutex_unlock(&mutex);

	pthread_cond_signal(&cond);


	return;
}

void ThreadPool::runTask(Task *t)
{
	int i = 0;
	std::string query(t->query);
	
	std::vector<VendorClient *>::iterator it;
	std::vector<VendorCall *> calls;

	ProductReply reply;

	// Send bid request to vendor
	i = 0;
	for (it = vendor_clients.begin(); it != vendor_clients.end(); ++it) {
		VendorClient *vc = *it;
		vc->RequestProductBid(query, calls);
	}

	// Wait for vendor responses
	i = 0;
	for (it = vendor_clients.begin(); it != vendor_clients.end(); ++it) {
		VendorClient *vc = *it;
		vc->WaitForProductBidReply();
	}


	std::vector<VendorCall *>::iterator it_vc;
	for (it_vc = calls.begin(); it_vc != calls.end(); ++it_vc) {
		VendorCall *call = *it_vc;

		ProductInfo* product_info = reply.add_products();
		product_info->set_price(call->reply.price());
		product_info->set_vendor_id(call->reply.vendor_id());

		delete call;
	}

	t->cdata->SetReply(reply);
	t->cdata->Proceed();


	delete t;

	return;
}



VendorClient::VendorClient(std::shared_ptr<Channel> channel)
	: clientStub(Vendor::NewStub(channel))
{
}

VendorClient::~VendorClient()
{
}

void VendorClient::RequestProductBid(const std::string& product_name, std::vector<VendorCall *> &calls)
{
	BidQuery request;
	request.set_product_name(product_name);

	call = new VendorCall;
	call->rpc = clientStub->PrepareAsyncgetProductBid(&call->context, request, &cq);
	call->rpc->StartCall();
	call->rpc->Finish(&call->reply, &call->status, (void*)1);

	calls.push_back(call);

	return;
}


void VendorClient::WaitForProductBidReply(void)
{
	Status status;
	void* got_tag;
	bool ok = false;

	GPR_ASSERT(cq.Next(&got_tag, &ok));
	GPR_ASSERT(got_tag == (void*)1);
	GPR_ASSERT(ok);

	return;
}

