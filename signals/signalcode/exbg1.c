#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
    for (;;)
    {
        sleep(10);
        printf("Hello! from the process\n");
    }
}
