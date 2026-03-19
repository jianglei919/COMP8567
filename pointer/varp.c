#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

double total(int, ...);

int main(int argc, char *argv[])
{
    printf("total of 10 and 20 is %f \n", total(2, 10, 20));
    printf("total of 10, 20 and 30 is %f\n",total(3, 10, 20, 30));
    printf("total of 10, 20, 30 and 40 is %f\n", total(4, 10, 20, 30, 40));
    printf("total of 10, 20, 30, 40 and 50 is %f\n",total(5, 10, 20, 30, 40, 50));
    exit(0);
}

double total(int n, ...)
{

    va_list argList;
    int sum = 0; // initialize sum to 0
    int i, v;
    printf("\nTHe number of arguments is %d\n", n);

    // create argList for the n arguments va_start(argList, n);
    va_start(argList, n);

    // extract all arguments from argList

    for (i = 0; i < n; i++)
    {
        v = va_arg(argList, int); // get next argument value
        printf("\n%d", v);
        sum += v;
    }
    printf("\n");
    va_end(argList); // clean up the argList before we return
    return (sum);
}
