#ifndef _SERVER_H
#define _SERVER_H
#include "common.h"
#include "handler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef enum {
  ST_SINGLE,
  ST_FORK,
  ST_THREAD,
  ST_NONE,
} ServerType;

#define ST_ARG_SINGLE "single"
#define ST_ARG_FORK "fork"
#define ST_ARG_THREAD "thread"

#define ST_DEFAULT ST_THREAD;

struct server_args {
  ServerType type;
};

void read_server_args(int argc, char *const argv[], struct server_args *args);

struct thread_args {
  Stats *stats;
  int sockfd;
  struct sockaddr_storage client_addr;
  socklen_t addr_size;
  pthread_t thread_id;
  int is_finished;
};

void single_process_server(int sockfd);
void fork_server(int sockfd);
void thread_server(int sockfd);

void fork_server_cleanup();

#endif
