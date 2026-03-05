#include <stdio.h>
#include <stdlib.h>

// define add function
int add(int num1, int num2){
    int sum = num1 + num2;
    printf("The Sum of %d + %d is: %d\n", num1, num2, sum);
    return sum;
}

// define multiply function
int multiply(int num1, int num2){
    int product = num1 * num2;
    printf("The Product of %d * %d is: %d\n", num1, num2, product);
    return product;
}

// define compute function that takes two function pointers as parameters, difference between the product and sum of num1 and num2.
int compute(int (*add)(int, int), int (*multiply)(int, int), int a, int b){
    int sum = add(a, b);
    int product = multiply(a, b);
    int result = product - sum;
    return result;
}

int main(void){
    // input two integers
    int a, b;
    printf("Enter two integers (a b):\n");
    if (scanf("%d %d", &a, &b) != 2) { // if the result of scanf is not 2 it means invalid input
        printf("Invalid input. Exit. \n");
        return 1;
    }

    //invoke compute method and print the result
    int result = compute(add, multiply, a, b);
    printf("The final result of product - sum is: %d\n", result);

    return 0;
}