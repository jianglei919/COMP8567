#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

int main(int argc, char *argv[])
{
    // signal(SIGINT,SIG_IGN);

    for (;;)
    {
        printf("Hello and welcome to COMP 8567\n");
        sleep(1);
    }
}
