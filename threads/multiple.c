#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int num1 = 0;

void *func(void *p)
{

   printf("Message from the thread with thread id=%d\n", pthread_self());
   sleep(1);
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
   sleep(2);
   printf("\nFrom the main function with thread id %d\n", pthread_self());
}
