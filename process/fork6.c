#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main()
{

    int pid1 = fork();
    int pid2 = fork();
    printf("pid=%d, ppid=%d\n", getpid(), getppid());

    if (pid1)
        exit(1);
    else
        for (;;)
        {
            printf("Active: pid=%d, ppid=%d\n", getpid(), getppid());
            sleep(1);
        }
}