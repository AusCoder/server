#include "handler.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT "3711"
#define BACKLOG 10
#define SEM_NAME "server"

Stats *stats_ipc = NULL;

struct thread_args {
  Stats *stats;
  int sockfd;
  struct sockaddr_storage client_addr;
  socklen_t addr_size;
  int thread_idx; // internal idx used to keep track of thread_args
  pthread_t thread_id;
};

// Could run with atexit, but then the child processes can
// remove the semaphore. For now, we run it on SIGINT
void cleanup() {
  if ((stats_ipc != NULL) && (stats_ipc->lock != STATS_NO_LOCK)) {
    printf("closing sem\n");
    if (sem_close(stats_ipc->lock) < 0) {
      perror("sem_close");
    }
    if (sem_unlink(SEM_NAME) < 0) {
      perror("sem_unlink");
    }
    stats_ipc->lock = STATS_NO_LOCK;
  }
  exit(130);
}

void sigchld_handler(int s) {
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  errno = saved_errno;
}

void sigint_handler(int s) { cleanup(); }

void single_process_server(int sockfd) {
  int newsockfd;
  struct sockaddr_storage client_addr;
  socklen_t sin_size;
  Stats stats;
  memset(&stats, 0, sizeof(stats));
  stats.lock = STATS_NO_LOCK;

  while (1) {
    sin_size = sizeof(client_addr);
    newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
    if (newsockfd < 0) {
      perror("accept");
      continue;
    }

    if (handle(&stats, newsockfd, (struct sockaddr *)&client_addr, sin_size) <
        0) {
      perror("handler");
    }
    close(newsockfd);
  }
}

void fork_server(int sockfd) {
  int newsockfd;
  struct sockaddr_storage client_addr;
  socklen_t sin_size;

  // memory mapped without a backing file
  stats_ipc = (Stats *)mmap(NULL, sizeof(Stats), PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (stats_ipc == MAP_FAILED) {
    perror("mmap");
    return;
  }
  memset(stats_ipc, 0, sizeof(Stats));

  // memory mapped backed with /dev/zero
  // if ((zerofd = open("/dev/zero", O_RDWR)) < 0) {
  //     perror("open");
  //     return;
  // }
  // stats_ipc = (Stats *)mmap(0, sizeof(Stats), PROT_READ | PROT_WRITE,
  //     MAP_SHARED, zerofd, 0);
  // if (stats_ipc == MAP_FAILED) {
  //     perror("mmap");
  //     return;
  // }
  // close(zerofd);

  // semaphore to lock stats
  stats_ipc->lock = sem_open(SEM_NAME, O_CREAT | O_EXCL, O_RDWR, 1);
  if (stats_ipc->lock == SEM_FAILED) {
    perror("sem_open");
    return;
  }

  while (1) {
    sin_size = sizeof(client_addr);
    newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
    if (newsockfd < 0) {
      perror("accept");
      continue;
    }

    if (!fork()) { // the child process
      close(sockfd);

      if (handle(stats_ipc, newsockfd, (struct sockaddr *)&client_addr,
                 sin_size) < 0) {
        perror("handler");
      }
      close(newsockfd);
      exit(0);
    }
    close(newsockfd);
  }
}

void *thread_run(void *arg) {
  struct thread_args *targs = arg;
  int ret;

  ret = handle(targs->stats, targs->sockfd,
               (struct sockaddr *)&targs->client_addr, targs->addr_size);
  if (ret < 0) {
    close(targs->sockfd);
    perror("handle");
    return NULL;
  }
  close(targs->sockfd);
  return NULL;
}

struct thread_args *alloc_thread_args(struct thread_args **thread_args_arr,
                                      int max_num_threads) {
  struct thread_args *targs = NULL;
  for (int i = 0; i < max_num_threads; i++) {
    if (thread_args_arr[i] == NULL) {
      targs = thread_args_arr[i] =
          (struct thread_args *)malloc(sizeof(struct thread_args));
      targs->thread_idx = i;
      break;
    }
  }
  return targs;
}

void free_thread_args(struct thread_args **thread_args_arr, int max_num_threads,
                      struct thread_args *targs) {
  assert(targs->thread_idx >= 0);
  assert(targs->thread_idx < max_num_threads);
  free(targs);
  thread_args_arr[targs->thread_idx] = NULL;
}

void thread_server(int sockfd) {
  int newsockfd, ret, maxNumThreads;
  maxNumThreads = 20;
  struct thread_args *threadArgs[maxNumThreads];
  struct thread_args *targs;
  struct sockaddr_storage client_addr;
  socklen_t sin_size;
  Stats stats;

  memset(&stats, 0, sizeof(stats));
  stats.lock = STATS_NO_LOCK;

  for (int i = 0; i < maxNumThreads; i++) {
    threadArgs[i] = NULL;
  }

  while (1) {
    sin_size = sizeof(client_addr);
    newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
    printf("newsockfd: %d\n", newsockfd);
    if (newsockfd < 0) {
      perror("accept");
      continue;
    }

    targs = alloc_thread_args(threadArgs, maxNumThreads);
    if (targs == NULL) {
      // This should kill the program
      fprintf(stderr, "Could not find free thread_args\n");
      close(newsockfd);
      continue;
    }
    targs->stats = &stats;
    targs->sockfd = newsockfd;
    targs->client_addr = client_addr;
    targs->addr_size = sin_size;

    // // This struct appears to be shared between the threads!
    // // 2 threads would end up with the same newsockfd and one
    // // thread would block on a read from a socket that had
    // // already been read from
    // struct thread_args targs;
    // targs.stats = &stats;
    // targs.sockfd = newsockfd;
    // // Problem here!
    // // Lets see what happens. It would be better to copy.
    // targs.client_addr = client_addr;
    // targs.addr_size = sin_size;

    ret = pthread_create(&(targs->thread_id), NULL, thread_run, targs);
    if (ret != 0) {
      perror("pthread_create");
      return;
    }
  }
}

int main(int argc, char *argv[]) {
  int sockfd, status;
  struct addrinfo hints, *servinfo, *p;
  struct sigaction sa_chld, sa_int;
  int yes = 1;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  status = getaddrinfo(NULL, PORT, &hints, &servinfo);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(status));
    return 1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

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
    exit(1);
  }

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  sa_chld.sa_handler = sigchld_handler;
  sigemptyset(&sa_chld.sa_mask);
  sa_chld.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa_chld, NULL) < 0) {
    perror("sigaction");
    exit(1);
  }

  sa_int.sa_handler = sigint_handler;
  sigemptyset(&sa_int.sa_mask);
  sa_int.sa_flags = 0;
  if (sigaction(SIGINT, &sa_int, NULL) < 0) {
    perror("sigaction");
    exit(1);
  }

  printf("server: waiting for connections on port %s\n", PORT);
  // TODO: add getopt for arguments
  // single_process_server(sockfd);
  // fork_server(sockfd);
  thread_server(sockfd);
  return 0;
}
