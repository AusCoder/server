#include "handler.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFSIZE 512
#define SENDBUFSIZE 1024

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int stats_inc(Stats *stats, StatsMod mod) {
  if ((stats->lock != STATS_NO_LOCK) && (sem_wait(stats->lock)) < 0) {
    return -1;
  }
  switch (mod) {
  case SM_INC_REQ:
    stats->reqs++;
    break;
  case SM_INC_2XX:
    stats->r2xx++;
    break;
  case SM_INC_3XX:
    stats->r3xx++;
    break;
  case SM_INC_4XX:
    stats->r4xx++;
    break;
  case SM_INC_5XX:
    stats->r5xx++;
    break;
  default:
    break;
  }
  if ((stats->lock != STATS_NO_LOCK) && (sem_post(stats->lock) < 0)) {
    return -1;
  }
  return 0;
}

ssize_t readRequestBuf(int sockfd, char *recvBuf, int bufsize) {
  ssize_t numbytes;

  numbytes = recv(sockfd, recvBuf, bufsize - 1, 0);
  if (numbytes < 0) {
    perror("recv");
    return -1;
  }
  // we don't handle large requests for now
  // I am super paranoid about overflows
  // I want an expandable buffer!
  if (numbytes >= BUFSIZE - 1) {
    fprintf(stderr, "request too large\n");
    return -1;
  }
  if (numbytes == 0) { // I don't think this is right?
    fprintf(stderr, "connection closed by client\n");
    return -1;
  }
  return numbytes;
}

// How to handle content spread across buffers?
// Simple solution for now is to just increase the buffer size
int parseHttpRequest(char *content, ssize_t content_len, Request *req) {
  char *component;

  component = strsep(&content, " ");
  if (strcmp(STR_GET, component) != 0) {
    return -1; // need an enum for handling errors
  }
  req->method = METHOD_GET;
  req->uri = strsep(&content, " "); // TODO: sanitize uri
  // TODO: handle when just a '\n'
  component = strsep(&content, "\r\n");
  if (strcmp(STR_HTTP10, component) == 0) {
    req->httpv = HTTPV10;
  } else if (strcmp(STR_HTTP11, component) == 0) {
    req->httpv = HTTPV11;
  } else {
    return -2;
  }
  return 0;
}

// Q: why can't I put an inline here?
void formatHttpHeaders(char *buf, size_t buflen, size_t contentlen) {
  sprintf(buf, "%s %s %s\r\n%s: %ld\r\n\r\n", STR_HTTP10, STR_STATUS_200,
          STR_STATUS_OK, STR_H_CONTENT_LEN, contentlen);
}

void formatHttpHeadersText(char *buf, size_t buflen, size_t contentlen) {
  sprintf(buf, "%s %s %s\r\n%s: %ld\r\n", STR_HTTP10, STR_STATUS_200,
          STR_STATUS_OK, STR_H_CONTENT_LEN, contentlen);
  size_t curbuflen = strlen(buf);
  contentlen -= curbuflen;
  sprintf(buf + curbuflen, "%s: %s\r\n\r\n", STR_H_CONTENT_TYPE, "text/html");
}

ssize_t fileSize(int fd) {
  struct stat s;
  if (fstat(fd, &s) < 0) {
    fprintf(stderr, "TODO: gotta fix this\n");
  }
  return s.st_size;
}

int sendAll(int sockfd, char *buf, ssize_t bytes_to_send) {
  ssize_t ret;
  ssize_t bytes_sent = 0;
  while (bytes_sent < bytes_to_send) {
    ret = send(sockfd, buf, bytes_to_send, 0);
    if (ret < 0) {
      perror("send");
      return -1;
    }
    bytes_sent += ret;
  }
  return 0;
}

int sendFromFd(int filefd, int sockfd) {
  ssize_t numbytes;
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
    if (sendAll(sockfd, buf, numbytes) < 0) {
      return -1;
    }
  }
}

// TODO: check that this works with a file > bufsize
int readAllFromFd(int fd, char *buf, int bufsize) {
  ssize_t numbytes;
  int numbytesRead = 0;

  while (1) {
    numbytes = read(fd, buf, bufsize);
    if (numbytes < 0) {
      perror("read");
      return -1;
    }
    if (numbytes == 0) {
      return numbytesRead;
    }
    bufsize -= numbytes;
    numbytesRead += numbytes;
  }
}

int readDirectoryForkExec(const char *dirpath, char *buf, int bufsize) {
  int ret;
  pid_t f;
  int pipefd[2];
  if (pipe(pipefd) < 0) {
    perror("pipe");
    return -1;
  }

  f = fork();
  if (f < 0) {
    perror("fork");
    return -1;
  }
  if (f == 0) { // child
    close(pipefd[0]);
    if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
      perror("dup2");
      exit(1);
    }
    if (dup2(pipefd[1], STDERR_FILENO) < 0) {
      perror("dup2");
      exit(1);
    }
    // TODO: Check the exit code?
    // -a doesn't seem to work
    execlp("ls", "-l", dirpath, (char *)NULL);
    perror("execlp");
    exit(1);
  }
  close(pipefd[1]);
  ret = readAllFromFd(pipefd[0], buf, bufsize);
  close(pipefd[0]);
  return ret;
}

