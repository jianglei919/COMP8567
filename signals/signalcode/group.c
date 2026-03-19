#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

int main()
{

    int a;
    int main_pid = getpid();
    printf("Main Process %d,%d,%d\n", getpid(0), getppid(0), getpgid(0));
    a = fork();
    a = fork();
    a = fork();
    printf("PID , PPID and PGID %d %d %d\n", getpid(0), getppid(0), getpgid(0));
    if (getpid() == main_pid)
    {
        sleep(8);
        printf("Main sends SIGINT to the entire group\n");
        kill(0, SIGINT);
    }
    else
        for (;;)
            ;
}
