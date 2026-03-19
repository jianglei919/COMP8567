// Swtiches between Parent and Child processes using kill()
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // 必须加
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
        signal(SIGINT, action); // Register Handler，如果改成SIGALRM，则父进程和子进程都会被ctrl+c杀死
        while (1)
        {
            printf("Parent is running with PID %d and PPID %d\n", getpid(), getppid());
            sleep(2);
            kill(pid, SIGINT); // Send signal to child
            pause();           // Wait for signal from the child process
        }
    }
    else
    { // Child
        signal(SIGINT, action);
        while (1)
        {
            pause(); // Wait for signal from the parent process
            printf("Child is running with PID %d and PPID %d\n", getpid(), getppid());
            sleep(2);
            kill(getppid(), SIGINT); // Send signal to parent
        }
    }
} // End main
