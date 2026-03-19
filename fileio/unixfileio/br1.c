#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

int main(void)
{

    // Reads from check.txt into an array of characters
    int fd3 = open("check.txt", O_RDONLY);
    if (fd3 == -1)
    {
        printf("\n File does not exist\n");
        exit(1);
    }
    // char *buff1;
    char *buff1 = malloc(51 * sizeof(char));
    long int n;

    n = read(fd3, buff1, 5);

    printf("\n\nThe number of bytes read were %ld\n", n);

    printf("\nThe characters read from the file are %s\n", buff1);

    n = read(fd3, buff1, 5);

    printf("\n\nThe number of bytes read were %ld\n", n);

    printf("\nThe characters read from the file are %s\n", buff1);

    n = read(fd3, buff1, 50);

    printf("\n\nThe number of bytes read were %ld\n", n);

    printf("\nThe characters read from the file are %s\n", buff1);

    close(fd3);
}
