#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void *func1(void *p)
{

   printf("Hello from the thread function (func1)\n");
   pthread_exit(NULL);
}

main()
{
   pthread_t t1, t2; // declare thread
   int ret = pthread_create(&t1, NULL, func1, NULL);
   // sleep(2);

   printf("From the main function\n");
   printf("The value of ret is %d\n", ret);
   printf("The ID of the thread is %d\n", t1);
}
