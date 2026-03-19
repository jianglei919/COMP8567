#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

// Replaces the default handler with a user-defined handler

void handler(int signo)
{
    printf("signal %d received, no action taken,returning to main\n", signo);
    // Additional action can be performed if required by the programmer*/
}

void handler1(int signo)
{

    printf("handler 1 -signal %d received, no action taken,returning to main\n", signo);

    // Additional action can be performed if required by the programmer*/
}

int main(int argc, char *argv[])
{

    signal(SIGALRM, handler);
    signal(SIGINT, handler);

    // System alarm goes off 5 seconds from this point
    alarm(5);

    while (1)
    {
        printf("I am working\n");
        sleep(1);
    }
    printf("from main\n");
}
