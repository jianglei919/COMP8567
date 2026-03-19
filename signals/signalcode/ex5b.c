#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // 必须加
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
                printf("\nThe child process will now be killed\n"); // killed after 10 seconds
                kill(pid, SIGINT);
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
