#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

// umaskex.c
int main()
{

    umask(0000);
    int fd1 = open("check1010.txt", O_CREAT | O_RDWR, 0777);
    return 0;
}
