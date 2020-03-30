#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "mpi.h"
#include "omp.h"

#define DEBUG	0

#if DEBUG
#define debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define debug(fmt, ...) ((void)0)
#endif

#define NUM_ITERATIONS 1000

/* Few defaults to handle max cases */
#define NUM_ROUNDS     32
#define NUM_THREADS    32

//bool gflags[NUM_THREADS][NUM_ROUNDS];
int gflags[NUM_THREADS][NUM_ROUNDS];

int nprocesses;	// Number of processes
int nthreads;	// Number of threads
int ndrounds;	// Max number of rounds for dissemination barrier

/* Structure used to maintain barrier status for each thread */
struct node {
	bool flags[NUM_ROUNDS];	// flags used for waiting / wakeup
	bool sense;				// local sense
	int numrounds;			// number of rounds this thread will participate in
	int parent_tid;			// thread ID of parent that will wakeup this thread
};

/* Tournament barrier init - Determine static barrier information
 *
 * @barriers	: pointer to array of barriers (shared)
 * @rank		: rank of current process
 */
void tournament_barrier_init(struct node *barriers, int rank)
{
	int tid, p, rounds, pow2threads;

	double log2threads = log(nthreads) / log(2);	// log2(nthreads)

	pow2threads = pow(2, (int)ceil(log2threads));	// determine closest number of threads which is a power of 2
	int nrounds = (int)ceil(log2threads);				// determine max number of rounds for barrier

	debug("rank %d, nthreads %d, pow2threads %d, maxrounds %d, %d\n",
		rank, nthreads, pow2threads, nrounds, barriers[0].parent_tid);

	/* Initialize static info for each thread's barrier */
	for (tid = 0; tid < nthreads; tid++) {

		/* Determine number of rounds (0-indexed) each tournament will participate
		 * For a system with 8 threads, threads 1,3,5,7 will participate in 1 round (numrounds = 0)
		 * threads 2 and 6 in 2 rounds (numrounds = 1), thread 4 in 3 rounds (numrounds = 2)
		 */
		p = (pow2threads | tid);
		rounds = 0;
		while (1) {
			if (p & 0x1)
				break;
			p = p >> 1;
			rounds++;
		}
		barriers[tid].numrounds = rounds;
		barriers[tid].sense = false;

		/* Determine parent for thread. Parent(1) = 0, Parent(2) = 0, Parent(3) = 2 etc. */
		barriers[tid].parent_tid = tid & ~((1 << (rounds + 1)) - 1);

		debug("rank %d tid %d for %d rounds (parent %d)\n",
			rank, tid, barriers[tid].numrounds, barriers[tid].parent_tid);
	}
	return;
}


/* Dissemination Barrier implementation
 *
 * @rank	: rank of current process
 */
void dissemination_barrier(int rank)
{
	int round = 0;
	int ret, recv_rank, send_rank, pow2;
	bool msg = true;
	MPI_Status status;

    debug("rank %d dissemination rounds %d\n", rank, ndrounds);
    
	pow2 = 1;
	for (round = 0; round < ndrounds; round++) {

		/* Determine rank of processes to send msg to and recv msg from */
		send_rank = (rank + pow2) % nprocesses;
		recv_rank = (rank + nprocesses - pow2) % nprocesses;

		/* Send message to target */
		debug("rank %d send msg to rank %d in round %d\n", rank, send_rank, round);
		ret = MPI_Send(&msg, 1, MPI_BYTE, send_rank, 1, MPI_COMM_WORLD);

		/* Recv message from source */
		ret = MPI_Recv(&msg, 1, MPI_BYTE, recv_rank, 1, MPI_COMM_WORLD, &status);
		debug("rank %d recv msg from rank %d in round %d\n", rank, recv_rank, round);

		pow2 = pow2 << 1;
	}
	return;
}




/* Tourmination (Tournament + Dissemination) barrier implementation
 *
 * @rank		: rank of current process
 * @tid			: thread id (private)
 * @barriers	: pointer to array of barriers (shared)
 */
