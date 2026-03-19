#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void *Func(void *arg)
{
    while (1)
    {
        printf("From the thread\n");
        // sleep(1);
    }
    pthread_exit(NULL);
}

int main()
{
    pthread_attr_t att; // Define a variable of type pthread_attr_t
    pthread_t tid;
    pthread_attr_init(&att);                                    //  Initialize the variable with default value /
    pthread_attr_setdetachstate(&att, PTHREAD_CREATE_DETACHED); // modify the attribute variable
    pthread_create(&tid, NULL, Func, NULL);                     // Pass a pointer to this variable while creating the thread
    pthread_join(tid, NULL);
    printf("The calling thread does not wait for the called thread despite the pthread_join()\n");

    pthread_attr_destroy(&att);

    return (0);
}
