#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
    int fd3 = open("check.txt", O_RDWR);
    long int n = lseek(fd3, -60, SEEK_SET);
    printf("The new offset is %ld\n", n);
    char *buff1 = "COMP 8567";
    n = write(fd3, buff1, 9);
    printf("The no of bytes written from the resulting offset is %ld\n", n);
    close(fd3);
}