//void tourmination_barrier(int rank, int tid, struct node *barriers)
void tourmination_barrier(int rank, int tid, int parent_tid, int numrounds, int *local_sense)
{
	int r, child_tid;

	//bool sense = !barriers[tid].sense;	// local sense
	int sense = *local_sense ? 0 : 1;	// local sense
    
	/* Arrival */
	for (r = 0; r < numrounds; r++) {

		/* If child tid > number of threads, skip arrival wait */
		child_tid = tid | (1 << r);
		if (child_tid >= nthreads) {
			gflags[tid][r] = sense;
			//barriers[tid].flags[r] = sense;
			continue;
		}

		/* Wait for child thread to indicate arrival to parent */
		debug("rank %d thread %d: waiting for arrival of child_tid %d\n", rank, tid, child_tid);
		while (gflags[tid][r] != sense);
	}

	/* Winner of tournament is 0. Everyone else informs arrival
	 * to parent and waits for parent to wakeup */
	if (tid != 0) {
		debug("rank %d thread %d: arrived, informing parent_tid %d. now waiting for wakeup\n", rank, tid, parent_tid);
		//barriers[parent_tid].flags[r] = sense;
		//while (barriers[tid].flags[r] != sense);
		gflags[parent_tid][r] = sense;
		while (gflags[tid][r] != sense);
	}
	/* Thread 0 will communicate with other processes to check if
	 * everyone has reached the barrier */
	else {
        debug("-----> rank %d dissemination begin\n", rank);

		dissemination_barrier(rank);
        debug("-----> rank %d dissemination complete. wakeup rounds %d\n", rank, r);
	}


	/* Wakeup */
	while (r-- > 0) {

		/* If child tid > number of threads, skip wakeup */
		child_tid = tid | (1 << r);
		if (child_tid >= nthreads)
			continue;

		/* Wakeup child */
		debug("rank %d thread %d: waking up child_tid %d\n", rank, tid, child_tid);
		//barriers[child_tid].flags[r] = sense;
		gflags[child_tid][r] = sense;
	}

	/* Update thread's barrier sense */
	//barriers[tid].sense = sense;
	*local_sense = sense;

	return;

}

void foo(int n)
{
	int i, j, k = 0;
	for (i = 0; i < 1000; i++) {
		for (j = 0; j < n; j++)
			k += i * j;
	}
}

int main(int argc, char *argv[])
{
	int rank, len;
	char hostname[MPI_MAX_PROCESSOR_NAME];
	double log2rounds;
	int num_iters = NUM_ITERATIONS;
	struct node barriers[NUM_THREADS];

    
    double total_time = 0;
	/* MPI initialization */
	MPI_Init(&argc, &argv);

	/* Determine number of tasks and rank of current task */
	MPI_Comm_size(MPI_COMM_WORLD, &nprocesses);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Get_processor_name(hostname, &len);

	/* Determine number of rounds for dissemination barrier */
	log2rounds = log(nprocesses) / log(2);
	ndrounds = ceil(log2rounds);

	/* Default 8 threads */
	nthreads = 8;

	/* Get number of threads and iterations */
	if (argc > 1) {
		nthreads = atoi(argv[1]);
		if (argc > 2)
			num_iters = atoi(argv[2]);
	}

	omp_set_num_threads(nthreads);

	/* Initialize barrier structure for tournament */
	memset(barriers, 0, sizeof(barriers));
	tournament_barrier_init(barriers, rank);

	#pragma omp parallel default(shared)
	{
		int n, tid = omp_get_thread_num();

		struct timeval start, end;
		double time1, time2, timediff = 0;
 		double time_taken; 
        
		int parent_tid = barriers[tid].parent_tid;
		int numrounds = barriers[tid].numrounds;
		int sense = 0;
		//bool sense = false;

		//gettimeofday(&start, NULL);
        time1 = MPI_Wtime();
        //time1 = omp_get_wtime();


		for (n = 0; n < num_iters; n++) {
			debug("process %d thread %d, barrier: %d\n", rank, tid, n);
			tourmination_barrier(rank, tid, parent_tid, numrounds, &sense);
			//foo(100*n);
			//debug("process %d thread %d, barrier: %d\n", rank, tid, n);
			//time1 = MPI_Wtime();
			//tourmination_barrier(rank, tid, barriers);
			//time2 = MPI_Wtime();
			//timediff += (time2 - time1);
		}

        //time2 = omp_get_wtime();
		time2 = MPI_Wtime();
		//gettimeofday(&end, NULL);

		//tourmination_barrier(rank, tid, barriers);
		//tourmination_barrier(rank, tid, parent_tid, numrounds, &sense);


		/* Calculate average time per barrier */
		timediff = ((time2 - time1) / num_iters);
  
		//time_taken = (end.tv_sec - start.tv_sec) * 1e6; 
		//time_taken = (time_taken + (end.tv_usec - start.tv_usec)) * 1;//e3; 
		//time_taken = time_taken / num_iters;

        //printf("rank %d tid %d timediff %f\n", rank, tid, time2-time1);
		#pragma omp critical
		total_time += timediff;
	}
    double average_time = (total_time / nthreads) * 1e9;
    printf("Avg time across all barrier %f ns\n", average_time);
    double avg_barrier_time = 0.0;
    MPI_Reduce(&average_time, &avg_barrier_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    if(!rank)
        printf("Avg Barrier time across all processes %f", avg_barrier_time/nprocesses);
    
	MPI_Finalize();
}
