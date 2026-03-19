#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
    int fd = open("new.txt", O_RDWR);
    char buff[5] = "Hello";
    write(fd, buff, 5);
    write(fd, buff, 5);
    int ret = lseek(fd, -11, SEEK_CUR); // 如果返回-1，说明出错了，不会改变当前的文件偏移量
    int ret1 = lseek(fd, 10, SEEK_CUR);
    close(fd);
    printf("The new offset is %d\n", ret);
    printf("The new offset is %d\n", ret1);
}