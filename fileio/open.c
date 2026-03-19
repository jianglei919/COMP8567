#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main()
{
    // int fd = open("new.txt", O_CREAT | O_RDONLY, 0777);
    int fd = open("new123.txt", O_RDWR);
    if (fd == -1)
    {
        printf("\n The operation was not successful\n");
        return 1;
    }
    else
    {
        printf("\n The file descriptor is %d \n", fd);
    }
    close(fd);
}