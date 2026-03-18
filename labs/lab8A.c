#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * Part A:
 * Implement: ls > new.txt
 * Idea:
 * 1) open new.txt (create if not exist, truncate if exist)
 * 2) dup2(file_fd, STDOUT_FILENO) so stdout goes to new.txt
 * 3) exec ls
 */

int main(void)
{
    int fd = open("new.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
    {
        perror("open(new.txt)");
        exit(1);
    }

    // Redirect stdout (fd=1) to new.txt
    if (dup2(fd, STDOUT_FILENO) < 0)
    {
        perror("dup2");
        close(fd);
        exit(1);
    }

    // fd is no longer needed after dup2
    close(fd);

    // Execute: ls
    execlp("ls", "ls", (char *)NULL);

    // If execlp returns, it failed
    perror("execlp(ls)");
    exit(1);
}