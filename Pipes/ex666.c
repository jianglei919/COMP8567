#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

int main()
{
    int pipe1[2], pipe2[2];

    // Create first pipe
    if (pipe(pipe1) == -1)
    {
        perror("pipe1");
        exit(EXIT_FAILURE);
    }

    // First child: pwd
    pid_t pid1 = fork();
    if (pid1 == -1)
    {
        perror("fork1");
        exit(EXIT_FAILURE);
    }

    if (pid1 == 0)
    {
        // Redirect stdout to pipe1's write end
        dup2(pipe1[1], STDOUT_FILENO);

        // Close unused pipe fds
        close(pipe1[0]);
        close(pipe1[1]);

        execlp("pwd", "pwd", NULL);
        perror("execlp pwd");
        exit(EXIT_FAILURE);
    }

    // Create second pipe
    if (pipe(pipe2) == -1)
    {
        perror("pipe2");
        exit(EXIT_FAILURE);
    }

    // Second child: wc
    pid_t pid2 = fork();
    if (pid2 == -1)
    {
        perror("fork2");
        exit(EXIT_FAILURE);
    }

    if (pid2 == 0)
    {
        // Redirect stdin from pipe1's read end
        dup2(pipe1[0], STDIN_FILENO);
        // Redirect stdout to pipe2's write end
        dup2(pipe2[1], STDOUT_FILENO);

        // Close all unused pipe fds
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);

        execlp("wc", "wc", NULL);
        perror("execlp wc");
        exit(EXIT_FAILURE);
    }

    // Close first pipe as it's no longer needed
    close(pipe1[0]);
    close(pipe1[1]);

    // Third child: wc -c
    pid_t pid3 = fork();
    if (pid3 == -1)
    {
        perror("fork3");
        exit(EXIT_FAILURE);
    }

    if (pid3 == 0)
    {
        // Redirect stdin from pipe2's read end
        dup2(pipe2[0], STDIN_FILENO);

        // Close all unused pipe fds
        close(pipe2[0]);
        close(pipe2[1]);

        execlp("wc", "wc", "-c", NULL);
        perror("execlp wc -c");
        exit(EXIT_FAILURE);
    }

    // Close second pipe as it's no longer needed
    close(pipe2[0]);
    close(pipe2[1]);

    // Wait for all child processes
    wait(NULL);
    wait(NULL);
    wait(NULL);

    return 0;
}
