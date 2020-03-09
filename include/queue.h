#ifndef _QUEUE_H
#define _QUEUE_H

#include "common.h"
#include <stdlib.h>
#include <pthread.h>

/* Queue api used by thead-queue server
 */
struct message {
  void *body;
  struct message *next;
};

struct sock_queue {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  struct message *head;
  struct message *last;
  size_t size;
};

int sock_queue_init(struct sock_queue *q);

int sock_queue_destroy(struct sock_queue *q);

int sock_queue_put(struct sock_queue *q, void *body);

void *sock_queue_get(struct sock_queue *q);

#endif
