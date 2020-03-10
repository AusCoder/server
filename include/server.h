#ifndef _SERVER_H
#define _SERVER_H
#include "common.h"
#include "handler.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum {
  ST_SINGLE,
  ST_FORK,
  ST_THREAD,
  ST_THREAD_POOL,
  ST_THREAD_QUEUE,
  ST_NONE,
} ServerType;

#define ST_ARG_SINGLE "single"
#define ST_ARG_FORK "fork"
#define ST_ARG_THREAD "thread"
#define ST_ARG_THREAD_POOL "thread-pool"
#define ST_ARG_THREAD_QUEUE "thread-queue"

#define ST_DEFAULT ST_THREAD

#define THREAD_POOL_NUM_THREADS 4

#define THREAD_QUEUE_NUM_THREADS 4

struct server_args {
  ServerType type;
};

void read_server_args(int argc, char *const argv[], struct server_args *args);

/* Single process server
 * Processes one request at a time
 */
void single_process_server(int sockfd);

/* Fork server
 * Forks a new process for each request
 */
void fork_server(int sockfd);

void fork_server_cleanup();

/* Thread server args
 * Starts new thread pre request
 */
void thread_server(int sockfd);

struct thread_args {
  Stats *stats;
  int sockfd;
  struct sockaddr_storage client_addr;
  socklen_t addr_size;
  pthread_t thread_id;
  int is_finished;
};

/* Thread pool server
 * Uses a pool of threads blocked on accept for new connections
 */
void thread_pool_server(int sockfd);

struct thread_pool_args {
  Stats *stats;
  int sockfd;
  pthread_t thread_id;
};

/* Thread queue server
 * Queues requests and uses thread pool to process requests
 * from the queue
 */
void thread_queue_server(int sockfd);

struct thread_queue_consumer_args {
  Stats *stats;
  struct queue *q;
  pthread_t thread_id;
};

struct thread_queue_message_body {
  int sockfd;
  struct sockaddr_storage client_addr;
  socklen_t addrlen;
};

#endif
