#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
    int fd3 = open("check.txt", O_RDWR);
    int long n = lseek(fd3, 10, SEEK_SET);
    printf("The no of bytes written from the resulting offset is %ld\n", n);
    char *buff1 = "COMP 8567";
    n = write(fd3, buff1, 9);
    printf("The no of bytes written from the resulting offset is %ld\n", n);

    // SEEK_CUR
    n = lseek(fd3, 5, SEEK_CUR);
    printf("The new offset using SEEK_CUR is %ld\n", n);
    n = write(fd3, buff1, 9);
    printf("The no of bytes written from the resulting offset is %ld\n", n);

    close(fd3);
}