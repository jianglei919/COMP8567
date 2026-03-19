#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(void)
{
	int i = fork();

	if (i == 0)
	{
		printf("\n\nCHILD PROCESS\n");
		printf("\n The id of the child process is %d \n", getpid());
		printf("\n The parent id of the child process is %d \n", getppid());
		for (int k = 0; k < 4; k++)
		{
			printf("\nThe child process is currently running");
			sleep(1);
		}
		exit(3);
	}
	else if (i < 0)
	{
		printf("\n\nERROR\n");
	}
	else
	{
		int status;
		int cid = wait(&status);
		wait();
		printf("\n\nThe PARENT process had resumed execution and return value of wait() is: %d\n", cid);
		printf("The PID of the parent process is %d \n", getpid());
		exit(0);
	}
}
