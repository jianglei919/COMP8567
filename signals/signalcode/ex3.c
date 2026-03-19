#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

int main(int argc, char *argv[])
{

    void (*retval)(int); // Declare a pointer to a function that receives one integer and returns void

    printf("\nReplacing SIG_DFL with SIG_IGN for the SIGINT signal\n");

    retval = signal(SIGINT, SIG_IGN); // ignore CTR-C

    for (int i = 1; i <= 10; i++)
    {
        printf("I am not sensitive to CTR-C\n");
        sleep(1);
    }

    printf("\nReplacing SIG_IGN with SIG_DFL for the SIGINT signal\n");

    signal(SIGINT, retval);

    printf("\n\n");

    for (int i = 1; i <= 10; i++)
    {
        printf("I am sensitive to CTR-C\n");
        sleep(1);
    }
}
