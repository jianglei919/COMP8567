#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    // Three consecutive fork() statements
    pid_t pid1 = fork();
    pid_t pid2 = fork();
    pid_t pid3 = fork();

    // **Determine which process this is**
    if (pid1 > 0 && pid2 > 0 && pid3 > 0) {
        printf("Main Process: PID=%d, Parent PID=%d\n", getpid(), getppid());
        while(1)
        sleep(2);
    }
    else if (pid1 == 0 && pid2 > 0 && pid3 > 0) {
        printf("Child 1: PID=%d, Parent PID=%d\n", getpid(), getppid());
        while(1)
        sleep(2);
    }
    else if (pid1 > 0 && pid2 == 0 && pid3 > 0) {
        printf("Child 2: PID=%d, Parent PID=%d\n", getpid(), getppid());
        while(1)
        sleep(2);

    }

   else if (pid1 > 0 && pid2 > 0 && pid3 == 0) {
        printf("Child 3: PID=%d, Parent PID=%d\n", getpid(), getppid());
        sleep(3);
    }
    else if (pid1 == 0 && pid2 == 0 && pid3 > 0) {
        printf("Grandchild 1 (GC1): PID=%d, Parent PID=%d\n", getpid(), getppid());
        while(1)
        sleep(2);

    }
    else if (pid1 > 0 && pid2 == 0 && pid3 == 0) {
        printf("Grandchild 2 (GC2): PID=%d, Parent PID=%d\n", getpid(), getppid());
        sleep(3);
    }
    else if (pid1 == 0 && pid2 > 0 && pid3 == 0) {
        printf("Grandchild 3 (GC3): PID=%d, Parent PID=%d\n", getpid(), getppid());
        sleep(3);
    }
    else if (pid1 == 0 && pid2 == 0 && pid3 == 0) {
        printf("Great-Grandchild (GGC): PID=%d, Parent PID=%d\n", getpid(), getppid());
        sleep(3);

    }

    return(0);

} 