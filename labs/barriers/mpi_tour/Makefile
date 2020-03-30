MPICC=mpicc
OMPI_LDFLAGS=-lm

all: tournament_barrier

tournament_barrier: tournament_barrier.c
	${MPICC} -o tournament_barrier tournament_barrier.c $(OMPI_LDFLAGS)

clean:
	$(RM) -f *.o tournament_barrier
