#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main()
{
    int fd[2];

    pipe(fd);

    pid_t pid = fork();

    if (pid > 0)
    { // parent -> ls
        close(fd[0]);
        dup2(fd[1], 1); // Parent’s output will be written into the pipe
        close(fd[1]);
        execlp("ls", "ls", (char *)NULL);
        perror("execlp ls");
        exit(1);
    }
    else
    { // child -> wc -w
        close(fd[1]);
        dup2(fd[0], 0); // Child’s input will be read from the pipe
        close(fd[0]);
        execlp("wc", "wc", "-w", (char *)NULL);
        perror("execlp wc");
        exit(1);
    }
    return 0;
}