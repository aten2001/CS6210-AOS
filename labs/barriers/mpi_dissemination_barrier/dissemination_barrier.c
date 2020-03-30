#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "mpi.h"

#define DEBUG	0

#if DEBUG
#define debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define debug(fmt, ...) ((void)0)
#endif

#define NUM_ITERATIONS 1000

int nprocesses;	// Number of processes
int nrounds;	// Max number of rounds for dissemination barrier

/* Dissemination Barrier implementation
 *
 * @rank	: rank of current process
 */
void dissemination_barrier(int rank)
{
	int round = 0;
	int ret, recv_rank, send_rank, pow2;
	bool msg = true;

	pow2 = 1;
	for (round = 0; round < nrounds; round++) {

		/* Determine rank of processes to send msg to and recv msg from */
		send_rank = (rank + pow2) % nprocesses;
		recv_rank = (rank + nprocesses - pow2) % nprocesses;

		/* Send message to target */
		debug("rank %d send msg to rank %d in round %d\n", rank, send_rank, round);
		ret = MPI_Send(&msg, 1, MPI_BYTE, send_rank, 1, MPI_COMM_WORLD);

		/* Recv message from source */
		ret = MPI_Recv(&msg, 1, MPI_BYTE, recv_rank, 1, MPI_COMM_WORLD, NULL);
		//sleep(1);
		debug("rank %d recv msg from rank %d in round %d\n", rank, recv_rank, round);

		pow2 = pow2 << 1;
	}
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
	int rank, len, n;
	int num_iters = NUM_ITERATIONS;
	char hostname[MPI_MAX_PROCESSOR_NAME];
	double log2rounds;

	struct timeval start, end;
	double time1, time2, timediff = 0;
 	double time_taken; 

	/* MPI initialization */
	MPI_Init(&argc, &argv);

	/* Determine number of tasks and rank of current task */
	MPI_Comm_size(MPI_COMM_WORLD, &nprocesses);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Get_processor_name(hostname, &len);

	/* Determine number of rounds for dissemination barrier */
	log2rounds = log(nprocesses) / log(2);
	nrounds = ceil(log2rounds);

	/* Get number of iterations */
	if (argc > 1)
		num_iters = atoi(argv[1]);

	//debug("processor name %s, numtasks %d, task rank: %d\n", hostname, nprocesses, rank);
	//debug("log2rounds: %f, nrounds; %d\n", log2rounds, nrounds);
    time1 = MPI_Wtime();
	for (n = 0; n < num_iters; n++) {
		//foo(100*n);
		//debug("process %d, barrier: %d\n", rank, n);
		dissemination_barrier(rank);
	}
    time2 = MPI_Wtime();
	timediff += (time2 - time1);

	time_taken = (timediff * 1000) / num_iters;

	printf("Avg Time taken per barrier by process %d is %f ms\n", rank, time_taken);
	
	double avg_barrier_time = 0.0;
    MPI_Reduce(&time_taken, &avg_barrier_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    if(!rank)
        printf("Avg Barrier time across all processes %f", avg_barrier_time*1e6/nprocesses);
   
	MPI_Finalize();
}
