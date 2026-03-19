#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>

int main(void)
{

	int fd = open("check.txt", O_RDWR);
	int num = 100;
	int i = fork();
	if (i == 0)
	{

		printf("\n\nCHILD PROCESS\n");
		printf("\n The id of the child process is %d \n", getpid());
		printf("\n The parent i0d of the child process is %d \n", getppid());

		char *buffc = "CHILD";
		int n = write(fd, buffc, 5);
		printf("\nThe number of bytes written by the child process is %d\n", n);
		num = num + 100;
		printf("\nThe value of num in the child process is %d\n", num);

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
		char *buffp = "PARENT";
		int n = write(fd, buffp, 6);
		printf("\nThe number of bytes written by the parent process is %d\n", n);
		num = num + 200;
		printf("\nThe value of num in the child process is %d\n", num);
		exit(0);
	}
}
