#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define N 100

int main(int argc, char **argv) {
    char hostname[N];
    if (gethostname(hostname, N) != 0) {
        fprintf(stderr, "gethostname() failed: %d\n", errno);
        exit(1);
    }
    printf("hostname: %s\n", hostname);
    return 1;
}
