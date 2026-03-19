#include <stdio.h>
#include <stdlib.h>
// waitpid().c

main()
{
    int status;
    int pid1, pid2;
    // waitpid(pid1,&status,0);
    pid1 = fork();
    pid2 = fork();

    if (pid1 > 0 && pid2 > 0)
    {
        printf("\nMain Process will now go into wait: %d %d\n", getpid(), getppid());
        int status;
        int child_pid = waitpid(pid2, &status, 0);
        int child_pid2 = waitpid(pid1, &status, 0);
        printf("\nThe main process resumed execution and was waiting for : %d\n", child_pid);
    }

    if (pid1 == 0 && pid2 > 0)
    {
        for (int i = 0; i < 5; i++)
        {
            sleep(1);
            printf("\nFirst child: %d %d\n", getpid(), getppid());
        }
    }

    if (pid1 > 0 && pid2 == 0)
    {
        printf("\nSecond child: %d %d\n", getpid(), getppid());
    }

    if (pid1 == 0 && pid2 == 0)
    {
        printf("\nGrandchild Process:%d %d\n", getpid(), getppid());
    }
}
