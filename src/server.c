#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <netdb.h>
#include "handler.h"

#define PORT "3711"
#define BACKLOG 10

void sigchld_handler(int s) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void single_process_server(int sockfd) {
    int new_fd;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    Stats stats;
    memset(&stats, 0, sizeof(stats));

    while (1) {
        sin_size = sizeof(client_addr);
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
        if (new_fd < 0) {
            perror("accept");
            continue;
        }

        if (handle(&stats, new_fd, (struct sockaddr *) &client_addr, sin_size) < 0) {
            perror("handler");
        }
        close(new_fd);
    }
}

void fork_server(int sockfd) {
    int newsockfd, zerofd;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    Stats *stats;

    // memory mapped without a backing file
    stats = (Stats *)mmap((void *)-1, sizeof(Stats), PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (stats == MAP_FAILED) {
        perror("mmap");
        return;
    }
    // memory mapped backed with /dev/zero
    // if ((zerofd = open("/dev/zero", O_RDWR)) < 0) {
    //     perror("open");
    //     return;
    // }
    // stats = (Stats *)mmap(0, sizeof(Stats), PROT_READ | PROT_WRITE,
    //     MAP_SHARED, zerofd, 0);
    // if (stats == MAP_FAILED) {
    //     perror("mmap");
    //     return;
    // }
    // close(zerofd);

    while (1) {
        sin_size = sizeof(client_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
        if (newsockfd < 0) {
            perror("accept");
            continue;
        }

        if (!fork()) { // the child process
            close(sockfd);
            if (handle(stats, newsockfd, (struct sockaddr *) &client_addr, sin_size) < 0) {
                perror("handler");
            }
            close(newsockfd);
            exit(0);
        }
        close(newsockfd);
    }
}

int main(int argc, char *argv[]) {
    int sockfd, status;
    struct addrinfo hints, *servinfo, *p;
    struct sigaction sa;
    int yes = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, PORT, &hints, &servinfo);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(status));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }
    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections on port %s\n", PORT);
    // single_process_server(sockfd);
    fork_server(sockfd);
    return 0;
}
