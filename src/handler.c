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
#define RECVBUFSIZE 512
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

static void free_request(Request *req) {
  free(req->uri);
}

// Allocates for the request uri
int read_request(int sockfd, Request *req) {
  char recvbuf[RECVBUFSIZE];
  ssize_t numbytes, read_numbytes, scan_numbytes;
  char *readbuf, *scanbuf, *tmpscanbuf;
  size_t bufsize = RECVBUFSIZE;

  read_numbytes = 0;
  scan_numbytes = 0;
  readbuf = recvbuf;
  scanbuf = recvbuf;

  while (1) {
    numbytes = recv(sockfd, readbuf, bufsize, 0);
    if (numbytes < 0)
      PERROR_RETURN("recv", -1);

    if (numbytes == 0) {
      printf("numbytes == 0\n");
      break;
    }

    read_numbytes += numbytes;
    scan_numbytes += numbytes;
    readbuf += numbytes;
    bufsize -= numbytes;

    tmpscanbuf = memchr(scanbuf, ' ', scan_numbytes);
    if (tmpscanbuf == NULL)
      STDERR_RETURN("memchr method", -1); // TODO should be a stderr error
    // need an enum for handling errors
    if (memcmp(STR_GET, scanbuf, STR_GET_LEN) != 0)
      STDERR_RETURN("not a get request",
                    -1);
    req->method = METHOD_GET;

    // Should check if scan_bytes is negative here
    scan_numbytes -= tmpscanbuf + 1 - scanbuf;
    scanbuf = tmpscanbuf + 1;

    tmpscanbuf = memchr(scanbuf, ' ', scan_numbytes);
    // This should error for really long uris
    // maybe we need a continue
    if (tmpscanbuf == NULL)
      STDERR_RETURN("memchr uri", -1);

    *tmpscanbuf = '\0';
    req->uri = (char *)malloc(sizeof(char) * (tmpscanbuf + 1 - scanbuf));
    strcpy(req->uri, scanbuf);

    scan_numbytes -= tmpscanbuf + 1 - scanbuf;
    scanbuf = tmpscanbuf + 1;

    tmpscanbuf = memchr(scanbuf, '\n', scan_numbytes);
    if (tmpscanbuf == NULL)
      STDERR_RETURN("memchr httpv", -1); // maybe this should continue?
    if (*(tmpscanbuf - 1) != '\r')
      STDERR_RETURN("bad carriage return", -1);

    if (memcmp(STR_HTTP10, scanbuf, STR_HTTP10_LEN) == 0) {
      req->httpv = HTTPV10;
    } else if (memcmp(STR_HTTP11, scanbuf, STR_HTTP11_LEN) == 0) {
      req->httpv = HTTPV11;
    } else {
      STDERR_RETURN("httpv", -1);
    }
    scan_numbytes -= tmpscanbuf + 1 - scanbuf;
    scanbuf = tmpscanbuf + 1;

    // now comes all the headers

    break;
  }
  return 0;
}

// Q: why can't I put an inline here?
// A: Because it is not a static function
// TODO: We really should check the buflen here
void format_http_headers(char *buf, UNUSED size_t buflen, size_t contentlen) {
  sprintf(buf, "%s %s %s\r\n%s: %ld\r\n\r\n", STR_HTTP10, STR_STATUS_200,
          STR_STATUS_OK, STR_H_CONTENT_LEN, contentlen);
}

void format_http_headers_text(char *buf, UNUSED size_t buflen,
                              size_t contentlen) {
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

int read_directory_fork_exec(const char *dirpath, char *buf, int bufsize) {
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

int read_directory_in_proc(const char *dirpath, char *buf, size_t bufsize) {
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

int handle_stats(Stats *stats, int sockfd) {
  size_t contentLen;
  char contentBuf[BUFSIZE], headerBuf[BUFSIZE];

  sprintf(contentBuf,
          "Server Statistics\n\nRequests: %d\n\n2xx: %d\n3xx: %d\n4xx: "
          "%d\n5xx: %d\n",
          stats->reqs, stats->r2xx, stats->r3xx, stats->r4xx, stats->r5xx);
  contentLen = strlen(contentBuf);

  format_http_headers(headerBuf, BUFSIZE, contentLen);

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

int handle_directory(UNUSED Stats *stats, int sockfd, UNUSED Request *req,
                     const char *dirpath) {
  int contentlen;
  char headerBuf[BUFSIZE];
  char contentBuf[SENDBUFSIZE];

  // TODO: how does other copy functions handle the terminating byte?
  contentlen = read_directory_in_proc(dirpath, contentBuf, SENDBUFSIZE);
  if (contentlen < 0) {
    return -1;
  }

  format_http_headers_text(headerBuf, BUFSIZE, contentlen);
  if (sendAll(sockfd, headerBuf, strlen(headerBuf)) < 0) {
    return -1;
  }

  if (sendAll(sockfd, contentBuf, contentlen) < 0) {
    return -1;
  }
  return 0;
}

int handle_file_or_directory(Stats *stats, int sockfd, Request *req) {
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
    return handle_directory(stats, sockfd, req, filepath);
  }
  sendFileSize = statbuf.st_size;

  // handle file
  fd = open(filepath, 0);
  if (fd < 0) {
    perror("open");
    close(fd);
    return -1;
  }

  format_http_headers(headerBuf, BUFSIZE, sendFileSize);
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
  // this guy needs to handle the stats incrementing
  int ret;
  if (strcmp(req->uri, STATS_URL) == 0) {
    ret = handle_stats(stats, sockfd);
  } else {
    ret = handle_file_or_directory(stats, sockfd, req);
  }
  free_request(req);
  return ret;

  // if (stats_inc(stats, SM_INC_2XX) < 0) {
  //  perror("stats_inc");
  //  return -1;
  //}
}

int handle(Stats *stats, int sockfd, struct sockaddr *client_addr,
           UNUSED socklen_t addr_size) {
  Request req;
  char client_addr_str[INET6_ADDRSTRLEN];

  inet_ntop(client_addr->sa_family, get_in_addr(client_addr), client_addr_str,
            sizeof(client_addr_str));

  printf("server: got connection from %s\n", client_addr_str);
  if (stats_inc(stats, SM_INC_REQ) < 0) {
    perror("stats_inc");
    return -1;
  }

  // numbytes = read_request_buf(sockfd, recvBuf, BUFSIZE);
  // if (numbytes < 0) {
  //  fprintf(stderr, "failed to read a request buffer from socket: %d\n",
  //          sockfd);
  //  return -1;
  //}

  //// Parse request
  // if (parse_http_request(recvBuf, numbytes, &req) < 0) {
  //  fprintf(stderr, "failed to parse request. TODO: render error num\n");
  //  return 0;
  //}

  if (read_request(sockfd, &req) < 0) {
    fprintf(stderr, "failed to read or parse request\n");
    return -1;
  }

  printf("method: %d. uri: %s. http version: %d\n", req.method, req.uri,
         req.httpv);
  return dispatch(stats, sockfd, &req);
}
