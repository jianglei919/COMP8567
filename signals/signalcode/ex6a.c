// Swtiches between Parent and Child processes using kill()
#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

void action()
{ // User defined handler
    printf("Switching\n");
}

int main(int argc, char *argv[])
{
    pid_t pid;

    if ((pid = fork()) > 0) // Parent
    {
        signal(SIGALRM, action); // Register Handler
        while (1)
        {
            printf("Parent is running with PID %d and PPID %d\n", getpid(), getppid());
            sleep(2);
            kill(pid, SIGALRM); // Send signal to child
            pause();            // Wait for signal from the child process
        }
    }
    else
    { // Child
        signal(SIGALRM, action);
        while (1)
        {
            pause(); // Wait for signal from the parent process
            printf("Child is running with PID %d and PPID %d\n", getpid(), getppid());
            sleep(2);
            kill(getppid(), SIGALRM); // Send signal to parent
        }
    }
} // End main
