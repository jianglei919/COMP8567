#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

// ls2.c with SEEK_CUR

int main()
{

    int fd3 = open("check.txt", O_RDWR);

    long int n;

    char *buff1 = "COMP 8567";

    n = write(fd3, buff1, 9);
    printf("\nThe no of bytes written is %ld\n", n);

    // n = lseek(fd3, 0, SEEK_CUR);
    // printf("\nThe resulting offset is %ld\n", n);

    n = write(fd3, buff1, 9);
    printf("\nThe no of bytes written is %ld\n", n);

    // n = lseek(fd3, 0, SEEK_CUR);
    // printf("\nThe resulting offset is %ld\n", n);

    n = lseek(fd3, 80, SEEK_CUR);

    printf("\nThe resulting offset is %ld\n", n);

    char *buff2 = "#########";

    n = write(fd3, buff2, 9);
    printf("\nThe no of bytes written is %ld\n", n);

    n = lseek(fd3, 0, SEEK_CUR);
    printf("\nThe resulting offset is %ld\n", n);

    close(fd3);
    return 0;
}
