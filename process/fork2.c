#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main() {
    if (fork() == 0) {
        fork();
        exit(0);
        // return 0;
    }
    // fork();
    // fork();
    wait(NULL);
    // sleep(3);
    printf("Done\n");
}