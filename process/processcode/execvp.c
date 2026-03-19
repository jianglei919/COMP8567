#include <unistd.h>

#include <unistd.h>
#include <stdio.h>

int main(void)
{

  printf("\nThe program will be replaced by 'ls -1(L) -t'  using execvp()\n");
  char *programName = "ls"; // full path of the executable not required
  char *args[] = {"ls", "-1", NULL};
  // Note: ls has two arguments in this example

  execvp(programName, args);

  printf("\nThe program successfully completed\n");
  return 0;
}
