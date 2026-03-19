#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

int main(int argc, char *argv[])
{
    pid_t pid;
    int p = getpid();

    if ((pid = fork()) > 0)
    { // Parent Process
        int i = 0;
        for (;;)
        {
            printf("Parent process id is %d\n", getpid());
            if (i == 5)
            {
                printf("\n Sending SIGSTOP the the child process\n"); // killed after 10 seconds
                kill(pid, SIGSTOP);
            }
            if (i == 10)
            {
                printf("\n Sending SIGCONT the the child process\n"); // killed after 10 seconds
                kill(pid, SIGCONT);
            }

            sleep(2);
            i = i + 1;
        }
    }
    else
    { // Child Process
        int k = 0;
        for (;;)
        {
            printf("Child process id is %d\n", getpid());
            sleep(2);
        }
    }
}
