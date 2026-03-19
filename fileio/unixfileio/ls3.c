#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

// ls2.c with SEEK_CUR

int main()
{

    int fd3 = open("check.txt", O_RDWR);

    long int n;

    char *buff1 = "COMP 8567";

    n = lseek(fd3, 10000000, SEEK_END);

    printf("\nThe resulting offset is %ld\n", n);

    n = write(fd3, buff1, 9);
    printf("\nThe no of bytes written from the resulting offset is %ld\n", n);

    n = lseek(fd3, 0, SEEK_CUR);
    printf("\nThe resulting offset after write is %ld\n", n);

    close(fd3);
    return 0;
}
