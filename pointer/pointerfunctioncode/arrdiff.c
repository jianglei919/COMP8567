#include <stdio.h>

int main(void)
{
    int ar[100];
    int *p1, *p2;

    p1 = ar;
    p2 = &ar[60];

    printf("\np2-p1(offset): %d\n", p2 - p1);

    p1 = &ar[30];
    p2 = &ar[80];

    printf("\np2-p1(offset): %d\n", p2 - p1);

    // printf("\np2-p1(offset): %d\n", p2 + p1);
}
