#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
// Changing process group id

void CTR_handler()
{ // handler
    printf("Process %d received CTR-C, exiting\n", getpid());
    exit(0);
}

int main(int argc, char *argv[])
{
    int i, pid;
    signal(SIGINT, CTR_handler); // register the handler

    if ((pid = fork()) == 0) // Child Process
    {

        printf("\nChild PID %d, PGID before setpgid() = %d\n", getpid(), getpgid(0));
        setpgid(0, getpid()); // child’s group id is set to child’s PID
        sleep(1);
        printf("\nChild PID %d, PGID after setpgid() = %d\n", getpid(), getpgid(0));

        for (;;)
        {
            sleep(2);
            printf("Child PID= %d, PGID=%d\n", getpid(), getpgid(0));
        }
    }
    else // Parent Process
    {
        for (;;)
        {
            sleep(2);
            printf("Parent PID= %d, PGID=%d\n", getpid(), getpgid(0));
        }
    }

    exit(0);
} // End Main
