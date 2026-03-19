#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

// Return value and other features of the alarm() system call

int main(int argc, char *argv[])
{

    alarm(6);
    sleep(8);
    int ret = alarm(4);

    printf("\nThe return value of alarm() is: %d ", ret);

    while (1)
    {
        printf("\nSleep for a second\n");
        sleep(1);
    }

    // unsigned int n=alarm(3);
    // printf("\nThe value of n is %d\n",n);

    printf("Exiting on Alarm\n");
    exit(0);
}
