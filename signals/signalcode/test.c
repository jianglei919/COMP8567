#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

// Replaces the default handler with a user-defined handler

void handler(int signo)
{
    printf("In the handler\n");
    alarm(3);
}

int main(int argc, char *argv[])
{
    signal(SIGALRM, handler);
    alarm(3);

    while (1)
    {
        printf("I am working\n");
        sleep(1);
    }
    printf("from main\n");
}
