#ifndef _COMMON_H
#define _COMMON_H

#include <errno.h>
#include <stdio.h>

#define LOG_LVL_ERR 1
#define LOG_LVL_WARN 2
#define LOG_LVL_INFO 3
#define LOG_LVL_DEBUG 4

extern int GLOBAL_log_lvl;
int set_log_lvl(int lvl);

#define LOG(lvl, fmt, ...)                                                     \
  do {                                                                         \
    if (lvl > GLOBAL_log_lvl)                                                  \
      break;                                                                   \
    if (lvl <= LOG_LVL_WARN)                                                   \
      fprintf(stderr, fmt, __VA_ARGS__);                                       \
    else                                                                       \
      fprintf(stdout, fmt, __VA_ARGS__);                                       \
  } while (0)

#define LOG_ERR(fmt, ...) LOG(LOG_LVL_ERR, fmt, __VA_ARGS__)
#define LOG_WARN(fmt, ...) LOG(LOG_LVL_WARN, fmt, __VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG(LOG_LVL_INFO, fmt, __VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG(LOG_LVL_DEBUG, fmt, __VA_ARGS__)

// LOGLN variants in include the newline for simple messages
#define LOGLN_ERR(msg) LOG_ERR("%s\n", msg)
#define LOGLN_WARN(msg) LOG_WARN("%s\n", msg)
#define LOGLN_INFO(msg) LOG_INFO("%s\n", msg)
#define LOGLN_DEBUG(msg) LOG_DEBUG("%s\n", msg)

#define LOGLN_ERR_EXIT(msg)                                                    \
  do {                                                                         \
    LOG_ERR("%s\n", msg);                                                      \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define LOGLN_ERR_RETURN(msg, ret)                                             \
  do {                                                                         \
    LOG_ERR("%s\n", msg);                                                      \
    return ret;                                                                \
  } while (0)

#define LOGLN_ERR_RETURN_VOID(msg)                                             \
  do {                                                                         \
    LOG_ERR("%s\n", msg);                                                      \
    return;                                                                    \
  } while (0)

#define PERROR_EXIT(msg)                                                       \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define PERROR_EXIT_ERRNO(en, msg)                                             \
  do {                                                                         \
    errno = en;                                                                \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define PERROR_RETURN(msg, ret)                                                \
  do {                                                                         \
    perror(msg);                                                               \
    return ret;                                                                \
  } while (0)

#define PERROR_RETURN_ERRNO(en, msg, ret)                                      \
  do {                                                                         \
    errno = en;                                                                \
    perror(msg);                                                               \
    return ret;                                                                \
  } while (0)

#define PERROR_RETURN_VOID(msg)                                                \
  do {                                                                         \
    perror(msg);                                                               \
    return;                                                                    \
  } while (0)

#define UNUSED __attribute__((unused))

#endif
