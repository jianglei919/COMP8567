#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

static void die(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(void)
{
    int pipe1[2]; // pipe1: ls  --> wc
    int pipe2[2]; // pipe2: wc  --> wc -w

    // ✅ 必须在 fork 之前创建 pipe，这样 fork 出来的进程才能继承到有效 fd
    if (pipe(pipe1) == -1)
        die("pipe pipe1");
    if (pipe(pipe2) == -1)
        die("pipe pipe2");

    // ========= fork #1: parent -> ls, child -> (wc | wc -w part) =========
    pid_t pid1 = fork();
    if (pid1 < 0)
        die("fork pid1");

    if (pid1 > 0)
    {
        // ================= parent: ls =================
        // stdout(1) -> pipe1[1]  (把 ls 输出写进 pipe1)
        if (dup2(pipe1[1], STDOUT_FILENO) == -1)
            die("dup2 pipe1[1] -> stdout");

        // parent 不需要的 fd 全部关闭
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);

        execlp("ls", "ls", (char *)NULL);
        die("execlp ls");
    }
    else
    {
        // ================= child: 负责后两段：wc 和 wc -w =================

        // ========= fork #2: parent -> wc, child -> wc -w =========
        pid_t pid2 = fork();
        if (pid2 < 0)
            die("fork pid2");

        if (pid2 > 0)
        {
            // --------------- child (pid2's parent): wc ---------------
            // stdin(0)  <- pipe1[0]  (从 pipe1 读 ls 的输出)
            // stdout(1) -> pipe2[1]  (把 wc 的输出写进 pipe2)
            if (dup2(pipe1[0], STDIN_FILENO) == -1)
                die("dup2 pipe1[0] -> stdin");
            if (dup2(pipe2[1], STDOUT_FILENO) == -1)
                die("dup2 pipe2[1] -> stdout");

            // 关闭不需要的 fd
            close(pipe1[0]);
            close(pipe1[1]);
            close(pipe2[0]);
            close(pipe2[1]);

            execlp("wc", "wc", (char *)NULL);
            die("execlp wc");
        }
        else
        {
            // --------------- child's child: wc -w ---------------
            // stdin(0)  <- pipe2[0]  (从 pipe2 读)
            if (dup2(pipe2[0], STDIN_FILENO) == -1)
                die("dup2 pipe2[0] -> stdin");

            // 这个进程不需要的 fd 全部关闭（否则可能导致 EOF 迟迟不来）
            close(pipe1[0]);
            close(pipe1[1]);
            close(pipe2[0]);
            close(pipe2[1]);

            execlp("wc", "wc", "-w", (char *)NULL);
            die("execlp wc -w");
        }
    }

    // 正常情况下不会执行到这里，因为三个进程都会 exec
    return 0;
}