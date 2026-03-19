#include <stdio.h>
#include <stdlib.h>
// f44.c

main()
{

    int a;
    a = fork();
    a = fork();

    printf("\nProcess id = %d, Parent id= %d\n", getpid(), getppid());
}
