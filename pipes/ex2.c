#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{
    char *msg1 = "Hello child process!";

    int fd[2];
    if (pipe(fd) == -1)
    {
        perror("pipe");
        exit(1);
    }

    // 如果读写的fd搞反了，返回-1

    int len = strlen(msg1);

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(1);
    }
    else if (pid == 0)
    {
        // Child process
        close(fd[1]); // Close the write end of the pipe in the child process

        char buff1[30]; // Buffer to store the message read from the pipe
        printf("\nFrom the Child Process:Parent has sent the following message:\n");
        int n = 0;

        n = read(fd[0], buff1, 5); // Read message from the pipe
        printf("\n%s", buff1);
        printf("\nThe number of characters read is %d\n", n);

        // n = read(fd[0], buff1, 5); // Read message from the pipe
        // printf("\n%s", buff1);
        // printf("\nThe number of characters read is %d\n", n);

        // n = read(fd[0], buff1, len + 1); // Read message from the pipe
        // buff1[n] = '\0';                 // Null-terminate the string
        // printf("\n%s", buff1);
        // printf("\nThe number of characters read is %d\n", n);

        // for (int i = 1; i < 20; i++)
        // {
        //     printf("Before read n=%d\n", n);
        //     n = read(fd[0], buff1, 3); // Read message from the pipe
        //     if (n > 0)
        //         buff1[n] = '\0'; // 关键：把它变成合法C字符串
        //     printf("The %dth read: n = %d", i, n);
        //     if (n <= 0)
        //     {
        //         printf("\nNo more characters to read from the pipe. n = %d\n", n);
        //         break; // Exit the loop if there are no more characters to read
        //     }
        //     printf("\nThe read content is: %s\n\n", buff1);
        //     // printf("\nThe number of characters read is %d\n", n);
        // }

        exit(0);
    }
    else
    {
        // Parent process
        // close(fd[0]); // Close the read end of the pipe in the parent process
        printf("\nFrom the Parent Process: Sending the following message to the child process:\n%s\n", msg1);
        int n = write(fd[1], msg1, len); // Write message to the pipe
        printf("The number of characters written into the pipe is %d\n", n);

        close(fd[1]); // Close the write end of the pipe in the parent process

        wait(NULL); // Wait for the child process to finish
    }
}