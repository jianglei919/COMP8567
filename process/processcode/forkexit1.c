#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

// Termination status of the child process using macros defined in sys/wait.h
// WIFEXITED(),WIFSIGNALED(),WEXITSTATUS(),WTERMSIG()

void main()
{
  int i, num;

  int pid = fork();

  if (pid == 0)
  {

    // num=num/0;

    for (int i = 0; i < 100; i++)
    {
      sleep(1);
      printf("\nFrom the child process: PID = %d\n", getpid());
      printf("\nFrom the child process: PPID = %d\n", getppid());
    }

    //  exit(256);
  }
  else if (pid < 0)
  {
    // error
  }
  else
  {
    int status;
    int k = wait(&status);
    printf("\nThe parent process has resumed execution\n");

    if (WIFEXITED(status))
    {
      printf("Normal Exit: %d \n", WEXITSTATUS(status));
    }
    else // WIFSIGNALED(status) is true
    {
      printf("Signalled Exit: %d\n", WTERMSIG(status));
    }
  }
}
