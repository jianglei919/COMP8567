#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main()
{

    int num = 0;
    int pid = fork();
    if (pid > 0)
    {
        num += 100;
    }
    else
    {
        num += 200;
    }
    num += 50;
    if (pid > 0)
    {
        printf("num=%d\n", num);
    }
}