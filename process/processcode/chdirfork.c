#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

int main()
{
    int p, p1;
    p = fork();

    if (p == -1)
    {
        printf("There is an error while calling fork()");
    }
    if (p == 0)
    {
        printf("\nWe are in the child process\n");
        char *s;
        // current working directory"\nThe new working directory of the child process is %s\n",getcwd(s,50));
        // new working directory
        // printf("\nThe new working directory of the child process is %s\n",getcwd(s,50));
        //  new working directory

        printf("\nThe current working directory of the child process is %s\n", getcwd(s, 50));
        // using the system call chdir()
        chdir("/home/pranga/Documents");

        printf("\nThe new working directory of the child process is %s\n", getcwd(s, 50));
        // new working directory

        int fd = open("new5.txt", O_CREAT | O_RDWR, 0777);
        if (fd == -1)
            printf("\nThe new file was not created\n");
        else
            printf("\nThe new file was created with a fd %d\n", fd);

        exit(0);
    }
    else
    {
        int k = wait();
        printf("\nParent process resumes\n");
        char *s;
        exit(0);
    }
    return 0;
}
