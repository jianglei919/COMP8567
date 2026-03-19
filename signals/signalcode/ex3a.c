#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

void handler1(int signo)
{
    printf("From handler 1 signal %d received returning to main\n", signo);
}

void handler2(int signo)
{
    printf("From handler 2 signal %d received returning to main\n", signo);
}

int main(int argc, char *argv[])
{

    void (*retval1)(int); // Declare a pointer to a function that receives one integer and returns void
    void (*retval2)(int); // Declare a pointer to a function that receives one integer and returns void
    void (*retval3)(int); // Declare a pointer to a function that receives one integer and returns void

    retval1 = signal(SIGINT, handler1); // reval1=SIG_DFL

    for (int i = 1; i <= 10; i++)
    {
        printf("Handler 1 is in effect for SIGINT\n");
        sleep(1);
    }

    printf("\nReplacing handler1 with retval1(SIG_DFL)for SIGINT\n");
    retval2 = signal(SIGINT, retval1); // retval2=handler1

    for (int i = 1; i <= 10; i++)
    {
        printf("SIG_DFL is in effect for SIGINT\n");
        sleep(1);
    }

    printf("\nReplacing SIG_DFL with handler1 for SIGINT\n");
    retval3 = signal(SIGINT, retval2); // retval3=SIG_DFL

    for (int i = 1; i <= 10; i++)
    {
        printf("Handler1 is back in effect for SIGINT\n");
        sleep(1);
    }

} // END
