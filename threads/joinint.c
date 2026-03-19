#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void *myThread()
{

   int *iptr;
   iptr = malloc(sizeof(int));
   *iptr = 2500;
   pthread_exit(iptr);
   // pthread_exit(void * ret)*/
   // return iptr;
}

int main()
{
   pthread_t tid;
   int *result;

   pthread_create(&tid, NULL, myThread, NULL);

   pthread_join(tid, (int *)&result);

   // pthread_join(tid,&result);

   // pthread_join(pthread_t tid, void ** rval_ptr)

   printf("%d\n", *result);

   return 0;
}
