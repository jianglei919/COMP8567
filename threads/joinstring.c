#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void *func(void *p)
{
   char *retval = "\nHello! from the thread function\n";
   pthread_exit(retval); // void *ptr
}

main()
{
   pthread_t t1; // declare thread
   char *retval;
   pthread_create(&t1, NULL, func, NULL);
   pthread_join(t1, &retval); // void ** ptr
   printf("The return value from the thread is: \n %s", retval);
}
