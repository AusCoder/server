#include "http.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT "3711"
#define MAXDATASIZE 100

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void formatHttpRequest(char *buf, size_t buflen, const char *uri) {
  strcpy(buf, STR_GET);
  strcat(buf, " ");
  strcat(buf, uri); // TODO: check length
  strcat(buf, " ");
  strcat(buf, STR_HTTP10);
  strcat(buf, STR_CRLF);
  strcat(buf, STR_CRLF);
}

int main(int argc, char *argv[]) {
  int sockfd, numbytes;
  char buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  int status;
  char s[INET6_ADDRSTRLEN];

  if (argc != 2) {
    fprintf(stderr, "usage: client hostname\n");
    exit(1);
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(argv[1], PORT, &hints, &servinfo);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(status));
    exit(1);
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd == -1) {
      perror("client: socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("client: connect");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s,
            sizeof(s));
  printf("client: connecting to %s\n", s);
  freeaddrinfo(servinfo);

  if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) < 0) {
    perror("recv");
    exit(1);
  }
  buf[numbytes] = '\0';
  printf("client: received '%s'\n", buf);

  close(sockfd);
  return 0;
}
