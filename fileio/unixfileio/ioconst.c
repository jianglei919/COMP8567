// fmax.c

#include <stdio.h>
#include <unistd.h> //Provides access to POSIX OS API

int main(void)
{
    printf("\n%d", STDIN_FILENO);
    printf("\n%d", STDOUT_FILENO);
    printf("\n%d\n", STDERR_FILENO);
}
