#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
    int fd3 = open("check.txt", O_RDWR);
    char *buff1 = "Hello";
    long int n;
    n = write(fd3, buff1, 8);

    printf("The number of bytes written were %ld\n", n);
    n = write(fd3, buff1, 8);
    printf("The number of bytes written were %ld\n", n);
    n = write(fd3, buff1, 8);
    printf("The number of bytes written were %ld\n", n);
    close(fd3);
}