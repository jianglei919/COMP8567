#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void *func(void *p)
{

   printf("\nFrom the thread function\n");
   char *msg;
   msg = p;
   printf("\nMessage sent from the main thread: %s\n", msg);
   sleep(1);

   return NULL;
}

main()
{
   pthread_t t1; // declare thread
   char *msg1 = "\nWelcome to COMP8567\n";
   pthread_create(&t1, NULL, func, msg1);
   sleep(2);
}
