#include "common.h"
#include "server.h"
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

//#define PORT "3711"
#define BACKLOG 10
#define _MAX_LISTENING_SOCKETS 32

// Could run with atexit, but then the child processes can
// remove the semaphore. For now, we run it on SIGINT
void sigint_handler(UNUSED int s) {
  fork_server_cleanup();
  exit(EXIT_FAILURE);
}

void sigchld_handler(UNUSED int s) {
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  errno = saved_errno;
}

int main(int argc, char *argv[]) {
  int sockfd;
  struct sigaction sa_chld, sa_int;
  struct server_cli_args cliargs;
  struct server_args args;

  if (read_server_cli_args(argc, argv, &cliargs) < 0)
    LOGLN_ERR_EXIT("failed to read server cli args");

  args.sockfdslen = 0;
  args.sockfds = malloc(cliargs.portslen * sizeof(*(args.sockfds)));
  if (args.sockfds == NULL)
    PERROR_EXIT("malloc");

  for (size_t i = 0; i < cliargs.portslen; i++) {
    args.sockfds[i] = sockfd = create_server_socket(cliargs.ports[i]);
    if (sockfd < 0)
      LOGLN_ERR_EXIT("failed to create server socket");
    args.sockfdslen++;
    printf("created server socket on port %s\n", cliargs.ports[i]);
  }

  sa_chld.sa_handler = sigchld_handler;
  sigemptyset(&sa_chld.sa_mask);
  sa_chld.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa_chld, NULL) < 0)
    PERROR_EXIT("sigaction");

  sa_int.sa_handler = sigint_handler;
  sigemptyset(&sa_int.sa_mask);
  sa_int.sa_flags = 0;
  if (sigaction(SIGINT, &sa_int, NULL) < 0)
    PERROR_EXIT("sigaction");

  if (cliargs.type == ST_SINGLE) {
    single_process_server(&args);
  } else if (cliargs.type == ST_FORK) {
    fork_server(&args);
  } else if (cliargs.type == ST_THREAD) {
    thread_server(&args);
  } else if (cliargs.type == ST_THREAD_POOL) {
    thread_pool_server(&args);
  } else if (cliargs.type == ST_THREAD_QUEUE) {
    thread_queue_server(&args);
  } else {
    LOGLN_ERR_EXIT("Unknown server type");
  }
  return 0;
}
