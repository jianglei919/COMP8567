#include <unistd.h>
#include <stdio.h>

int main(void)
{

  printf("\nThis program will be replaced by ls -1 using execl()\n");

  char *programName = "/bin/ls";
  char *arg1 = "-1";
  char *arg2 = "/home/pranga/chapter3";

  execl(programName, programName, arg1, NULL);

  // execl(programName, programName,arg1,arg2,NULL);

  // execl("/bin/ls","/bin/ls","-1",NULL);  //This is acceptable too
  // execl("/bin/ls","/bin/ls","-1","/home/pranga/chapter3",NULL);  //This is acceptable too

  printf("\nThe program successfully completed\n");

  return 0;
}
