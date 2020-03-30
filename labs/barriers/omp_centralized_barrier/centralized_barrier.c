#include <stdio.h>
#include <stdlib.h>
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

int nthreads;

/* Centralized barrier implementation 
 *
 * @count		: count of threads left to reach barrier (shared)
 * @sense		: global sense (shared)
 * @local_sense	: local sense (private)
 */
void centralized_barrier(int *count, bool *sense, bool *local_sense)
{
	int lcount;

	*local_sense = !*local_sense;		// Each thread toggles its own sense

	lcount = __sync_fetch_and_sub( count, 1 );	// Atomic update of count

	if (lcount == 1) {
		*count = nthreads;
		*sense = *local_sense;			// Last thread toggles global sense
	} else {
		while (*sense != *local_sense);	// Each thread waits for global sense to reverse
	}

	return;
}

void foo()
{
	int i, j, k = 0;
	for (i = 0; i < 100; i++) {
		for (j = 0; j < 100; j++)
			k += i * j;
	}
}

int main(int argc, char **argv)
{
	bool sense = true;
	int count = 0;
	int num_iters = NUM_ITERATIONS;
    double total_time = 0.0;
	/* Default 8 threads */
	nthreads = 8;

	/* Get number of threads and iterations */
	if (argc > 1) {
		nthreads = atoi(argv[1]);
		if (argc > 2)
			num_iters = atoi(argv[2]);
	}
	omp_set_num_threads(nthreads);

	printf("nthreads %d\n", nthreads);

	/* Initialize count */
	count = nthreads;

	/* Setting count & global sense as shared data */
	#pragma omp parallel shared(count, sense) firstprivate(nthreads)
	{
		int n, tid = omp_get_thread_num();
		bool local_sense = true;

		struct timeval start, end;
		double time1, time2, timediff = 0;
 		double time_taken; 

		gettimeofday(&start, NULL);

		for (n = 0; n < num_iters; n++) {
			//foo();
			debug("thread %d, barrier: %d\n", tid, n);
			//time1 = omp_get_wtime();
			centralized_barrier(&count, &sense, &local_sense);
			//time2 = omp_get_wtime();
			//timediff += (time2 - time1);
		}

		gettimeofday(&end, NULL);

		/* Calculate average time per barrier */
		//timediff = (timediff * 1000) / num_iters;

		time_taken = (end.tv_sec - start.tv_sec) * 1e9; 
		time_taken = (time_taken + (end.tv_usec - start.tv_usec)) * 1e3; 
		time_taken = time_taken / num_iters;
  
  		printf("Avg Time taken per barrier by process %d is %f ns\n", tid, time_taken);
        #pragma omp critical
        total_time += time_taken;
	}
    printf("Avg time across all barrier %f ns\n", total_time/nthreads);
	return 0;
}
