#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_CMD 256
#define MAX_ARGS 32

int main(void)
{
    char command[MAX_CMD];
    char *args[MAX_ARGS];

    printf("Hello, MiniBash!\n");

    while (1)
    {
        printf("minibash> ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            printf("\n");
            break;
        }

        command[strcspn(command, "\n")] = '\0';

        if (strlen(command) == 0)
            continue;

        if (strcmp(command, "exit") == 0)
            break;

        /* 参数拆分 */
        int i = 0;
        char *token = strtok(command, " ");
        while (token != NULL && i < MAX_ARGS - 1)
        {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork failed");
            continue;
        }
        else if (pid == 0)
        {
            execvp(args[0], args);
            perror("execvp failed");
            exit(EXIT_FAILURE);
        }
        else
        {
            int status;
            waitpid(pid, &status, 0);
        }
    }

    return 0;
}