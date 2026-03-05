#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main()
{

    int input_num;
    printf("Please enter a number: ");
    int ret = scanf("%d", &input_num);
    if (ret != 1) { // check if input is valid
        printf("Failed to read input number.\n");
        return 1; 
    }

    for (;;) 
    {
        // validate input number
        if (input_num < 1 || input_num > 3)
        {
            printf("Invalid number! Please enter 1, 2, or 3.\n");

            // prompt for input again
            printf("Please continue to enter a number: ");
            ret = scanf("%d", &input_num);
            if (ret != 1) { // check if input is valid
                printf("Failed to read input number.\n");
                return 1; 
            }
        }
        else
            break;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("Fork failed!\n");
        exit(1);
    }
    if (pid == 0)
    {
        // child process
        if (input_num == 1)
        {
            // implicit exit witheout using exit()
            printf("Child process: pid is %d, ppid is %d\n", getpid(), getppid());
        }
        else if (input_num == 2)
        {
            printf("Child process: pid is %d, ppid is %d\n", getpid(), getppid());
            // explicit exit with exit code 254
            exit(254);
        }
        else if (input_num == 3)
        {
            // Child process prints its pid and ppid 5 times in a loop with 1 second interval and then executes the statement int n = 15/ 0;
            for (int i = 0; i < 5; i++)
            {
                printf("Child process: pid is %d, ppid is %d\n", getpid(), getppid());
                // 1 second interval
                sleep(1);
            }
            int n = 15 / 0;
        }
        else
        {
            // invalid input
            printf("Invalid input number!\n");
            exit(1);
        }   
    }
    else
    {
        // parent process
        int status;
        wait(&status);
        if (WIFEXITED(status))
        {
            printf("Parent process: Normal Exit and the exit status is %d\n", WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            printf("Parent process: Signalled exit and the signal number is %d\n", WTERMSIG(status));
        }
    }
    return 0;
}