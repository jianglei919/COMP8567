#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
// ls1.c with SEEK_SET

int main()
{

    int fd3 = open("check.txt", O_RDWR);

    int long n = lseek(fd3, 40, SEEK_SET);

    printf("\nThe resulting offset is %ld\n", n);

    char *buff1 = "COMP 8567";

    n = write(fd3, buff1, 9);

    printf("\nThe no of bytes written from the resulting offset is %ld\n", n);

    close(fd3);
    return 0;
}
