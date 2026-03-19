#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

int main()
{
    int p;
    p = fork();

    if (p == -1)
    {
        printf("There is an error while calling fork()");
    }
    if (p == 0)
    {
        int k;
        printf("\nWe are in the child process\n");
        printf("\nThe child process will now be differentiated(replaced) \n");
        // int i = execl("/bin/ls", "bin/ls", "-1", NULL);
        // int i = execl("/bin/ls", "bin/ls", NULL);
        k = execl("/home/pranga/chapter5/welcome1", "/home/pranga/chapter5/welcome", NULL);

        printf("\nThe ret value is %d\n", k);
        printf("\nWe are back in the child process\n");
    }
    else
    {
        int k = wait();
        printf("\nWe are in the parent process\n");
        exit(0);
    }
    return 0;
}
