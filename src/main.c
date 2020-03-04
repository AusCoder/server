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

#define PORT "3711"
#define BACKLOG 10

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
  int sockfd, status;
  struct addrinfo hints, *servinfo, *p;
  struct sigaction sa_chld, sa_int;
  struct server_args servargs;
  int yes = 1;

  read_server_args(argc, argv, &servargs);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  status = getaddrinfo(NULL, PORT, &hints, &servinfo);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(status));
    exit(EXIT_FAILURE);
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
      HANDLE_ERROR_EXIT("setsockopt");

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }
  freeaddrinfo(servinfo);

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(EXIT_FAILURE);
  }

  if (listen(sockfd, BACKLOG) == -1)
    HANDLE_ERROR_EXIT("listen");

  sa_chld.sa_handler = sigchld_handler;
  sigemptyset(&sa_chld.sa_mask);
  sa_chld.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa_chld, NULL) < 0)
    HANDLE_ERROR_EXIT("sigaction");

  sa_int.sa_handler = sigint_handler;
  sigemptyset(&sa_int.sa_mask);
  sa_int.sa_flags = 0;
  if (sigaction(SIGINT, &sa_int, NULL) < 0)
    HANDLE_ERROR_EXIT("sigaction");

  if (servargs.type == ST_SINGLE) {
    printf("single process server: waiting for connections on port %s\n", PORT);
    single_process_server(sockfd);
  } else if (servargs.type == ST_FORK) {
    printf("fork server: waiting for connections on port %s\n", PORT);
    fork_server(sockfd);
  } else if (servargs.type == ST_THREAD) {
    printf("thread server: waiting for connections on port %s\n", PORT);
    thread_server(sockfd);
  } else if (servargs.type == ST_THREAD_POOL) {
    printf("thread pool server: waiting for connections on port %s\n", PORT);
    thread_pool_server(sockfd);
  } else {
    fprintf(stderr, "Unknown server type\n");
    exit(EXIT_FAILURE);
  }
  return 0;
}
