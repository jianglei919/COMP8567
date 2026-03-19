#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

// append.c

int main()
{

    // Appends buff1 at the end of check.txt

    int fd3 = open("check.txt", O_APPEND | O_RDWR);
    printf("The value of fd3 is %d\n", fd3);

    char buff[5];
    read(fd3, buff, 5);
    printf("%s\n", buff);

    read(fd3, buff, 5);
    printf("%s\n", buff);

    read(fd3, buff, 5);
    buff[2] = '\0';
    printf("%s\n", buff);

    int n = write(fd3, "COMP 8567 ", 10);
    printf("\n1th The number of bytes written were %d\n", n);

    int offset = lseek(fd3, 0, SEEK_SET);
    printf("\n1th The offset after lseek is %d\n", offset);

    n = write(fd3, "COMP 8567 ", 10);
    printf("\n2th The number of bytes written were %d\n", n);

    offset = lseek(fd3, 0, SEEK_SET);
    printf("\n2th The offset after lseek is %d\n", offset);

    n = write(fd3, "COMP 8567 ", 10);
    printf("\n3th The number of bytes written were %d\n", n);

    offset = lseek(fd3, 0, SEEK_SET);
    printf("\n3th The offset after lseek is %d\n", offset);

    n = write(fd3, "COMP 8567 ", 10);
    printf("\n4th The number of bytes written were %d\n", n);

    if (n < 0)
        printf("\nWrite unsuccessful\n");
    else
        printf("\nWrite successful\n");

    close(fd3);
}
