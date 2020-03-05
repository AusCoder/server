#include "common.h"
#include "queue.h"

int queue_init(struct queue *q) {
  int en;
  en = pthread_mutex_init(&q->mutex, NULL);
  if (en != 0)
    PERROR_RETURN_ERRNO(en, "pthread_mutex_init", -1);
  en = pthread_cond_init(&q->cond, NULL);
  if (en != 0)
    PERROR_RETURN_ERRNO(en, "pthread_cond_init", -1);
  q->head = NULL;
  q->last = NULL;
  q->size = 0;
  return 0;
}

int queue_destroy(struct queue *q) {
  int en;
  struct message *m;
  en = pthread_cond_destroy(&q->cond);
  if (en != 0)
    PERROR_RETURN_ERRNO(en, "pthread_cond_destroy", -1);
  en = pthread_mutex_destroy(&q->mutex);
  if (en != 0)
    PERROR_RETURN_ERRNO(en, "pthread_mutex_destroy", -1);

  while (q->size > 0) {
    m = q->head;
    q->head = q->head->next;
    free(m);
  }
  // Free everything - print a warning?
  //if (q->head != NULL || q->last != NULL)
  //  STDERR_RETURN("queue_destroy: non empty queue", -1);
  return 0;
}

int queue_put(struct queue *q, int sockfd) {
  int en;
  struct message *m;
  m = (struct message *)malloc(sizeof(*m));
  m->sockfd = sockfd;
  m->next = NULL;

  en = pthread_mutex_lock(&q->mutex);
  if (en != 0)
    PERROR_RETURN_ERRNO(en, "pthread_mutex_lock", -1);

  if (q->size == 0) {
    q->head = m;
    q->last = m;
  } else {
    q->last->next = m;
    q->last = m;
  }
  q->size++;

  en = pthread_cond_signal(&q->cond);
  if (en != 0)
    PERROR_RETURN_ERRNO(en, "pthread_cond_signal", -1);
  en = pthread_mutex_unlock(&q->mutex);
  if (en != 0)
    PERROR_RETURN_ERRNO(en, "pthread_mutex_unlock", -1);
  return 0;
}

int queue_get(struct queue *q) {
  int sockfd, en;
  struct message *m;

  en = pthread_mutex_lock(&q->mutex);
  if (en != 0)
    PERROR_RETURN_ERRNO(en, "pthread_mutex_lock", -1);
  while (q->size == 0) {
    en = pthread_cond_wait(&q->cond, &q->mutex);
    if (en != 0)
      PERROR_RETURN_ERRNO(en, "pthread_cond_wait", -1);
  }

  m = q->head;
  sockfd = m->sockfd;
  free(m);

  q->size--;
  if (q->size == 0) {
    q->head = NULL;
    q->last = NULL;
  } else {
    q->head = q->head->next;
  }

  en = pthread_mutex_unlock(&q->mutex);
  if (en != 0)
    PERROR_RETURN_ERRNO(en, "pthread_mutex_unlock", -1);
  return sockfd;
}
