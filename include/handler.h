#ifndef _HANDLER_H
#define _HANDLER_H

#include <sys/types.h>
#include <sys/socket.h>
#include "http.h"
#include <semaphore.h>

#define WEB_ROOT "www"
#define STATS_URL "/statistics"

#define STATS_NO_LOCK (sem_t *)-1

typedef struct _Request Request;

typedef struct _Stats Stats;

// Q: how to enum in C?
struct _Request {
    int method;
    char *uri;
    int httpv;
};

struct _Stats {
    int reqs;
    int r2xx;
    int r3xx;
    int r4xx;
    int r5xx;

    sem_t *lock;
};

typedef enum {
    SM_INC_REQ, SM_INC_2XX, SM_INC_3XX, SM_INC_4XX, SM_INC_5XX
} StatsMod;

int handle(Stats *stats, int sockfd, struct sockaddr *client_addr, socklen_t addr_size);

#endif

// Could I write a string class that will hold arbitrary length strings?
// (Up to a certain size)
// Once it hits an initial size limit, it resizes.
// Then there is an overall limit on the size of the string, and there is a hard failure.
