#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

void Handler(int signo)
{
    printf("\nIn the handler\n");
}

int main(int argc, char *argv[])
{

    // signal(SIGALRM,Handler); //install the handler

    alarm(5);

    printf("\nThe system is now pausing\n");

    int i = pause();

    printf("\nThe process has resumed after pause\n");

    printf("\nThe return value of pause() is %d\n", i);
}
