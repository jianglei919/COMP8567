#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main(){
    pid_t ppid = getppid();
    printf("main pid: %d, ppid: %d\n", getpid(), ppid);
    fork();
    fork();
    fork();
    fork();
    printf("%d %d\n",getpid(),getpgid(0));
    setpgid(0, ppid);
    pause();
}