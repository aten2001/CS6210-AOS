/*
1. line-76 we cannot join a already detached thread.
2. line-116 Cannot detach the consumer thread, as that will lead to memory leak and might cause abrupt shutdown if main thread returns before it finishes.
3. line-150 g_num_prod should be locked before we edit that.
4. line 175 cannot return a memory address on stack to caller process. So malloced it on heap before returning its pointer.
5. line 213 we need to again get locks, since we released them after previous iteration.
6. line 39  not a logical bug, but the mutex was never initialized. So did a static initialization
*/
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>


/* Queue Structures */

typedef struct queue_node_s {
  struct queue_node_s *next;
  struct queue_node_s *prev;
  char c;
} queue_node_t;

typedef struct {
  struct queue_node_s *front;
  struct queue_node_s *back;
  pthread_mutex_t lock;
} queue_t;


/* Thread Function Prototypes */
void *producer_routine(void *arg);
void *consumer_routine(void *arg);


/* Global Data */
long g_num_prod; /* number of producer threads */
/** Bug 6
 * This mutex was never initialized. Doing a static initialization*/
pthread_mutex_t g_num_prod_lock = PTHREAD_MUTEX_INITIALIZER;


/* Main - entry point */
int main(int argc, char **argv) {
  queue_t queue;
  pthread_t producer_thread, consumer_thread;
  void *thread_return = NULL;
  int result = 0;

  /* Initialization */

  printf("Main thread started with thread id %lu\n", pthread_self());

  memset(&queue, 0, sizeof(queue));
  pthread_mutex_init(&queue.lock, NULL);

  g_num_prod = 1; /* there will be 1 producer thread */

  /* Create producer and consumer threads */

  result = pthread_create(&producer_thread, NULL, producer_routine, &queue);
  if (0 != result) {
    fprintf(stderr, "Failed to create producer thread: %s\n", strerror(result));
    exit(1);
  }

  printf("Producer thread started with thread id %lu\n", producer_thread);

  
  result = pthread_detach(producer_thread);
  if (0 != result)
    fprintf(stderr, "Failed to detach producer thread: %s\n", strerror(result));
  
  result = pthread_create(&consumer_thread, NULL, consumer_routine, &queue);
  if (0 != result) {
    fprintf(stderr, "Failed to create consumer thread: %s\n", strerror(result));
    exit(1);
  }

  /* Join threads, handle return values where appropriate */

  /** Bug 1: Cannot join a already detached thread*/
  // result = pthread_join(producer_thread, NULL);
  // if (0 != result) {
  //   fprintf(stderr, "Failed to join producer thread: %s\n", strerror(result));
  //   pthread_exit(NULL);
  // }
  

  result = pthread_join(consumer_thread, &thread_return);
  if (0 != result) {
    fprintf(stderr, "Failed to join consumer thread: %s\n", strerror(result));
    pthread_exit(NULL);
  }
  printf("\nPrinted %lu characters.\n", *(long*) thread_return);
  free(thread_return);
  
  pthread_mutex_destroy(&queue.lock);
  pthread_mutex_destroy(&g_num_prod_lock);
  return 0;
}


/* Function Definitions */

/* producer_routine - thread that adds the letters 'a'-'z' to the queue */
void *producer_routine(void *arg) {
  queue_t *queue_p = arg;
  queue_node_t *new_node_p = NULL;
  pthread_t consumer_thread;
  int result = 0;
  char c;

  result = pthread_create(&consumer_thread, NULL, consumer_routine, queue_p);
  if (0 != result) {
    fprintf(stderr, "Failed to create consumer thread: %s\n", strerror(result));
    exit(1);
  }

  /**Bug 2 
   * Logic here is that I won't detach this consumer thread, 
   * because else there will be memory leak. The consumer thread returns a pointer 
   * to the memory on heap which has to be released. So we wait for this consumer thread.
  result = pthread_detach(consumer_thread);
  if (0 != result)
    fprintf(stderr, "Failed to detach consumer thread: %s\n", strerror(result));
  */
  for (c = 'a'; c <= 'z'; ++c) {

    /* Create a new node with the prev letter */
    new_node_p = malloc(sizeof(queue_node_t));
    new_node_p->c = c;
    new_node_p->next = NULL;

    /* Add the node to the queue */
    pthread_mutex_lock(&queue_p->lock);
    if (queue_p->back == NULL) {
      assert(queue_p->front == NULL);
      new_node_p->prev = NULL;
      queue_p->front = new_node_p;
      queue_p->back = new_node_p;
    }
    else {
      assert(queue_p->front != NULL);
      new_node_p->prev = queue_p->back;
      queue_p->back->next = new_node_p;
      queue_p->back = new_node_p;
    }
    pthread_mutex_unlock(&queue_p->lock);

    sched_yield();
  }

  /* Bug 3 
  Decrement the number of producer threads running, then return */
  /* Get the lock for g_num_prod before editing the variable*/
  pthread_mutex_lock(&g_num_prod_lock);
  --g_num_prod;
  pthread_mutex_unlock(&g_num_prod_lock);

  /* consumer thread wait*/
  void *thread_return = NULL;
  result = pthread_join(consumer_thread, &thread_return);
  if (0 != result) {
    fprintf(stderr, "Failed to join consumer thread: %s\n", strerror(result));
    pthread_exit(NULL);
  }
  free(thread_return);

  return (void *)0;
}


/* consumer_routine - thread that prints characters off the queue */
void *consumer_routine(void *arg) {                
  queue_t *queue_p = arg;
  queue_node_t *prev_node_p = NULL;
  /**Bug 4
   * We cannot return a pointer to a stack address. Malloc it on heap and then return*/
  long *count = malloc(sizeof(long));
  *count = 0; /* number of nodes this thread printed */

  printf("Consumer thread started with thread id %lu\n", pthread_self());

  /* terminate the loop only when there are no more items in the queue
   * AND the producer threads are all done */

  pthread_mutex_lock(&queue_p->lock);
  pthread_mutex_lock(&g_num_prod_lock);
  while(queue_p->front != NULL || g_num_prod > 0) {
    pthread_mutex_unlock(&g_num_prod_lock);

    if (queue_p->front != NULL) {

      /* Remove the prev item from the queue */
      prev_node_p = queue_p->front;

      if (queue_p->front->next == NULL)
        queue_p->back = NULL;
      else
        queue_p->front->next->prev = NULL;

      queue_p->front = queue_p->front->next;
      pthread_mutex_unlock(&queue_p->lock);

      /* Print the character, and increment the character count */
      printf("%c", prev_node_p->c);
      free(prev_node_p);
      ++(*count);
    }
    else { /* Queue is empty, so let some other thread run */
      pthread_mutex_unlock(&queue_p->lock);
      sched_yield();
    }
    /**Bug 5 
     * Again getting the locks, as we released them in the last iteration*/
    pthread_mutex_lock(&queue_p->lock);
    pthread_mutex_lock(&g_num_prod_lock);
  
  }
  pthread_mutex_unlock(&g_num_prod_lock);
  pthread_mutex_unlock(&queue_p->lock);
  return (void*) count;
}
