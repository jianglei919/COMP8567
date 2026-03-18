#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main()
{
    int pipe1[2]; // 定义第1个管道 pipe1
    if (pipe(pipe1) == -1)
    {
        perror("pipe");
        exit(1);
    }

    int pid1 = fork();
    if (pid1 < 0)
    {
        perror("fork");
        exit(1);
    }

    if (pid1 > 0)
    {
        // parent -> ls

        close(pipe1[0]);   // 关闭管道 pipe1 的读端，因为父进程只需要写入数据到管道p1
        dup2(pipe1[1], 1); // 父进程的 out 将被写入管道p1
        close(pipe1[1]);   // 写完关闭避免读阻塞
        execlp("ls", "ls", (char *)NULL);
        perror("execlp ls");
        exit(1);
    }
    else
    {
        // child -> wc
        close(pipe1[1]);   // 关闭管道p1的写端，因为子进程只需要从管道 pipe1 读取数据
        dup2(pipe1[0], 0); // 子进程的 in 将从管道 pipe1 读取数据
        close(pipe1[0]);   // 读完关闭避免写阻塞

        int pipe2[2]; // 定义第2个管道 pipe2

        // 这里我们创建第二个管道 pipe2 来连接 wc 和 wc -w
        if (pipe(pipe2) == -1)
        {
            perror("pipe");
            exit(1);
        }

        int pid2 = fork();
        if (pid2 < 0)
        {
            perror("fork");
            exit(1);
        }

        if (pid2 > 0)
        {
            // Child -> wc
            close(pipe2[0]);   // 关闭管道 pipe2 的读端，因为子进程 wc 只需要写入数据到管道 pipe2
            dup2(pipe2[1], 1); // 子进程 wc 的 out 将被写入管道 pipe2
            close(pipe2[1]);   // 写完关闭避免读阻塞
            execlp("wc", "wc", (char *)NULL);
            perror("Child execlp wc");
            exit(1);
        }
        else
        {
            // Child's child -> wc -w
            close(pipe2[1]);   // 关闭管道 pipe2 的写端，因为子进程的子进程 wc -w 只需要从管道 pipe2 读取数据
            dup2(pipe2[0], 0); // 子进程的子进程的 in 将从管道 pipe2 读取数据
            close(pipe2[0]);   // 读完关闭避免写阻塞
            execlp("wc", "wc", "-w", (char *)NULL);
            perror("Child's child execlp wc -w");
            exit(1);
        }
    }
    return 0;
}