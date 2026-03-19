#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
    int fd = open("ls4.txt", O_RDWR | O_CREAT, 0666);
    int ret = lseek(fd, -3, SEEK_CUR);
    // int ret1 = lseek(fd, 2, SEEK_CUR);
    char buff[5] = "Hello";
    write(fd, buff, 5);
    close(fd);
    printf("The new offset is %d\n", ret);
    // printf("The new offset is %d\n", ret1);
}