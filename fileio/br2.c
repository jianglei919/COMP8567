#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main()
{
    int fd = open("check.txt", O_RDWR);
    if (fd == -1)
    {
        printf("\n The operation was not successful\n");
        return 1;
    }
    else
    {
        printf("\n The file descriptor is %d \n", fd);

        char buff[100];
        long int n = read(fd, buff, 3);
        printf("The number of bytes read is %ld\n", n);
        printf("The read bytes were %s\n", buff);

        n = read(fd, buff, 3);
        printf("The number of bytes read is %ld\n", n);
        printf("The read bytes were %s\n", buff);

        n = read(fd, buff, 100);
        printf("The number of bytes read is %ld\n", n);
        printf("The read bytes were %s\n", buff);

    }
    close(fd);
}