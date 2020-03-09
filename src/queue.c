#include "queue.h"
#include "common.h"
#include <assert.h>

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
    q->size--;
  }
  return 0;
}

int queue_put(struct queue *q, void *body) {
  int en;
  struct message *m;
  m = (struct message *)malloc(sizeof(*m));
  m->body = body;
  m->next = NULL;

  en = pthread_mutex_lock(&q->mutex);
  if (en != 0)
    PERROR_RETURN_ERRNO(en, "pthread_mutex_lock", -1);

  if (q->size == 0) {
    assert(q->head == NULL);
    assert(q->last == NULL);
    q->head = m;
    q->last = m;
  } else {
    assert(q->head != NULL);
    assert(q->last != NULL);
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

void *queue_get(struct queue *q) {
  void *body;
  int en;
  struct message *m;

  en = pthread_mutex_lock(&q->mutex);
  if (en != 0)
    PERROR_RETURN_ERRNO(en, "pthread_mutex_lock", NULL);
  while (q->size == 0) {
    en = pthread_cond_wait(&q->cond, &q->mutex);
    if (en != 0)
      PERROR_RETURN_ERRNO(en, "pthread_cond_wait", NULL);
  }

  m = q->head;
  body = m->body;

  q->size--;
  if (q->size == 0) {
    q->head = NULL;
    q->last = NULL;
  } else {
    q->head = q->head->next;
  }

  free(m);

  en = pthread_mutex_unlock(&q->mutex);
  if (en != 0)
    PERROR_RETURN_ERRNO(en, "pthread_mutex_unlock", NULL);
  return body;
}
