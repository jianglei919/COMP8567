#include <stdio.h>

int main(void)
{

    int num = 100;

    int *b;
    b = &num;

    int **c;
    c = &b;

    int ***d;
    d = &c;

    printf("\nThe value of *b is: %d\n", *b);
    printf("\nThe value of **c is: %d\n", **c);
    printf("\nThe value of ***d is: %d\n", ***d);
}
