#include <stdio.h>

int main(void)
{

	int a = 100;

	int *p = &a;

	//printf("\nThe address at p %d\n", p);
	//printf("\nThe address at p %p\n", p);
	printf("\nThe address at p %p\n", (void*)p);

	p = p + 1;

	//printf("\nThe address of p+1 is %d\n", p);
	//printf("\nThe address of p+1 is %p\n", p);
	printf("\nThe address of p+1 is %p\n", (void*)p);

	p = p - 1;

	//printf("\nThe address of p-1 is %d\n", p);
	//printf("\nThe address of p-1 is %p\n", p);
	printf("\nThe address of p-1 is %p\n", (void*)p);

	p = p + 2;
	//printf("\nThe address of p+2 is %d\n", p);
	//printf("\nThe address of p+2 is %p\n", p);
	printf("\nThe address of p+2 is %p\n", (void*)p);

	p = p - 2;

	//printf("\nThe address of p-2 is %d\n", p);
	//printf("\nThe address of p-2 is %p\n", p);
	printf("\nThe add ress of p-2 is %p\n", (void*)p);
}
