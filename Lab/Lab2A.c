#include <stdio.h>
#include <stdlib.h>

int main(void){
    int n;
    printf("Enter the number of elements (n):\n");
    if (scanf("%d", &n) != 1 || n <= 0) { // if the result of scanf is not 1 or n <= 0 it measns invalid input. For example, input a charracter or negative number or ctrl+d
        printf("Invalid n.\n");
        return 1;  
    }

    // Allocate n integers on the heap
    int *arr = (int *)malloc((size_t)n * sizeof(int));
    if (!arr) {
        perror("malloc failed ! Exit. \n");
        return 1;  
    }

    for (int i = 0; i < n; i++) {
        printf("\nPlease enter %dth element:\n", i+1);
        int input_element = scanf("%d", arr + i);
        if (input_element != 1) { // if the result of scanf is not 1 it means invalid input
            printf("Invalid input. Exit. \n");
            free(arr);
            return 1;  
        }
    }

    //print the original arrray elements without using arrray indexing
    printf("\nPrint arrray elements in original order:\n");
    for (int i = 0; i < n; i++) {
        printf("%d ", *(arr + i));
    }
    printf("\n");

    //printf the reversed arrray elements without using arrray indexing
    printf("\nPrint arrray elements in reverse order:\n");
    for (int i = n - 1; i >= 0; i--) {
        printf("%d ", *(arr + i));
    }
    printf("\n");


    free(arr);
}