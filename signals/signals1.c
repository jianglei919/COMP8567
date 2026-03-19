#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>

void handler(int s) { printf("Sig "); }

void mysleep(int s) {
    for (int i = 0; i < s; i++) {
        sleep(1);
        printf("Sleeping %d s\n", i + 1);
    }
    printf("\n");
}

int main() {
    signal(SIGALRM, handler);
    mysleep(3);
    alarm(5);
    // pause();
    int left = sleep(4);
    // printf("Left: %d\n", left);
    printf("Main\n");
}