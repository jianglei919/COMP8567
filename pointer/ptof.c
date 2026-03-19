#include <stdio.h>

// Declaring pointers to functions and using them

int add(int a, int b)
{
    return a + b;
}

int subtract(int a, int b)
{
    return a - b;
}

int main(void)
{
    int (*f1)(int, int); // Is a pointer to any function that returns an interger and takes two integers as parameters

    f1 = add;

    int retvalue = f1(10, 20);

    printf("\nThe return value is %d\n", retvalue);

    f1 = subtract;

    retvalue = f1(10, 20);

    printf("\nThe return value is %d\n", retvalue);
}
