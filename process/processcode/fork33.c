#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int pid;
    pid = fork();
    pid = fork();
    pid = fork();

    // seven new processes are created, in addition to the parent process.

    if (pid == 0)
    {
        for (;;)
            ;
    }
    else if (pid < 0)
    {
        printf("Error Forking");
    }
    else
    {
        for (;;)
            ;
    }
}
