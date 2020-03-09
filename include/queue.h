#ifndef _QUEUE_H
#define _QUEUE_H

#include "common.h"
#include <pthread.h>
#include <stdlib.h>

/* Queue api used by thead-queue server
 */
struct message {
  void *body;
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

int queue_destroy(struct queue *q);

int queue_put(struct queue *q, void *body);

void *queue_get(struct queue *q);

#endif
