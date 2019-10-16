#ifndef _HANDLER_H
#define _HANDLER_H

#include <sys/types.h>
#include <sys/socket.h>

int handle(int sockfd, struct sockaddr *client_addr, socklen_t addr_size);

#endif
