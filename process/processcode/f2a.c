#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>

int main(void)
{

	int fd = open("check.txt", O_RDWR);
	int i = fork();
	if (i == 0)
	{

		printf("\n\nCHILD PROCESS\n");

		char *buffc = "CHILD";
		int n = write(fd, buffc, 5);
		printf("\nThe number of bytes written by the child process is %d\n", n);

		exit(0);
	}
	else if (i < 0)
	{
		printf("\n\nERROR\n");
	}
	else
	{
		printf("\n\nPARENT PROCESS\n");
		char *buffp = "PARENT";
		int n = write(fd, buffp, 6);
		printf("\nThe number of bytes written by the parent process is %d\n", n);
		exit(0);
	}
}
