#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // 必须加
#include <sys/signal.h>

// 父进程死后, 子进程的 PGID 仍然是原来的, 但终端前台进程组已经变成 shell, 子进程不再属于当前前台进程组shell
// Ctrl+C → 发给前台进程组, 父进程死后 → shell 成为前台, 子进程不再接收 Ctrl+C
int main(int argc, char *argv[])
{
    pid_t pid;

    if ((pid = fork()) > 0)
    { // Parent Process
        for (;;)
        {
            printf("Parent process id is %d, pgid=%d\n", getpid(), getpgid(0));
            sleep(2);
        }
    }
    else
    { // Child Process
        // signal(SIGINT, SIG_DFL); //无效, 因为但父进程被杀后，shell认为任务结束了，会把自己设置为前台进程组，因此子进程不再属于当前前台进程组shell, 不再接收 Ctrl+C
        int k = 0;
        for (;;)
        {
            printf("Child process id is %d, ppid=%d, pgid=%d\n", getpid(), getppid(), getpgid(0));
            if (k == 5)
            {
                printf("\nThe parent process will now be killed\n"); // killed after 10 seconds
                kill(getppid(), SIGINT);
            }
            sleep(2);
            k = k + 1;
        }
    }
}
