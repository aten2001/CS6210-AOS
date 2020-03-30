#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>
#include <stdbool.h>

#include <sys/time.h>

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

int nthreads;	// Number of threads

//bool gflags[NUM_THREADS][NUM_ROUNDS];
int gflags[NUM_THREADS][NUM_ROUNDS];

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
 */
void tournament_barrier_init(struct node *barriers)
{
	int tid, p, rounds;

	double log2threads = log(nthreads) / log(2);		// log2(nthreads)

	int pow2threads = pow(2, (int)ceil(log2threads));	// determine closest number of threads which is a power of 2
	int nrounds = (int)ceil(log2threads);				// determine max number of rounds for barrier

	debug("nthreads %d, pow2threads %d, maxrounds %d, %d\n",
		nthreads, pow2threads, nrounds, barriers[0].parent_tid);

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

		debug("tid %d for %d rounds (parent %d)\n",
			tid, barriers[tid].numrounds, barriers[tid].parent_tid);
	}
	return;
}

#if 0
/* Tournament barrier implementation
 *
 * @barriers	: pointer to array of barriers (shared)
 * @tid			: thread id (private)
 */
void tournament_barrier(struct node *barriers, int tid)
{
	int r, child_tid;
	int parent_tid = barriers[tid].parent_tid;

	bool sense = !barriers[tid].sense;	// local sense

	/* Arrival */
	for (r = 0; r < barriers[tid].numrounds; r++) {

		/* If child tid > number of threads, skip arrival wait */
		child_tid = tid | (1 << r);
		if (child_tid >= nthreads) {
			//barriers[tid].flags[r] = sense;
			gflags[tid][r] = sense;
			continue;
		}

		/* Wait for child thread to indicate arrival to parent */
		debug("thread %d: waiting for arrival of child_tid %d\n", tid, child_tid);
		//while (barriers[tid].flags[r] != sense);
		while (gflags[tid] != sense);
	}

	/* Winner of tournament is 0. Everyone else informs arrival
	 * to parent and waits for parent to wakeup */
	if (tid != 0) {
		debug("thread %d: arrived, informing parent_tid %d. now waiting for wakeup\n", tid, parent_tid);
		//barriers[parent_tid].flags[r] = sense;
		//while (barriers[tid].flags[r] != sense);
		gflags[parent_tid][r] = sense;
		while (gflags[tid][r] != sense);
	}

	/* Wakeup */
	while (r-- > 0) {

		/* If child tid > number of threads, skip wakeup */
		child_tid = tid | (1 << r);
		if (child_tid >= nthreads)
			continue;

		/* Wakeup child */
		debug("thread %d: waking up child_tid %d\n", tid, child_tid);
		//barriers[child_tid].flags[r] = sense;
		gflags[child_tid][r] = sense;
	}

	/* Update thread's barrier sense */
	barriers[tid].sense = sense;

	return;
}
#endif

/* Tournament barrier implementation
 *
 * @barriers	: pointer to array of barriers (shared)
 * @tid			: thread id (private)
 */
void tournament_barrier(int tid, int parent_tid, int numrounds, int *local_sense)
{
	int r, child_tid;

	int sense = *local_sense ? 0 : 1;	// local sense

	/* Arrival */
	for (r = 0; r < numrounds; r++) {

		/* If child tid > number of threads, skip arrival wait */
		child_tid = tid | (1 << r);
		if (child_tid >= nthreads) {
			gflags[tid][r] = sense;
			continue;
		}

		/* Wait for child thread to indicate arrival to parent */
		debug("thread %d: waiting for arrival of child_tid %d\n", tid, child_tid);
		while (gflags[tid][r] != sense);
	}

	/* Winner of tournament is 0. Everyone else informs arrival
	 * to parent and waits for parent to wakeup */
	if (tid != 0) {
		debug("thread %d: arrived, informing parent_tid %d. now waiting for wakeup\n", tid, parent_tid);
		gflags[parent_tid][r] = sense;
		while (gflags[tid][r] != sense);
	}

	/* Wakeup */
	while (r-- > 0) {

		/* If child tid > number of threads, skip wakeup */
		child_tid = tid | (1 << r);
		if (child_tid >= nthreads)
			continue;

		/* Wakeup child */
		debug("thread %d: waking up child_tid %d\n", tid, child_tid);
		gflags[child_tid][r] = sense;
	}

	/* Update thread's barrier sense */
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

int main(int argc, char **argv)
{
	int num_iters = NUM_ITERATIONS;
	struct node barriers[NUM_THREADS];
	memset(barriers, 0, sizeof(barriers));

    
	double total_time = 0;

	/* Default 8 threads */
	nthreads = 8;

	/* Get number of threads and iterations */
	if (argc > 1) {
		nthreads = atoi(argv[1]);
		if (argc > 2)
			num_iters = atoi(argv[2]);
	}

	omp_set_num_threads(nthreads);

	/* Initialize barriers */
	tournament_barrier_init(barriers);

	/* Setting barriers as shared data */
	#pragma omp parallel default(shared) //shared(barriers) firstprivate(nthreads)
	{
		int n, tid = omp_get_thread_num();

		struct timeval start, end;
		double time1, time2, timediff = 0;
 		double time_taken;

		int parent_tid = barriers[tid].parent_tid;
		int numrounds = barriers[tid].numrounds;
		int sense = 0;
		//bool sense = false;

		gettimeofday(&start, NULL);

		for (n = 0; n < num_iters; n++) {
			tournament_barrier(tid, parent_tid, numrounds, &sense);

			//foo(n);
			//printf("thread %d, barrier: %d\n", tid, n);
			//time1 = omp_get_wtime();
			//tournament_barrier(barriers, tid);
			//time2 = omp_get_wtime();
			//timediff += (time2 - time1);
		}

		gettimeofday(&end, NULL);

		/* Calculate average time per barrier */
		timediff = (timediff * 1000) / num_iters;
  
		time_taken = (end.tv_sec - start.tv_sec) * 1e9; 
		time_taken = (time_taken + (end.tv_usec - start.tv_usec) * 1e3); 
		time_taken = time_taken / num_iters;
  
		#pragma omp critical
		total_time += time_taken;
	}
    printf("Avg time across all barrier %f ns\n", total_time/nthreads);
	return 0;
}
