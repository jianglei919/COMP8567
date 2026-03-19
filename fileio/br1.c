#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
    // Reads from check.txt into an array of characters and print it
    int fd3 = open("check.txt", O_RDONLY);
    char *buff1;
    long int n;
    n = read(fd3, buff1, 10);
    printf("The number of bytes read is %ld\n", n);
    printf("%s\n", buff1);
    close(fd3);
}