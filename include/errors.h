#ifndef _ERRORS_H
#define _ERRORS_H

#define HANDLE_ERROR_EXIT(msg)                                                 \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define HANDLE_ERROR_EXIT_ERRNO(en, msg)                                       \
  do {                                                                         \
    error = en;                                                                \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define HANDLE_ERROR_RETURN(msg, ret)                                          \
  do {                                                                         \
    perror(msg);                                                               \
    return ret;                                                                \
  } while (0)

#define HANDLE_ERROR_RETURN_VOID(msg)                                          \
  do {                                                                         \
    perror(msg);                                                               \
    return;                                                                    \
  } while (0)

#endif