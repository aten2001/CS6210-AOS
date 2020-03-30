MPICC=mpicc
OMPI_LDFLAGS=-lm

all: dissemination_barrier

dissemination_barrier: dissemination_barrier.c
	${MPICC} -o dissemination_barrier dissemination_barrier.c $(OMPI_LDFLAGS)

clean:
	$(RM) -f *.o dissemination_barrier
