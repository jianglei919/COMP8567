#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main(void)
{

	int i = fork();
	if (i == 0)
	{
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
		printf("\n\nPARENT PROCESS\n");
		printf("\n The id of the parent process is %d \n", getpid());
		printf("\n The parent id of parent process is %d \n", getppid());
		exit(0);
	}
}
