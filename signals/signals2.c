#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();
    if (pid == 0) {
        // signal(SIGINT, SIG_IGN);
        pause();
        printf("Child ");
    } else {
        sleep(1);
        kill(pid, SIGINT);
        int ret = wait(NULL);
        printf("Wait return: %d\n", ret);
        printf("Parent");
    }
    // int ret = wait(NULL);
    // printf("Wait return: %d\n", ret);
}