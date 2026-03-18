#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main()
{
    char buff1[10];

    int fd1 = open("file1.txt", O_RDONLY);
    int fd2 = open("file2.txt", O_RDONLY);

    dup2(fd1, fd1); // dup2(fd1, fd1) does nothing, but it is a valid call

    ssize_t n;

    n = read(fd1, buff1, 1);
    printf("Before dup2, fd1 read: %s\n", buff1);

    n = read(fd2, buff1, 2);
    printf("Before dup2, fd2 read: %s\n", buff1);

    int m = lseek(fd1, 0, SEEK_CUR);
    printf("Before dup2, fd1 current position: %d\n", m);

    m = lseek(fd2, 0, SEEK_CUR);
    printf("Before dup2, fd2 current position: %d\n", m);

    // ----------dup2----------

    if (dup2(fd1, fd2) == -1)
    {
        perror("dup2");
        exit(1);
    }

    n = read(fd1, buff1, 3);
    printf("\nAfter dup2, fd1 read: %s\n", buff1);

    n = read(fd2, buff1, 4); // 这个时候的fd2会操作file1.txt文件
    printf("After dup2, fd2 read: %s\n", buff1);

    int k = lseek(fd1, 0, SEEK_CUR); // Reset file offset for fd1
    printf("After dup2, fd1 current position: %d\n", k);

    k = lseek(fd2, 0, SEEK_CUR); // Reset file offset for fd2
    printf("After dup2, fd2 current position: %d\n", k);

    // dup2后，fd1和fd2共享offset
    return 0;
}