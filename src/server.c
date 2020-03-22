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

int read_server_cli_args(int argc, char *const argv[],
                         struct server_cli_args *args) {
  char *buf;
  int opt;

  args->ports = malloc(MAX_LISTENING_PORTS * sizeof(char *));
  if (args->ports == NULL)
    LOGLN_ERRNO_RETURN("malloc", -1);
  args->portssize = MAX_LISTENING_PORTS;
  args->portslen = 0;

  args->type = ST_NONE;

  while ((opt = getopt(argc, argv, "t:p:")) != -1) {
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
        return -1;
      }
    case 'p':
      if (args->portslen >= args->portssize)
        LOGLN_ERR_RETURN("more than maximum number of ports specified", -1);
      args->ports[args->portslen] = buf =
          malloc((strlen(optarg) + 1) * sizeof(char));
      if (buf == NULL)
        PERROR_RETURN("malloc", -1);
      strcpy(buf, optarg);
      args->portslen++;
      break;
    default:
      fprintf(stderr, "Usage: %s [-t type]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (args->type == ST_NONE) {
    args->type = ST_DEFAULT;
  }

  if (args->portslen == 0) {
    args->ports[args->portslen] = buf =
        malloc((strlen(DEFAULT_PORT) + 1) * sizeof(char));
    if (buf == NULL)
      LOGLN_ERRNO_RETURN("malloc", -1);
    strcpy(buf, DEFAULT_PORT);
    args->portslen++;
  }

  return 0;
}

int create_server_socket(const char *port) {
  int sockfd, status;
  struct addrinfo hints, *servinfo, *p;
  int yes = 1;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  status = getaddrinfo(NULL, port, &hints, &servinfo);
  if (status != 0) {
    LOG_ERR("getaddrinfo() failed: %s\n", gai_strerror(status));
    return -1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd < 0) {
      LOGLN_ERRNO("socket");
      continue;
    }

    // if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK) < 0)
    //  PERROR_RETURN("fcntl", -1);

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0)
      LOGLN_ERRNO_RETURN("setsockopt", -1);

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
      close(sockfd);
      LOGLN_ERRNO("bind");
      continue;
    }

    break;
  }
  freeaddrinfo(servinfo);

  if (p == NULL)
    LOGLN_ERR_RETURN("failed to bind", -1);

  if (listen(sockfd, BACKLOG) == -1)
    LOGLN_ERRNO_RETURN("listen", -1);

  return sockfd;
}

void single_process_server(struct server_args *args) {
  int sockfd, newsockfd;
  struct sockaddr_storage client_addr;
  socklen_t sin_size;
  Stats stats;

  memset(&stats, 0, sizeof(stats));
  stats.lock = STATS_NO_LOCK;

  if (args->sockfdslen != 1)
    LOGLN_ERR_RETURN_VOID("single_process_server can only run on one port");
  sockfd = args->sockfds[0];

  // TODO: add a log message saying the server type
  while (1) {
    sin_size = sizeof(client_addr);
    newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
    if (newsockfd < 0) {
      perror("accept");
      continue;
    }

    if (handle(&stats, newsockfd, (struct sockaddr *)&client_addr, sin_size) <
        0)
      LOG_ERR("%s\n", HANDLE_ERR_MSG);
    close(newsockfd);
  }
}

