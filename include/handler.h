#ifndef _HANDLER_H
#define _HANDLER_H

#include <sys/types.h>
#include <sys/socket.h>
#include "http.h"

#define WEB_ROOT "www"
#define STATS_URL "/statistics"

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
};

int handle(Stats *stats, int sockfd, struct sockaddr *client_addr, socklen_t addr_size);

#endif

// Could I write a string class that will hold arbitrary length strings?
// (Up to a certain size)
// Once it hits an initial size limit, it resizes.
// Then there is an overall limit on the size of the string, and there is a hard failure.
