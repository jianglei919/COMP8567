#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main() {
    pid_t main_pid = getpid();
    printf("Main process pid: %d\n", main_pid);

    // if (fork() || fork()) {
    //     fork();
    // }

    // if (fork() || fork() && fork());

    if (fork()&&fork()||fork());

    printf("pid: %d, ppid: %d\n", getpid(), getppid());
    // printf("1\n");
    fflush(stdout);
    if (getpid() == main_pid) {
        while (wait(NULL) > 0); // Wait for all child processes to finish
        printf("All child processes have finished. Exiting main process.\n");
    }
    return 0;
}