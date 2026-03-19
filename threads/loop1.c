#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void *myThreadFunc1()
{
    for (;;)
    {
        printf("First thread\n");
    }
    return NULL;
}

void *myThreadFunc2()
{
    for (;;)
    {
        printf("Second thread\n");
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t threadId1, threadId2;
    // Create a new thread to run myThreadFunc()
    pthread_create(&threadId1, NULL, myThreadFunc1, NULL);
    pthread_create(&threadId2, NULL, myThreadFunc2, NULL);

    pthread_join(threadId1, NULL); // Main waits until threadId1 completes execution
    // pthread_join(threadId2,NULL);//Main waits until threadId1 completes execution

    printf("This is the main thread\n");

    return (0);
} // compile with -lpthread library link
