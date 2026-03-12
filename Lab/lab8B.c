#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

/*
 * Part B (requirement: 2 pipes + 2 forks):
 * Implement: date | wc -c | wc
 *
 * Process plan (3 commands, but only 2 forks):
 *  - Child #1: exec "date"        (stdout -> pipe1 write)
 *  - Child #2: exec "wc -c"       (stdin <- pipe1 read, stdout -> pipe2 write)
 *  - Parent  : exec "wc"          (stdin <- pipe2 read)
 *
 * Note about zombies:
 *  - Parent execs wc, so it cannot waitpid().
 *  - To avoid zombies while still using only 2 forks, parent ignores SIGCHLD
 *    so terminated children are auto-reaped by the kernel.
 */

static void die(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(void)
{
    int p1[2]; // date -> wc -c
    int p2[2]; // wc -c -> wc

    // Avoid zombies in the 2-fork design (parent will exec wc, cannot wait)
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
        die("signal(SIGCHLD)");

    if (pipe(p1) == -1)
        die("pipe(p1)");
    if (pipe(p2) == -1)
        die("pipe(p2)");

    // fork #1: date
    int pid1 = fork();
    if (pid1 < 0)
        die("fork(pid1)");

    if (pid1 == 0)
    {
        // Child #1: date (stdout -> p1[1])
        close(p1[0]);
        if (dup2(p1[1], STDOUT_FILENO) < 0)
            die("dup2(date->p1[1])");
        close(p1[1]);

        // p2 unused in this child
        close(p2[0]);
        close(p2[1]);

        execlp("date", "date", (char *)NULL);
        die("execlp(date)");
    }

    // fork #2: wc -c
    int pid2 = fork();
    if (pid2 < 0)
        die("fork(pid2)");

    if (pid2 == 0)
    {
        // Child #2: wc -c (stdin <- p1[0], stdout -> p2[1])
        close(p1[1]);
        if (dup2(p1[0], STDIN_FILENO) < 0)
            die("dup2(p1[0]->stdin)");
        close(p1[0]);

        close(p2[0]);
        if (dup2(p2[1], STDOUT_FILENO) < 0)
            die("dup2(p2[1]->stdout)");
        close(p2[1]);

        execlp("wc", "wc", "-c", (char *)NULL);
        die("execlp(wc -c)");
    }

    // Parent: wc (stdin <- p2[0])
    // Parent does NOT use pipe1
    close(p1[0]);
    close(p1[1]);

    close(p2[1]);
    if (dup2(p2[0], STDIN_FILENO) < 0)
        die("dup2(p2[0]->stdin)");
    close(p2[0]);

    execlp("wc", "wc", (char *)NULL);
    die("execlp(wc)");

    return 0;
}