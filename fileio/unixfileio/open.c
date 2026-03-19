// open.c
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

// Tries to open a file and displays an error if the file cannot be opened in the specified mode
// Prints the file descriptor (a non-negative integer) if open() is successful 1

int main(void)
{

    int fd1 = open("new.txt", O_CREAT | O_RDWR, 0777);
    // int fd1=open("check.txt",O_RDWR);
    // int fd1=open("check432324.txt",O_RDWR);
    // int fd1=open("/home/pranga/chapter4/check.txt",O_RDWR);

    if (fd1 == -1)
        printf("\n The operation was not successful\n");
    else
        printf("\n The file descriptor is %d\n", fd1);

    close(fd1);
}
