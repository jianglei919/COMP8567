
// bw1.c
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{

    // Writes into check.txt into an array of characters

    int fd5 = open("check.txt", O_RDWR);

    char *buff1 = "Hello";

    long int n;

    n = write(fd5, buff1, 10);
    printf("\n\nThe number of bytes written were %ld\n", n);
    n = write(fd5, buff1, 10);
    printf("\n\nThe number of bytes written were %ld\n", n);
    n = write(fd5, buff1, 10);
    printf("\n\nThe number of bytes written were %ld\n", n);
    close(fd5);
}
