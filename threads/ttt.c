#include <pthread.h>
#include <stdio.h>

void *myThreadFunc1()
{
    for (;;)
    {
        printf("First thread\n");
        sleep(1);
    }
    printf("Returning from the first thread\n");
    return NULL;
}

void *myThreadFunc3()
{
    for (;;)
    {
        printf("Third thread\n");
        sleep(1);
    }
    printf("Returning from the third thread\n");
    return NULL;
}

void *myThreadFunc2()
{
    pthread_t threadId3;
    pthread_create(&threadId3, NULL, myThreadFunc3, NULL);
    // pthread_join(threadId3,NULL);
    for (;;)
    {
        printf("Second thread\n");
        sleep(1);
    }
    printf("Returning from the second thread\n");
    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t threadId1, threadId2;
    pthread_create(&threadId1, NULL, myThreadFunc1, NULL);
    pthread_create(&threadId2, NULL, myThreadFunc2, NULL);
    // pthread_join(threadId1,NULL);
    // pthread_join(threadId2,NULL);
    printf("This is the main thread\n");

    return (0);
} // compile with -lpthread library link
