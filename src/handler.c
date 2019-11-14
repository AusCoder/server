#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "handler.h"

#define BUFSIZE 512
#define SENDBUFSIZE 1024

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

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
        return -2;
    }
    printf("method: %d. uri: %s. http version: %d\n", req->method, req->uri, req->httpv);
    return 0;
}

// Q: why can't I put an inline here?
void formatHttpHeaders(char *buf, size_t buflen, size_t contentlen) {
    sprintf(
        buf,
        "%s %s %s\r\n%s: %ld\r\n\r\n",
        STR_HTTP10, STR_STATUS_200, STR_STATUS_OK,
        STR_H_CONTENT_LEN, contentlen
    );
}

ssize_t fileSize(int fd) {
    struct stat s;
    if (fstat(fd, &s) < 0) {
        fprintf(stderr, "shit, gotta fix this\n");
    }
    return s.st_size;
}

int sendFile(int sockfd, int filefd, char *filepath, const char *uri) {
    int numbytes;
    char buf[SENDBUFSIZE];

    while (1) {
        numbytes = read(filefd, buf, SENDBUFSIZE);
        if (numbytes < 0) {
            perror("read");
            return -1;
        }
        if (numbytes == 0) {
            return 0;
        }
        if (send(sockfd, buf, numbytes, 0) < 0) {
            perror("send");
            return -1;
        }
    }
}

int handleStats(Stats *stats, int sockfd) {
    size_t contentLen;
    char contentBuf[BUFSIZE], headerBuf[BUFSIZE];

    sprintf(
        contentBuf,
        "Server Statistics\n\nRequests: %d\n\n2xx: %d\n3xx: %d\n4xx: %d\n5xx: %d\n",
        stats->reqs, stats->r2xx, stats->r3xx, stats->r4xx, stats->r5xx
    );
    contentLen = strlen(contentBuf);

    formatHttpHeaders(headerBuf, BUFSIZE, contentLen);

    if (send(sockfd, headerBuf, strlen(headerBuf), 0) < 0) {
        perror("send");
        return -1;
    }
    if (send(sockfd, contentBuf, contentLen, 0) < 0) {
        perror("send");
        return -1;
    }
    stats->r2xx++;
    return 0;
}

int handle(Stats *stats, int sockfd, struct sockaddr *client_addr, socklen_t addr_size) {
    Request req;
    ssize_t numbytes, sendFileSize;
    int fd;
    char client_addr_str[INET6_ADDRSTRLEN], recvBuf[BUFSIZE], headerBuf[BUFSIZE], filepath[BUFSIZE];

    inet_ntop(
        client_addr->sa_family,
        get_in_addr(client_addr),
        client_addr_str,
        sizeof(client_addr_str)
    );
    printf("server: got connection from %s\n", client_addr_str);
    stats->reqs++;

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
    if (numbytes == 0) { // I don't think this is right?
        printf("connection closed by client\n");
        return 0;
    }

    // Parse request
    if (parseHttpRequest(recvBuf, numbytes, &req) < 0) {
        fprintf(stderr, "failed to parse request. TODO: render error num\n");
        return 0;
    }

    // Serve statitics
    if (strcmp(req.uri, STATS_URL) == 0) {
        return handleStats(stats, sockfd);
    }

    // Serve file
    if (strlen(WEB_ROOT) + strlen(req.uri) > BUFSIZE - 1) {
        fprintf(stderr, "filepath overflow\n");
        return 0;
    }

    strcpy(filepath, WEB_ROOT);
    strcat(filepath, req.uri);

    // Open file
    fd = open(filepath, 0);
    if (fd < 0) {
        perror("open");
        close(fd);
        return -1;
    }
    sendFileSize = fileSize(fd);

    formatHttpHeaders(headerBuf, BUFSIZE, sendFileSize);
    // TODO: should loop here because send might not send everything
    if (send(sockfd, headerBuf, strlen(headerBuf), 0) < 0) {
        perror("send");
        close(fd);
        return -1;
    }
    if (sendFile(sockfd, fd, filepath, req.uri) < 0) {
        fprintf(stderr, "failed to send file\n");
        close(fd);
        return 0;
    }
    close(fd);
    stats->r2xx++;

    return 0;
}
