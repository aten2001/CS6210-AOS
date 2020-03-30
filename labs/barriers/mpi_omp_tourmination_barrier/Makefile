MPICC=mpicc
OMPI_LDFLAGS=-lm -fopenmp

all: tourmination_barrier

tourmination_barrier: tourmination_barrier.c
	${MPICC} -o tourmination_barrier tourmination_barrier.c $(OMPI_LDFLAGS)

clean:
	$(RM) -f *.o tourmination_barrier
