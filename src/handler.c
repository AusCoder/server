#include <stdio.h>
#include <string.h>
#include "handler.h"

#define BUFSIZE 512

#define GET "GET"
#define HTTP10 "HTTP/1.0"
#define HTTP11 "HTTP/1.1"

int parse_http(char *content, ssize_t content_len) {
    char *component;
    component = strsep(&content, " ");
    if (strcmp(GET, component) != 0) {
        printf("not a get request\n");
    } else {
        printf("a get request\n");
    }

    // component = strsep(&content, " ");
    // str
}

int handle(int sockfd, struct sockaddr *client_addr, socklen_t addr_size) {
    ssize_t numbytes;
    char buf[BUFSIZE];
    while (1) {
        numbytes = recv(sockfd, buf, BUFSIZE - 1, 0);
        if (numbytes < 0) {
            perror("recv");
            return -1;
        }
        if (numbytes >= BUFSIZE - 1) { // we don't handle large requests for now
            fprintf(stderr, "request too large\n");
            return -1;
        }
        printf("received: %ld bytes\n%s\n", numbytes, buf);
        if (numbytes == 0) { // I don't think this is right
            printf("connection closed by client");
            return 0;
        }
        parse_http(buf, numbytes);
        break;
    }

    return 0;
}
