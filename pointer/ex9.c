// ex9.c

#include <stdio.h>

int main()
{
    int ar[4] = {5, 10, 15, 20};

    printf("The value of ar is %d \n", ar);
    printf("The value of &ar[0] is %d \n", &ar[0]);

    int *ptr;

    int i;

    for (i = 0; i < 4; i++)
        printf("%d\n", ar[i]);

    printf("\n");

    ptr = ar; /* Equivalent to ptr=&ar[0] */

    for (i = 0; i < 4; i++)
        printf("%d\n", ptr[i]);
}
