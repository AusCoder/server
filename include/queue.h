#ifndef _QUEUE_H
#define _QUEUE_H

#include <stdlib.h>
#include <pthread.h>

/* Queue api used by thead-queue server
 */
struct message {
  int sockfd;
  struct message *next;
};

struct queue {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  struct message *head;
  struct message *last;
  size_t size;
};

int queue_init(struct queue *q);

void queue_destroy(struct queue *q);

void queue_put(struct queue *q, int sockfd);

int queue_get(struct queue *q);

#endif
