#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>

// CTR-C goes to all processes in the process group

void CTR_handler(int signo)
{
    printf("Process %d received signal no: %d, exit\n", getpid(), signo);
    exit(0);
}

int main(int argc, char *argv[])
{
    int i;
    signal(SIGINT, CTR_handler); // Register the handler
    int main_pid = getpid();
    printf("The main process, PID= %d, PPID= %d, PGID= %d\n", getpid(), getppid(), getpgid(0));
    // getpgid(0) 0 indicates that the calling process' pgid will be returned

    fork();
    fork();
    sleep(1);
    if (getpid() == main_pid)
        kill(getpid(), SIGKILL);
    // signal(SIGINT, CTR_handler);//Register the handler
    sleep(1);
    printf("PID=%d PPID=%d PGID= %d\n", getpid(), getppid(), getpgid(0));
    pause(); // Wait for a singal
}
