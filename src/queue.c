#include "common.h"
#include "queue.h"

int queue_init(struct queue *q) {
  int en;
  en = pthread_mutex_init(q->mutex, NULL);
  if (en != 0)
    PERROR_RETURN_ERRNO(en, "pthread_mutex_init", -1);
}

void queue_destroy(struct queue *q);

void queue_put(struct queue *q, int sockfd);

int queue_get(struct queue *q);
