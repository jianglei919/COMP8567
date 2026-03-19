#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main(void)
{
    int i;

    /* 创建 3 个子进程 */
    for (i = 0; i < 3; i++)
    {
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork failed");
            exit(1);
        }

        if (pid == 0)
        {
            /* 子进程 */
            printf("Child %d started, PID=%d, PPID=%d\n", i, getpid(), getppid());

            sleep(1 + i); // 不同子进程运行时间不同
            printf("Child %d exiting\n", i);

            exit(10 + i); // 每个子进程返回不同的 exit code
        }
    }

    /* 父进程：等待所有子进程 */
    int status;
    pid_t child_pid;

    while ((child_pid = wait(&status)) > 0)
    {
        if (WIFEXITED(status))
        {
            printf("Parent: child PID=%d exited normally, status=%d\n",
                   child_pid, WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            printf("Parent: child PID=%d killed by signal %d\n",
                   child_pid, WTERMSIG(status));
        }
    }

    printf("Parent: all children have terminated\n");
    return 0;
}