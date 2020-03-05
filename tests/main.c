#include "tests.h"

int main(void) {
  int num_failed;
  Suite *s;
  SRunner *sr;

  s = queue_suite();
  sr = srunner_create(s);

  srunner_run_all(sr, CK_NORMAL);
  num_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (num_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
