#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <pthread.h>

int item_to_produce, curr_buf_size;
int total_items, max_buf_size, num_workers, num_masters;
int total_consumed = 0;

int *buffer;

// condition variables for synchronization
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t buffer_not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer_not_empty = PTHREAD_COND_INITIALIZER;
int done_producing = 0;

void print_produced(int num, int master) {
  printf("Produced %d by master %d\n", num, master);
}

void print_consumed(int num, int worker) {
  printf("Consumed %d by worker %d\n", num, worker);
}

// produce items and place in buffer
// synchronize using lock and condition variables
void *generate_requests_loop(void *data)
{
  int thread_id = *((int *)data);

  while(1)
  {
    // lock
    pthread_mutex_lock(&mutex);
    
    // check if we've produced everything
    if(item_to_produce >= total_items) {
      // signal that we're done producing
      done_producing = 1;
      // unlock
      pthread_mutex_unlock(&mutex);
      break;
    }
    
    // wait if buffer is full
    while(curr_buf_size >= max_buf_size) {
      pthread_cond_wait(&buffer_not_full, &mutex);
      
      // check termination condition after waking up
      if(item_to_produce >= total_items) {
        done_producing = 1;
        pthread_mutex_unlock(&mutex);
        return 0;
      }
    }
    
    // produce and add to buffer
    buffer[curr_buf_size++] = item_to_produce;
    print_produced(item_to_produce, thread_id);
    item_to_produce++;
    
    // signal that buffer isn't empty
    pthread_cond_signal(&buffer_not_empty);
    pthread_mutex_unlock(&mutex);
  }
  return 0;
}

// worker consumes buffer items
void *consume_requests_loop(void *data)
{
  int thread_id = *((int *)data);
  int item;
  
  while(1)
  {
    // lock
    pthread_mutex_lock(&mutex);
    
    // if buffer empty wait
    while(curr_buf_size <= 0) {
      // if nothing is left to be produced + buffer empty return
      if(done_producing && curr_buf_size == 0) {
        pthread_mutex_unlock(&mutex);
        return 0;
      }
      // otherwise wait
      pthread_cond_wait(&buffer_not_empty, &mutex);
      
      // recheck after waking up
      if(done_producing && curr_buf_size == 0) {
        pthread_mutex_unlock(&mutex);
        return 0;
      }
    }
    
    // consume buffer item
    curr_buf_size--;
    item = buffer[curr_buf_size];
    total_consumed++;
    print_consumed(item, thread_id);
    
    // let threads know buffer isn't full
    pthread_cond_signal(&buffer_not_full);
    pthread_mutex_unlock(&mutex);
  }
  return 0;
}

int main(int argc, char *argv[])
{
  int *master_thread_id;
  int *worker_thread_id;
  pthread_t *master_thread;
  pthread_t *worker_thread;
  item_to_produce = 0;
  curr_buf_size = 0;
  
  int i;
  
  if (argc < 5) {
    printf("./master-worker #total_items #max_buf_size #num_workers #masters e.g. ./exe 10000 1000 4 3\n");
    exit(1);
  }
  else {
    num_masters = atoi(argv[4]);
    num_workers = atoi(argv[3]);
    total_items = atoi(argv[1]);
    max_buf_size = atoi(argv[2]);
  }
    
  buffer = (int *)malloc(sizeof(int) * max_buf_size);

  // create master producer threads
  master_thread_id = (int *)malloc(sizeof(int) * num_masters);
  master_thread = (pthread_t *)malloc(sizeof(pthread_t) * num_masters);
  for (i = 0; i < num_masters; i++)
    master_thread_id[i] = i;

  // create worker consumer threads
  worker_thread_id = (int *)malloc(sizeof(int) * num_workers);
  worker_thread = (pthread_t *)malloc(sizeof(pthread_t) * num_workers);
  for (i = 0; i < num_workers; i++)
    worker_thread_id[i] = i;

  // start master threads
  for (i = 0; i < num_masters; i++)
    pthread_create(&master_thread[i], NULL, generate_requests_loop, (void *)&master_thread_id[i]);
  
  // start worker threads
  for (i = 0; i < num_workers; i++)
    pthread_create(&worker_thread[i], NULL, consume_requests_loop, (void *)&worker_thread_id[i]);
  
  // wait for all master threads to complete
  for (i = 0; i < num_masters; i++) {
    pthread_join(master_thread[i], NULL);
    printf("master %d joined\n", i);
  }
  
  // wait for all worker threads to complete
  for (i = 0; i < num_workers; i++) {
    pthread_join(worker_thread[i], NULL);
    printf("worker %d joined\n", i);
  }
  
  // deallocate buffers
  free(buffer);
  free(master_thread_id);
  free(master_thread);
  free(worker_thread_id);
  free(worker_thread);
  
  return 0;
}