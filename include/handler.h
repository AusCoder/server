#ifndef _HANDLER_H
#define _HANDLER_H

#include <sys/types.h>
#include <sys/socket.h>
#include "http.h"

#define WEB_ROOT "www"

// Q: how to enum in C?
struct _Request {
    int method;
    char *uri;
    int httpv;
};
typedef struct _Request Request;

int handle(int sockfd, struct sockaddr *client_addr, socklen_t addr_size);

#endif

// Could I write a string class that will hold arbitrary length strings?
// (Up to a certain size)
// Once it hits an initial size limit, it resizes.
// Then there is an overall limit on the size of the string, and there is a hard failure.
