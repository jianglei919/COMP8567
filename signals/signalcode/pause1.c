#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

void handler(int signo)
{
    printf("The signal number is %d\n", signo);
}

int main(int argc, char *argv[])
{
    pid_t pid;

    if ((pid = fork()) > 0)
    { // Parent Process
        signal(SIGTSTP, handler);
        printf("We are in the parent process\n");
        int ret = pause();
        printf("The ret value is %d\n", ret);
    }
    else
    {
        printf("We are in the child process\n");
        sleep(2);
        kill(getppid(), SIGTSTP);
    } // End child
}
