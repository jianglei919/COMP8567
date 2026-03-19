#include <stdio.h>

// fap9.c

// int (*m1)(int,int);
// int (*m2)(int);

int maximum(int a, int b)
{

    if (a > b)
        return a;
    else
        return b;
}

int fact(int c)
{
    int ret = 1;
    while (c >= 1)
    {
        ret = ret * (c--);
    }
    return ret;
}

int compute(int (*m1)(int, int), int (*m2)(int), int x1, int x2)
{
    int ret1, ret2;
    ret1 = m1(x1, x2);
    ret2 = m2(ret1); // factorial of ret1
    return ret2;
}

int main(void)
{
    int num1, num2;

    printf("\nEnter the value of num1\n");
    scanf("%d", &num1);

    printf("\nEnter the value of num2\n");
    scanf("%d", &num2);

    int ret3 = compute(maximum, fact, num1, num2); // Calcualtes the factorial of the max (of num1 and num2)

    printf("\nThe factorial of max(%d,%d) is %d\n", num1, num2, ret3);
} // end main