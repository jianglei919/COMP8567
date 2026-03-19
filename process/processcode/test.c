#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>

int main(void)
{
        int a = 100, b = 200, c = 300;
        for (int k = 0; k <= 10; k++)
        {
                printf("Welcome to COMP 8567\n");
        }

        int fd3 = open("check.txt", O_RDONLY);
        printf("\nThe value of fd3 from main is %d \n", fd3);
        char buff[10];
        int ret = read(fd3, buff, 10);
        printf("\nThe value of ret from main is %d \n", ret);
        printf("\nThe contents of the file read from the main process are %s\n", buff);

        int i = fork();
        if (i == 0)
        {
                a = a + 10;
                b = b + 20;
                c = c + 30;
                char buff1[10];
                int retc = read(fd3, buff1, 10);
                printf("\nThe value of retc is %d \n", retc);
                printf("\nThe values of a, b and c from the child process are %d %d %d \n", a, b, c);
                printf("\nThe contents of the file read from the child process are %s\n", buff1);
                printf("\n\nCHILD PROCESS\n");
                printf("\n The id of the child process is %d \n", getpid());
                printf("\n The parent id of the child process is %d \n", getppid());
                sleep(1);

                exit(0);
        }
        else if (i < 0)
        {
                printf("\n\nERROR\n");
        }
        else
        {
                char buff1[10];
                int retp = read(fd3, buff1, 10);
                printf("\nThe value of retp is %d \n", retp);
                printf("\nThe values of a, b and c from the child process are %d %d %d \n", a, b, c);
                printf("\nThe contents of the file read from the parent process are %s\n", buff1);
                printf("\n\nPARENT PROCESS\n");
                printf("\n The id of the parent process is %d \n", getpid());
                printf("\n The parent id of parent process is %d \n", getppid());
                exit(0);
        }
        sleep(2);
        close(fd3);
}