int readDirectoryInProc(const char *dirpath, char *buf, int bufsize) {
  size_t byteswritten, entNameSize;
  DIR *dir;
  struct dirent *ent;

  if ((dir = opendir(dirpath)) == NULL) {
    perror("opendir");
    return -1;
  }

  byteswritten = 0;
  while (1) {
    errno = 0;
    if ((ent = readdir(dir)) == NULL && (errno != 0)) {
      perror("readdir");
      // TODO: check return code of closedir
      closedir(dir);
      return -1;
    }
    if (ent == NULL) {
      closedir(dir);
      return byteswritten;
    }
    entNameSize = strlen(ent->d_name);
    if (entNameSize > bufsize - 2) {
      // TODO: error because buffer not big enough
      closedir(dir);
      return 0;
    }
    strcpy(buf, ent->d_name);
    buf += entNameSize;
    strcpy(buf, "\n");
    buf += 1;
    byteswritten += entNameSize + 1;
    bufsize -= entNameSize + 1;
  }
}

int handleStats(Stats *stats, int sockfd) {
  size_t contentLen;
  char contentBuf[BUFSIZE], headerBuf[BUFSIZE];

  sprintf(contentBuf,
          "Server Statistics\n\nRequests: %d\n\n2xx: %d\n3xx: %d\n4xx: "
          "%d\n5xx: %d\n",
          stats->reqs, stats->r2xx, stats->r3xx, stats->r4xx, stats->r5xx);
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
  if (stats_inc(stats, SM_INC_2XX) < 0) {
    perror("stats_inc");
    return -1;
  }
  return 0;
}

int handleDirectory(Stats *stats, int sockfd, Request *req,
                    const char *dirpath) {
  int contentlen;
  char headerBuf[BUFSIZE];
  char contentBuf[SENDBUFSIZE];

  // TODO: how does other copy functions handle the terminating byte?
  contentlen = readDirectoryInProc(dirpath, contentBuf, SENDBUFSIZE);
  if (contentlen < 0) {
    return -1;
  }

  formatHttpHeadersText(headerBuf, BUFSIZE, contentlen);
  if (sendAll(sockfd, headerBuf, strlen(headerBuf)) < 0) {
    return -1;
  }

  if (sendAll(sockfd, contentBuf, contentlen) < 0) {
    return -1;
  }
  return 0;
}

int handleFileOrDirectory(Stats *stats, int sockfd, Request *req) {
  int fd;
  ssize_t sendFileSize;
  char headerBuf[BUFSIZE], filepath[BUFSIZE];
  struct stat statbuf;

  if (strlen(WEB_ROOT) + strlen(req->uri) > BUFSIZE - 1) {
    fprintf(stderr, "filepath overflow\n");
    return 0;
  }

  strcpy(filepath, WEB_ROOT);
  strcat(filepath, req->uri);

  // Stat path to see what it is
  if (stat(filepath, &statbuf) != 0) {
    fprintf(stderr, "Failed to stat file\n");
    return -1;
  }
  if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {
    return handleDirectory(stats, sockfd, req, filepath);
  }
  sendFileSize = statbuf.st_size;

  // handle file
  fd = open(filepath, 0);
  if (fd < 0) {
    perror("open");
    close(fd);
    return -1;
  }

  formatHttpHeaders(headerBuf, BUFSIZE, sendFileSize);
  // TODO: should loop here because send might not send everything
  if (send(sockfd, headerBuf, strlen(headerBuf), 0) < 0) {
    perror("send");
    close(fd);
    return -1;
  }
  if (sendFromFd(fd, sockfd) < 0) {
    fprintf(stderr, "failed to send file\n");
    close(fd);
    return 0;
  }
  close(fd);
  if (stats_inc(stats, SM_INC_2XX) < 0) {
    perror("stats_inc");
    return -1;
  }
  return 0;
}

int dispatch(Stats *stats, int sockfd, Request *req) {
  if (strcmp(req->uri, STATS_URL) == 0) {
    return handleStats(stats, sockfd);
  }
  return handleFileOrDirectory(stats, sockfd, req);
}

int handle(Stats *stats, int sockfd, struct sockaddr *client_addr,
           socklen_t addr_size) {
  Request req;
  ssize_t numbytes;
  char client_addr_str[INET6_ADDRSTRLEN], recvBuf[BUFSIZE];

  inet_ntop(client_addr->sa_family, get_in_addr(client_addr), client_addr_str,
            sizeof(client_addr_str));
  // TODO: Try without printing all these logs and see if
  //  I am getting requests that aren't processed somewhere
  // (Trying to track down why num requests greater than expected)
  printf("server: got connection from %s\n", client_addr_str);
  if (stats_inc(stats, SM_INC_REQ) < 0) {
    perror("stats_inc");
    return -1;
  }

  numbytes = readRequestBuf(sockfd, recvBuf, BUFSIZE);
  if (numbytes < 0) {
    fprintf(stderr, "failed to read a request buffer from socket: %d\n",
            sockfd);
    return -1;
  }

  // Parse request
  if (parseHttpRequest(recvBuf, numbytes, &req) < 0) {
    fprintf(stderr, "failed to parse request. TODO: render error num\n");
    return 0;
  }
  printf("method: %d. uri: %s. http version: %d\n", req.method, req.uri,
         req.httpv);
  return dispatch(stats, sockfd, &req);
}

