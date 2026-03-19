// Swtiches between Parent and Child processes using kill()
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // 必须加
#include <sys/signal.h>
// Switching with SIGFPE
// To illustrate Kill() can be used send any signal

void action()
{ // User defined handler
    printf("Switching\n");
    sleep(2);
}

int main(int argc, char *argv[])
{
    pid_t pid;
    if ((pid = fork()) > 0) // Parent
    {
        signal(SIGTERM, action); // Register Handler
        while (1)
        {
            printf("Parent is running with PID %d and PPID %d\n", getpid(), getppid());
            sleep(1);
            kill(pid, SIGTERM); // Send signal to child
            pause();            // Wait for signal from the child process
        }
    }
    else
    { // Child
        signal(SIGTERM, action);
        while (1)
        {
            pause(); // Wait for signal from the parent process
            printf("Child is running with PID %d and PPID %d\n", getpid(), getppid());
            kill(getppid(), SIGTERM); // Send signal to parent
        }
    }
} // End main