void fork_server(struct server_args *args) {
  int sockfd, newsockfd;
  pid_t ret;
  struct sockaddr_storage client_addr;
  socklen_t sin_size;

  // memory mapped without a backing file
  stats_ipc = (Stats *)mmap(NULL, sizeof(Stats), PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (stats_ipc == MAP_FAILED)
    LOGLN_ERR_RETURN_VOID("mmap");

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
    LOGLN_ERR_RETURN_VOID("sem_open");

  if (args->sockfdslen != 1)
    LOGLN_ERR_RETURN_VOID("fork_server can only run on one port");
  sockfd = args->sockfds[0];

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

void thread_server(struct server_args *args) {
  int sockfd, newsockfd, ret, max_num_threads;
  max_num_threads = 20;
  struct thread_args *thread_args_arr[max_num_threads];
  struct thread_args *targs;
  struct sockaddr_storage client_addr;
  socklen_t sin_size;
  Stats stats;

  memset(&stats, 0, sizeof(stats));
  stats.lock = STATS_NO_LOCK;

  if (args->sockfdslen != 1)
    LOGLN_ERR_RETURN_VOID("thread_server can only run on one port");
  sockfd = args->sockfds[0];

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
      LOGLN_ERR_RETURN_VOID("pthread_create");
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

void thread_pool_server(struct server_args *args) {
  int sockfd, ret;
  struct thread_pool_args *targs;
  struct thread_pool_args *targs_arr[THREAD_POOL_NUM_THREADS];
  Stats stats;

  memset(&stats, 0, sizeof(stats));
  stats.lock = STATS_NO_LOCK;

  if (args->sockfdslen != 1)
    LOGLN_ERR_RETURN_VOID("thread_pool_server can only run on one port");
  sockfd = args->sockfds[0];

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
      LOGLN_ERR_RETURN("queue_get", NULL);

    if (handle(targs->stats, body->sockfd,
               (struct sockaddr *)&(body->client_addr), body->addrlen) < 0)
      fprintf(stderr, HANDLE_ERR_MSG);

    close(body->sockfd);
    free(body);
  }
}

static int thread_queue_start_consumers(Stats *stats, struct queue *q) {
  int ret;
  struct thread_queue_consumer_args *targs;

  for (int i = 0; i < THREAD_QUEUE_NUM_THREADS; i++) {
    targs = (struct thread_queue_consumer_args *)malloc(sizeof(*targs));
    if (targs == NULL)
      PERROR_RETURN("malloc", -1);

    targs->stats = stats;
    targs->q = q;

    ret = pthread_create(&targs->thread_id, NULL, thread_queue_consumer_run,
                         targs);
    if (ret != 0)
      PERROR_RETURN("pthread_create", -1);

    ret = pthread_detach(targs->thread_id);
    if (ret != 0)
      PERROR_RETURN("pthread_detach", -1);
  }
  return 0;
}

// Might integrate this single socket listening
#if 0
static int thread_queue_producer_single_socket(struct queue *q, int sockfd) {
  struct thread_queue_message_body *body;

  while (1) {
    body = (struct thread_queue_message_body *)malloc(sizeof(*body));
    if (body == NULL)
      PERROR_RETURN("malloc", -1);

    body->addrlen = sizeof(body->client_addr);
    body->sockfd = accept(sockfd, (struct sockaddr *)&body->client_addr,
                          &body->addrlen);
    if (body->sockfd < 0) {
      perror("accept");
      free(body);
      continue;
    }
    if (queue_put(q, body) < 0)
      LOGLN_ERR_RETURN("queue_put", -1);
  }
}
#endif

/**
 * Accepts and returns the next socket available with a connection.
 * Returns ERR_SELECT_INTERRUPTED if select was interrupted by a
 * signal.
 */
static int accept_next(int *sockfds, size_t sockfdslen,
                       struct sockaddr *client_addr, socklen_t *addrlen) {
  int maxfd, sockfd, ret;
  fd_set rfds;

  maxfd = -1;
  FD_ZERO(&rfds);
  for (size_t i = 0; i < sockfdslen; i++) {
    sockfd = sockfds[i];
    FD_SET(sockfd, &rfds);
    if (sockfd > maxfd) {
      maxfd = sockfd;
    }
    LOG_INFO("Adding socket to watch set: %d\n", sockfd);
  }
  maxfd++;

  ret = select(maxfd, &rfds, NULL, NULL, NULL);
  if (ret < 0 && errno == EINTR)
    LOGLN_ERR_RETURN("received signal on select", ERR_SELECT_INTERRUPTED);
  if (ret < 0)
    LOGLN_ERRNO_RETURN("select", -1);

  for (size_t i = 0; i < sockfdslen; i++) {
    sockfd = sockfds[i];
    if (FD_ISSET(sockfd, &rfds)) {
      ret = accept(sockfd, client_addr, addrlen);
      if (ret < 0)
        LOGLN_ERRNO_RETURN("accept", -1);
      return ret;
    }
  }
  LOGLN_ERR_RETURN("failed to find socket for read", -1);
}

static int thread_queue_producer(struct queue *q, struct server_args *args) {
  struct thread_queue_message_body *body;
  while (1) {
    body = (struct thread_queue_message_body *)malloc(sizeof(*body));
    if (body == NULL)
      LOGLN_ERRNO_RETURN("malloc", -1);

    body->addrlen = sizeof(body->client_addr);
    body->sockfd =
        accept_next(args->sockfds, args->sockfdslen,
                    (struct sockaddr *)&body->client_addr, &body->addrlen);
    if (body->sockfd == ERR_SELECT_INTERRUPTED) {
      free(body);
      continue;
    }
    if (body->sockfd < 0) {
      free(body);
      LOGLN_ERR_RETURN("failed to accept socket", -1);
    }
    if (queue_put(q, body) < 0)
      LOGLN_ERR_RETURN("queue_put", -1);
  }
}

void thread_queue_server(struct server_args *args) {
  // int sockfd;
  struct queue q;
  Stats stats;

  memset(&stats, 0, sizeof(stats));
  stats.lock = STATS_NO_LOCK;
  queue_init(&q);

  // if (args->sockfdslen != 1)
  //  LOGLN_ERR_RETURN_VOID("thread_queue_server can only run on one port");
  // sockfd = args->sockfds[0];

  if (thread_queue_start_consumers(&stats, &q) < 0) {
    // close(sockfd);
    LOGLN_ERR_RETURN_VOID("failed to start thread queue consumers");
  }

  // TODO: Add args->sockfds cleanup function
  if (thread_queue_producer(&q, args) < 0) {
    // close(sockfd);
    LOGLN_ERR_RETURN_VOID("failed to run thread queue producer");
  }
}
