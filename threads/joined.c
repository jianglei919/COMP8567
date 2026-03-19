#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void *Func1(void *arg)
{
    while (1)
    {
        printf("From Function 1\n");
        sleep(1);
    }
    pthread_exit(NULL);
}

void *Func2(void *arg)
{
    while (1)
    {
        printf("From Function 2\n");
        sleep(1);
    }
    pthread_exit(NULL);
}

int main()
{
    pthread_attr_t att; // Define a variable of type pthread_attr_t
    pthread_t tid1, tid2;
    pthread_attr_init(&att);                                    //  Initialize the variable with default value /
    pthread_attr_setdetachstate(&att, PTHREAD_CREATE_DETACHED); // modify the attribute variable
    pthread_create(&tid1, &att, Func1, NULL);                   // Pass a pointer to this variable while creating the thread
    pthread_join(tid1, NULL);
    printf("The calling thread did not wait for the called thread despite the pthread_join()\n");

    pthread_attr_setdetachstate(&att, PTHREAD_CREATE_JOINABLE); // modify the attribute variable
    printf("\nA new thread is created in the joinable mode\n");
    pthread_create(&tid2, &att, Func2, NULL); // Pass a pointer to this variable while creating the thread
    pthread_join(tid2, NULL);

    pthread_attr_destroy(&att);
    return (0);
}
