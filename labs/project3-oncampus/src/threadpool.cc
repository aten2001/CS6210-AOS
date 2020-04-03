
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

	std::cout << "creating " << nthreads << " threads\n";

	task_cnt = 0;
	num_threads = nthreads;

    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);

	std::vector<std::string>::iterator it;
	for (it = vendors.begin(); it != vendors.end(); it++) {
		std::shared_ptr<Channel> ch = grpc::CreateChannel(*it, grpc::InsecureChannelCredentials());
		channels.push_back(ch);
		std::cout << "creating channel for vendor at " << *it << "\n";
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

Task* ThreadPool::addTask(const std::string& query)
{
	int task_id;
	Task *t = new Task;

	pthread_mutex_lock(&mutex);

	// initialize task structure
	t->task_id = task_cnt++;
	t->query = query;
	sem_init(&t->sem, 0, 0);
	tasks.push(t);

	pthread_mutex_unlock(&mutex);

	pthread_cond_signal(&cond);

	std::cout << "Added task (" << t->task_id << ", " << query << ") to tasklist\n";

	return t;
}

void ThreadPool::runTask(Task *t)
{
	int i = 0;
	std::string query(t->query);
	std::cout << "Thread " << thread_id << ": Running query: " << query << ", task_id: " << t->task_id << "\n";

	std::vector<VendorClient *>::iterator it;
	std::vector<VendorCall *> calls;

	// Send bid request to vendor
	i = 0;
	for (it = vendor_clients.begin(); it != vendor_clients.end(); ++it) {
		//std::cout << "Thread " << thread_id << ": Sending Bid Request to vendor " << i++ << "\n";
		VendorClient *vc = *it;
		vc->RequestProductBid(query, calls);
	}

	// Wait for vendor responses
	i = 0;
	for (it = vendor_clients.begin(); it != vendor_clients.end(); ++it) {
		//std::cout << "Thread " << thread_id << ": Waiting for Bid Reply from vendor " << i++ << "\n";
		VendorClient *vc = *it;
		vc->WaitForProductBidReply();
	}

	// Inform waiting thread of replies
	t->replies = calls;
	sem_post(&t->sem);
	std::cout << "Thread " << thread_id << ": Completed query: " << query << ". Sem posting task " << t->task_id << "\n";

	return;
}



void ThreadPool::getThreadEvent(Task *t, ProductReply *reply)
{
	std::cout << "Task " << t->task_id << " waiting on semaphore\n";
	sem_wait(&t->sem);		
	std::cout << "Task " << t->task_id << " done waiting on semaphore. Query : " << t->query << "\n";

	std::vector<VendorCall *>::iterator it;

	for (it = t->replies.begin(); it != t->replies.end(); ++it) {
		VendorCall *call = *it;

		ProductInfo* product_info = reply->add_products();
		product_info->set_price(call->reply.price());
		product_info->set_vendor_id(call->reply.vendor_id());
		//std::cout << "Bid: (" << t->query << ", " << call->reply.vendor_id() << ", " << call->reply.price() << ")" << std::endl;

		delete call;
	}
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

