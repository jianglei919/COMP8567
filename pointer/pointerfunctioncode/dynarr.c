#include <stdio.h>
#include <stdlib.h>

int main(void)
{

	int n;
	printf("\nEnter the number of elements in the array\n");
	scanf("%d", &n);
	int *ar;
	ar = malloc(n * sizeof(int));

	for (int i = 0; i < n; i++)
	{
		printf("Enter element %d\n", i);
		scanf("%d", &ar[i]);
		// scanf("%d",&*(ar+i));
	}

	printf("\nThe elements of the array are\n");
	for (int i = 0; i < n; i++)
	{
		printf("\n%d", ar[i]);
	}

	free(ar);
	// End dynarr.c
}
