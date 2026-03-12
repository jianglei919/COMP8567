#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

int main()
{
    int main_pid = getpid();
    int pid1 = fork();
    int pid2 = fork();
    int pid3 = fork();

    printf("pid: %d, ppid: %d\n", getpid(), getppid());

    for (;;)
        ;

    return 0;
}