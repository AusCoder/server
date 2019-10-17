#ifndef _HANDLER_H
#define _HANDLER_H

#include <sys/types.h>
#include <sys/socket.h>

#define CRLF "\r\n"
#define CRLF_LEN 3

#define STR_GET "GET"

#define STR_HTTP10 "HTTP/1.0"
#define STR_HTTP10_LEN 9
#define STR_HTTP11 "HTTP/1.1"
#define STR_HTTP11_LEN 9

#define STR_STATUS_OK "OK"
#define STR_STATUS_OK_LEN 3
#define STR_STATUS_200 "200"
#define STR_STATUS_200_LEN 4

#define METHOD_GET 1

#define HTTPV10 0
#define HTTPV11 1

#define WEB_ROOT "www"

// Q: how to enum in C?
struct _Request {
    int method;
    char *uri;
    int httpv;
};
typedef struct _Request Request;

int parseHttpHeaders(char *content);
int parseHttpRequest(char *content, ssize_t content_len, Request *req);

int formatHttpHeaders(char *buf, size_t buflen);
// int send_http_headers(int sockfd)
int handle(int sockfd, struct sockaddr *client_addr, socklen_t addr_size);

#endif

// Could I write a string class that will hold arbitrary length strings?
// (Up to a certain size)
// Once it hits an initial size limit, it resizes.
// Then there is an overall limit on the size of the string, and there is a hard failure.
