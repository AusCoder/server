#include <stdio.h>
#include <string.h>
#include "handler.h"

#define BUFSIZE 512

int parseHttpRequest(char *content, ssize_t content_len, Request *req) {
    // TODO: how to handle content spread across buffers?
    // Simple solution for now is to just increase the buffer size
    char *component;

    component = strsep(&content, " ");
    if (strcmp(STR_GET, component) != 0) {
        return -1; // need an enum for handling errors
    }
    req->method = METHOD_GET;
    req->uri = strsep(&content, " ");  // TODO: sanitize uri
    // TODO: handle when just a '\n'
    component = strsep(&content, "\r\n");
    if (strcmp(STR_HTTP10, component) == 0) {
        req->httpv = HTTPV10;
    } else if (strcmp(STR_HTTP11, component) == 0) {
        req->httpv = HTTPV11;
    } else {
        return -2;  // need error enum
    }
    printf("method: %d. uri: %s. http version: %d\n", req->method, req->uri, req->httpv);
    return 0;
}

int formatHttpHeaders(char *buf, size_t buflen) {
    int len;

    // len = STR_HTTP10_LEN + CRLF_LEN + CRLF_LEN;
    len = 0;
    if (len > buflen) {
        return -1;
    }

    strcpy(buf, STR_HTTP10);
    buf += STR_HTTP10_LEN;
    // *buf++ = ' ';
    // *buf++ = '\0';
    strcpy(buf, STR_STATUS_200);
    buf += STR_STATUS_200_LEN;
    // strcpy(buf, CRLF);
    // buf += CRLF_LEN;
    // strcpy(buf, CRLF);
    // buf += CRLF_LEN;
    return len;
}

int sendFile(int sockfd, const char *uri) {
    FILE *fd;
    int numbytes;
    char buf[BUFSIZE];
    // Q: Is this really the best way to handle variable size buffers?
    char filepath[strlen(WEB_ROOT) + strlen(uri)];

    strcpy(filepath, WEB_ROOT);
    strcat(filepath, uri);
    // Q: is it possible to send direct from the file to socket?
    fd = fopen(filepath, "r");
    if (fd == NULL) {
        perror("fopen");
        return -1;
    }
    // read might be easier to use than fread
    // it might have better error handling
    do {
        numbytes = fread(buf, sizeof(char), BUFSIZE, fd);
        printf("read %u bytes from %s\n", numbytes, filepath);
    } while (numbytes > 0);
    // TODO check for error or eof using ferror and feof
    return 0;
}

int handle(int sockfd, struct sockaddr *client_addr, socklen_t addr_size) {
    Request req;
    ssize_t numbytes;
    int headerLen;
    char recvBuf[BUFSIZE], headerBuf[BUFSIZE];

    numbytes = recv(sockfd, recvBuf, BUFSIZE - 1, 0);
    if (numbytes < 0) {
        perror("recv");
        return -1;
    }
    // we don't handle large requests for now
    // I am super paranoid about overflows
    // I want an expandable buffer!
    if (numbytes >= BUFSIZE - 1) {
        fprintf(stderr, "request too large\n");
        return 0;
    }
    // printf("received: %ld bytes\n%s\n", numbytes, buf);
    if (numbytes == 0) { // I don't think this is right
        printf("connection closed by client\n");
        return 0;
    }
    if (parseHttpRequest(recvBuf, numbytes, &req) < 0) {
        fprintf(stderr, "failed to parse request. TODO: render error num\n");
        return 0;
    }
    headerLen = formatHttpHeaders(headerBuf, BUFSIZE);
    printf("headers: %s\n", headerBuf);
    if (headerLen < 0) {
        fprintf(stderr, "failed to format headers\n");
        return 0;
    }
    if (send(sockfd, headerBuf, headerLen, 0) < 0) {
        perror("send");
        return -1;
    }
    if (sendFile(sockfd, req.uri) < 0) {
        fprintf(stderr, "failed to send file\n");
        return 0;
    }

    return 0;
}
