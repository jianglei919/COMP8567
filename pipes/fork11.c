#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

int main()
{
    int pid1 = fork();
    if (pid1 == 0)
    {
        // Child process 1
        printf("Child 1: PID = %d\n", getpid());

        for (;;)
            ;

        exit(0);
    }

    int pid2 = fork();
    if (pid2 == 0)
    {
        // Child process 2
        printf("Child 2: PID = %d\n", getpid());

        for (;;)
            ;

        exit(0);
    }

    int pid3 = fork();
    if (pid3 == 0)
    {
        // Child process 3
        printf("Child 3: PID = %d\n", getpid());

        for (;;)
            ;

        exit(0);
    }

    // Parent process
    printf("Parent: PID = %d\n", getpid());

    for (;;)
        ;

    // Wait for all child processes to finish
    wait(NULL);
    wait(NULL);
    wait(NULL);
}