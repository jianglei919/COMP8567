#include <stdio.h>
#include <unistd.h>

int main(void)
{
	int i = fork();

	if (i == 0)
	{
		for (;;)
		{
			sleep(1);
			printf("\n\nCHILD PROCESS\n");
			printf("\n The id of the child process is %d \n", getpid());
			printf("\n The parent id of the child process is %d \n", getppid());
		}
	}
	else if (i < 0)
	{
		printf("\n\nERROR\n");
	}
	else
	{
		for (;;)
		{
			sleep(1);
			printf("\n\nPARENT PROCESS\n");
			printf("\n The id of the parent process is %d \n", getpid());
			printf("\n The parent id of the parent process is %d \n", getppid());
		}
	}
}
