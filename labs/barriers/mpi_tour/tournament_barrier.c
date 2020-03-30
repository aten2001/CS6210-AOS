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

void foo(int n)
{
	int i, j, k = 0;
	for (i = 0; i < 1000; i++) {
		for (j = 0; j < n; j++)
			k += i * j;
	}
}

void tournament_barrier(int rank, int numtasks) //, int n)
{
    int round = 1;
    bool wakeup = false;
    bool arrival = false;
    // Arrival part of Tournament
    while(round < numtasks){
        int dst = rank ^ round;
        // If your partner is beyond the task count we have skip the round
        // Such a case arises when task count is not a power of 2
        if(dst >= numtasks){
	    round <<= 1;
            continue;
	}
        if ((rank & round) != 0)
        {
            arrival = true;
            debug("Rank: %d Arrival: Sent To %d\n", rank, dst);
            //lose the match during arrival
            MPI_Send(&arrival, 1, MPI_BYTE, dst, 99, MPI_COMM_WORLD);
            MPI_Status status;
            //wait to winner to handshake back
            //debug("Rank: %d Wake-up: Recved from %d\n", rank, dst);
            MPI_Recv(&wakeup, 1, MPI_BYTE, dst, 99, MPI_COMM_WORLD, &status);
            break;
        }
        else{
            MPI_Status status;
            MPI_Recv(&arrival, 1, MPI_BYTE, dst, 99, MPI_COMM_WORLD, &status);
	        //debug("Rank: %d Arrival Src %d rank %d\n", dst, rank);
        }
        round = round << 1;
    }
    
    // Start the waking up procedure
    round >>= 1;
    if (rank ==0)
        wakeup = true;
    while (round >= 1){
        int dst = rank ^ round;
        if(dst >= numtasks){
            round >>=1;
            continue;
        }
        if((rank % round) == 0){
            debug("Rank: %d Wake-up: %d\n", rank, dst); 
             MPI_Send(&wakeup, 1, MPI_BYTE, dst, 99, MPI_COMM_WORLD);
        }
        round = round >> 1;
    }
}


int main(int argc, char *argv[])
{
	int rank, len, n;
	int num_iters = NUM_ITERATIONS;
	char hostname[MPI_MAX_PROCESSOR_NAME];
	
	struct timeval start, end;
	double time1, time2, timediff = 0;
 	double time_taken; 
    int nprocesses;	// Number of processes

	/* MPI initialization */
	MPI_Init(&argc, &argv);

	/* Determine number of tasks and rank of current task */
	MPI_Comm_size(MPI_COMM_WORLD, &nprocesses);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Get_processor_name(hostname, &len);

	
	/* Get number of iterations */
	if (argc > 1)
		num_iters = atoi(argv[1]);

	//debug("processor name %s, numtasks %d, task rank: %d\n", hostname, nprocesses, rank);
	//debug("log2rounds: %f, nrounds; %d\n", log2rounds, nrounds);
    time1 = MPI_Wtime();
	for (n = 0; n < num_iters; n++) {
		//foo(100*n);
		debug("process %d, barrier: %d\n", rank, n);
		tournament_barrier(rank, nprocesses);
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
