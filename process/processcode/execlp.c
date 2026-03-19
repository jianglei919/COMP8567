#include <unistd.h>
#include <stdio.h>

int main(void)
{

  printf("\nThis program will be replaced using execlp()\n");

  //  char *programName = "/home/pranga/chapter5/welcome"; //Absolute path not required
  char *programName = "ls"; // Absolute path not required
  char *arg1 = "-1";

  int ret = execlp(programName, programName, NULL);

  printf("\nThe return value of exec is %d\n", ret);

  return 0;
}
