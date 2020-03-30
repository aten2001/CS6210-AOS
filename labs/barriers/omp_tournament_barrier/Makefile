CC = gcc            # default is CC = cc
CFLAGS = -g -Wall   # default is CFLAGS = [blank]
CPPFLAGS =          # default is CPPFLAGS = [blank]
LDFLAGS = -fopenmp -lm  # default is LDFLAGS = [blank]

# default compile command: $(CC) $(CFLAGS) $(CPPFLAGS) -c -o <foo>.o <foo>.c

all: tournament_barrier

tournament_barrier: tournament_barrier.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)
# default linking command: $(CC) $(LDFLAGS) <foo>.o -o <foo>

clean:
	$(RM) -f *.o tournament_barrier
