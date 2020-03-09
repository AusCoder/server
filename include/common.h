#ifndef _COMMON_H
#define _COMMON_H

#include <errno.h>
#include <stdio.h>

#define HANDLE_ERROR_EXIT(msg)                                                 \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define PERROR_EXIT(msg)                                                       \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define HANDLE_ERROR_EXIT_ERRNO(en, msg)                                       \
  do {                                                                         \
    errno = en;                                                                \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define PERROR_EXIT_ERRNO(en, msg)                                             \
  do {                                                                         \
    errno = en;                                                                \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define HANDLE_ERROR_RETURN(msg, ret)                                          \
  do {                                                                         \
    perror(msg);                                                               \
    return ret;                                                                \
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

#define HANDLE_ERROR_RETURN_VOID(msg)                                          \
  do {                                                                         \
    perror(msg);                                                               \
    return;                                                                    \
  } while (0)

#define PERROR_RETURN_VOID(msg)                                                \
  do {                                                                         \
    perror(msg);                                                               \
    return;                                                                    \
  } while (0)

#define HANDLE_ERROR_STDERR_RETURN(msg, ret)                                   \
  do {                                                                         \
    fprintf(stderr, msg);                                                      \
    return ret;                                                                \
  } while (0)

#define STDERR_RETURN(msg, ret)                                                \
  do {                                                                         \
    fprintf(stderr, "%s\n", msg);                                              \
    return ret;                                                                \
  } while (0)

#define STDERR_RETURN_VOID(msg)                                                \
  do {                                                                         \
    fprintf(stderr, "%s\n", msg);                                              \
    return;                                                                    \
  } while (0)

#define STDERR_EXIT(msg)                                                       \
  do {                                                                         \
    fprintf(stderr, "%s\n", msg);                                              \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define UNUSED __attribute__((unused))

#endif
