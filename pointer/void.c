#include <stdio.h>

// void.c
int main()
{

    void *ptr1;

    int a = 100;
    ptr1 = &a; // contains the address of an integer
    printf("\n The value of ptr1 is %d\n", ptr1);

    float b = 20.32;
    ptr1 = &b; // contains the address of a float
    printf("\n The value of ptr1 is %d\n", ptr1);

    printf("%f",*ptr1); //generic pointers cannot be dereferenced

    float *temp;
    temp = ptr1;
    printf("The value after dereferencing temp %f\n", *temp);
}
