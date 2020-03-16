#include "common.h"

int GLOBAL_log_lvl = LOG_LVL_INFO;

int set_log_lvl(int lvl) {
  // TODO: check for valid lvl
  GLOBAL_log_lvl = lvl;
  return 0;
}
