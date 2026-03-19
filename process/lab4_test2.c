#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main()
{
    // wirte a example that the main process wait for all child processes to terminate
    int num_children = 3;
    for (int i = 0; i < num_children; i++)
    {
        int pid = fork();
        if (pid < 0)
        {
            perror("Fork failed!\n");
            exit(1);
        }
        if (pid == 0)
        {
            // child process
            printf("Child process %d: pid is %d, ppid is %d\n", i + 1, getpid(), getppid());
            sleep(1 + i);
            exit(i + 1); // exit with different status
        }
    }
    // parent process
    for (int i = 0; i < num_children; i++)
    {
        int status;
        int child_pid = wait(&status);
        if (WIFEXITED(status))
        {
            printf("Parent process: Child with pid %d terminated normally with exit status %d\n", child_pid, WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            printf("Parent process: Child with pid %d terminated by signal %d\n", child_pid, WTERMSIG(status));
        }
    }

    return 0;
}