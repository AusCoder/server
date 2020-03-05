#include "tests.h"
#include "queue.h"
#include <unistd.h>

void *consumer_run(void *args) {
  int *sockfd_ptr;
  struct queue *q;

  q = args;
  sockfd_ptr = (int *)malloc(sizeof(*sockfd_ptr));
  *sockfd_ptr = queue_get(q);
  return sockfd_ptr;
}

START_TEST(test_put_get_2_threads) {
  int *result;
  pthread_t thread;
  struct queue q;
  ck_assert_int_eq(queue_init(&q), 0);
  pthread_create(&thread, NULL, consumer_run, &q);

  usleep(10 * 1000);

  queue_put(&q, 17);
  pthread_join(thread, (void **)&result);
  queue_destroy(&q);
  ck_assert_int_eq(*result, 17);
}
END_TEST

Suite *queue_suite() {
  Suite *s;
  TCase *tc_queue;

  s = suite_create("Queue");
  tc_queue = tcase_create("Queue");

  tcase_add_test(tc_queue, test_put_get_2_threads);
  suite_add_tcase(s, tc_queue);

  return s;
}
