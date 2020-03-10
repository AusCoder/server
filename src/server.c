#include "server.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/mman.h>

#define SEM_NAME "server"
#define HANDLE_ERR_MSG "handle: error\n"

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
      } else if (strcmp(ST_ARG_THREAD_POOL, optarg) == 0) {
        args->type = ST_THREAD_POOL;
        break;
      } else if (strcmp(ST_ARG_THREAD_QUEUE, optarg) == 0) {
        args->type = ST_THREAD_QUEUE;
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
        0)
      fprintf(stderr, HANDLE_ERR_MSG);
    close(newsockfd);
  }
}

void fork_server(int sockfd) {
  int newsockfd;
  pid_t ret;
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

    ret = fork();
    if (ret < 0)
      PERROR_EXIT("fork");

    if (ret == 0) { // child
      close(sockfd);

      if (handle(stats_ipc, newsockfd, (struct sockaddr *)&client_addr,
                 sin_size) < 0)
        fprintf(stderr, HANDLE_ERR_MSG);
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

  if (handle(targs->stats, targs->sockfd,
             (struct sockaddr *)&targs->client_addr, targs->addr_size) < 0)
    fprintf(stderr, HANDLE_ERR_MSG);

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

void *thread_pool_run(void *_args) {
  int newsockfd;
  socklen_t sin_size;
  struct sockaddr_storage client_addr;
  struct thread_pool_args *args = _args;

  sin_size = sizeof(client_addr);

  while (1) {
    newsockfd =
        accept(args->sockfd, (struct sockaddr *)&client_addr, &sin_size);
    if (newsockfd < 0) {
      perror("accept");
      continue;
    }
    if (handle(args->stats, newsockfd, (struct sockaddr *)&client_addr,
               sin_size) < 0)
      fprintf(stderr, HANDLE_ERR_MSG);
    close(newsockfd);
  }
}

void thread_pool_server(int sockfd) {
  int ret;
  struct thread_pool_args *targs;
  struct thread_pool_args *targs_arr[THREAD_POOL_NUM_THREADS];
  Stats stats;

  memset(&stats, 0, sizeof(stats));
  stats.lock = STATS_NO_LOCK;

  for (int i = 0; i < THREAD_POOL_NUM_THREADS; i++) {
    targs_arr[i] = targs =
        (struct thread_pool_args *)malloc(sizeof(struct thread_pool_args));
    targs->stats = &stats;
    targs->sockfd = sockfd;

    ret = pthread_create(&(targs->thread_id), NULL, thread_pool_run, targs);
    if (ret < 0) {
      close(sockfd);
      PERROR_RETURN_VOID("pthread_create");
    }
  }

  for (int i = 0; i < THREAD_POOL_NUM_THREADS; i++) {
    targs = targs_arr[i];
    pthread_join(targs->thread_id, NULL);
  }
}

void *thread_queue_consumer_run(void *args) {
  struct thread_queue_message_body *body;
  struct thread_queue_consumer_args *targs = args;

  while (1) {
    body = queue_get(targs->q);
    if (body == NULL)
      STDERR_RETURN("queue_get", NULL);

    if (handle(targs->stats, body->sockfd,
               (struct sockaddr *)&(body->client_addr), body->addrlen) < 0)
      fprintf(stderr, HANDLE_ERR_MSG);

    close(body->sockfd);
    free(body);
  }
}

void thread_queue_server(int sockfd) {
  int ret;
  struct thread_queue_message_body *body;
  struct thread_queue_consumer_args *targs;
  struct queue q;
  Stats stats;

  memset(&stats, 0, sizeof(stats));
  stats.lock = STATS_NO_LOCK;
  queue_init(&q);

  for (int i = 0; i < THREAD_QUEUE_NUM_THREADS; i++) {
    targs = (struct thread_queue_consumer_args *)malloc(sizeof(*targs));
    if (targs == NULL) {
      close(sockfd);
      PERROR_RETURN_VOID("malloc");
    }
    targs->stats = &stats;
    targs->q = &q;

    ret =
        pthread_create(&targs->thread_id, NULL, thread_queue_consumer_run, targs);
    if (ret != 0) {
      close(sockfd);
      PERROR_RETURN_VOID("pthread_create");
    }
    ret = pthread_detach(targs->thread_id);
    if (ret != 0) {
      close(sockfd);
      PERROR_RETURN_VOID("pthread_detach");
    }
  }

  while (1) {
    body = (struct thread_queue_message_body *)malloc(sizeof(*body));
    if (body == NULL) {
      close(sockfd);
      PERROR_RETURN_VOID("malloc");
    }

    body->addrlen = sizeof(body->client_addr);
    body->sockfd = accept(sockfd, (struct sockaddr *)&(body->client_addr),
                          &(body->addrlen));
    if (body->sockfd < 0) {
      perror("accept");
      free(body);
      continue;
    }
    if (queue_put(&q, body) < 0)
      STDERR_RETURN_VOID("queue_put");
  }
}
