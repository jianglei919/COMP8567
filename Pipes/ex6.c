#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main()
{
    int p1[2]; // 定义第1个管道 p1
    if (pipe(p1) == -1)
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

        close(p1[0]);   // 关闭管道 p1 的读端，因为父进程只需要写入数据到管道p1
        dup2(p1[1], 1); // 父进程的 out 将被写入管道p1
        close(p1[1]);   // 写完关闭避免读阻塞
        execlp("ls", "ls", (char *)NULL);
        perror("execlp ls");
        exit(1);
    }
    else
    {
        // child -> wc
        close(p1[1]);   // 关闭管道p1的写端，因为子进程只需要从管道 p1 读取数据
        dup2(p1[0], 0); // 子进程的 in 将从管道 p1 读取数据
        close(p1[0]);   // 读完关闭避免写阻塞

        int p2[2]; // 定义第2个管道 p2

        // 这里我们创建第二个管道 p2 来连接 wc 和 wc -w
        if (pipe(p2) == -1)
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
            close(p2[0]);   // 关闭管道 p2 的读端，因为子进程 wc 只需要写入数据到管道 p2
            dup2(p2[1], 1); // 子进程 wc 的 out 将被写入管道 p2
            close(p2[1]);   // 写完关闭避免读阻塞
            execlp("wc", "wc", (char *)NULL);
            perror("Child execlp wc");
            exit(1);
        }
        else
        {
            // Child's child -> wc -w
            close(p2[1]);   // 关闭管道 p2 的写端，因为子进程的子进程 wc -w 只需要从管道 p2 读取数据
            dup2(p2[0], 0); // 子进程的子进程的 in 将从管道 p2 读取数据
            close(p2[0]);   // 读完关闭避免写阻塞
            execlp("wc", "wc", "-w", (char *)NULL);
            perror("Child's child execlp wc -w");
            exit(1);
        }
    }
    return 0;
}