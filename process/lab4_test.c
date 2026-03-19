#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main()
{
    // fork operation
    int pid = fork();

    if (pid < 0)
    {
        // fork failed
        perror("Fork failed!\n");
    }
    if (pid == 0)
    {
        // this is child process
        printf("Please choose 1,2 or 3:\n");
        // get ueser input
        int k;
        scanf("%d", &k);

        if (k == 1)
        {
            // exits implicitly without using exit ()
            printf("Child process pid is %d,ppid is %d\n", getpid(), getppid());
        }
        else if (k == 2)
        {
            // exits explicitly using exit(3)
            printf("Child process pid is %d,ppid is %d\n", getpid(), getppid());
            exit(3);
        }
        else if (k == 3)
        {
            for (int i = 0; i < 3; i++)
            {
                // print 3 times
                printf("Child process pid is %d,ppid is %d\n", getpid(), getppid());
                // interval is one second
                sleep(1);
            }
            // make a signal exit
            int n = 30 / 0;
        }
        else
        {
            // unknown input
            printf("You give a invalid number!\n");
            exit(1);
        }
    }
    else
    {
        // this is parent process
        int status;
        // parent wait for child
        wait(&status);
        // normally exit
        if (WIFEXITED(status))
        {
            printf("Normal Exit and the exit status is:%d\n", WEXITSTATUS(status));
        }
        // exit by a signal
        else if (WIFSIGNALED(status))
        {
            printf("Signalled exit and the signal number is:%d\n", WTERMSIG(status));
        }
    }

    return 0;
}