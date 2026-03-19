#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int num1 = 0;

void *func(void *p)
{
   int a = 100;
   printf("The address of a is %d\n\n", &a);
   pthread_exit(NULL);
   return NULL;
}

main()
{
   pthread_t t1, t2, t3, t4; // declare thread

   pthread_create(&t1, NULL, func, NULL);
   pthread_create(&t2, NULL, func, NULL);
   pthread_create(&t3, NULL, func, NULL);
   pthread_create(&t4, NULL, func, NULL);
   // sleep(1);

   sleep(2);
   printf("\nFrom the main function\n");
}
