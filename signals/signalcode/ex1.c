#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

// Basic working of the alarm() system call

int main(int argc, char *argv[])
{

    alarm(7);

    while (1)
    {
        printf("Sleep for a second\n");
        sleep(1);
    }

    printf("Exiting on Alarm\n");
    exit(0);
}
