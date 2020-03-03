#include "server.h"
#include "common.h"
#include "handler.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/mman.h>

#define SEM_NAME "server"

Stats *stats_ipc = NULL;

void read_server_args(int argc, char *const argv[], struct server_args *args) {
  int opt;

  args->type = ST_NONE;

  while ((opt = getopt(argc, argv, "t:")) != -1) {
    switch (opt) {
    case 't':
      if (strcmp(ST_ARG_SINGLE, optarg) == 0) {
        args->type = ST_SINGLE;
        break;
      } else if (strcmp(ST_ARG_FORK, optarg) == 0) {
        args->type = ST_FORK;
        break;
      } else if (strcmp(ST_ARG_THREAD, optarg) == 0) {
        args->type = ST_THREAD;
        break;
      } else {
        fprintf(stderr, "Invalid server type: %s\n", optarg);
        exit(EXIT_FAILURE);
      }
    default:
      fprintf(stderr, "Usage: %s [-t type]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (args->type == ST_NONE) {
    args->type = ST_DEFAULT;
  }

  return;
}

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
  if (stats_ipc == MAP_FAILED)
    HANDLE_ERROR_RETURN_VOID("mmap");

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
  if (stats_ipc->lock == SEM_FAILED)
    HANDLE_ERROR_RETURN_VOID("sem_open");

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

void fork_server_cleanup() {
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
}

void *thread_run(void *arg) {
  struct thread_args *targs = arg;
  int ret;

  ret = handle(targs->stats, targs->sockfd,
               (struct sockaddr *)&targs->client_addr, targs->addr_size);
  if (ret < 0) {
    perror("handle");
  }
  close(targs->sockfd);
  targs->is_finished = 1;
  return NULL;
}

struct thread_args *alloc_thread_args(struct thread_args **thread_args_arr,
                                      int max_num_threads) {
  struct thread_args *targs = NULL;
  for (int i = 0; i < max_num_threads; i++) {
    if (thread_args_arr[i] == NULL) {
      targs = thread_args_arr[i] =
          (struct thread_args *)malloc(sizeof(struct thread_args));
      break;
    }
  }
  return targs;
}

void free_thread_args(struct thread_args **thread_args_arr,
                      int max_num_threads) {
  struct thread_args *targs;
  for (int i = 0; i < max_num_threads; i++) {
    targs = thread_args_arr[i];
    if (targs != NULL && targs->is_finished) {
      pthread_join(targs->thread_id, NULL);
      thread_args_arr[i] = NULL;
      free(targs);
    }
  }
}

void thread_server(int sockfd) {
  int newsockfd, ret, max_num_threads;
  max_num_threads = 20;
  struct thread_args *thread_args_arr[max_num_threads];
  struct thread_args *targs;
  struct sockaddr_storage client_addr;
  socklen_t sin_size;
  Stats stats;

  memset(&stats, 0, sizeof(stats));
  stats.lock = STATS_NO_LOCK;

  for (int i = 0; i < max_num_threads; i++) {
    thread_args_arr[i] = NULL;
  }

  while (1) {
    sin_size = sizeof(client_addr);
    newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
    if (newsockfd < 0) {
      perror("accept");
      continue;
    }

    targs = alloc_thread_args(thread_args_arr, max_num_threads);
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
    targs->is_finished = 0;

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
      free_thread_args(thread_args_arr, max_num_threads);
      HANDLE_ERROR_RETURN_VOID("pthread_create");
    }

    free_thread_args(thread_args_arr, max_num_threads);
  }
}
