#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

int main(int argc, char *argv[])
{
    pid_t pid;

    if ((pid = fork()) > 0)
    { // Parent Process
        for (;;)
        {
            printf("Parent process id is %d\n", getpid());
            sleep(2);
        }
    }
    else
    { // Child Process
        int k = 1;
        for (;;)
        {
            printf("Child process id is %d  %d\n", getpid(), getppid());
            sleep(2);
            k = k + 1;

            if (k == 5)
            {
                printf("The child process sends SIGINT to the parent process\n");
                kill(getppid(), SIGINT);
            }
            /*
            if(k==9){
            printf("The child process sends SIGINT to itself\n");
            kill(getpid(),SIGINT)//Child kills self
            } */
        } // End for
    } // End child
}
