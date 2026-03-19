#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

pthread_t tid[5];
int no_of_tickets = 2;

pthread_mutex_t lk1; // Mutex step1 - GLobal Declaration

void *bookticket(void *arg)
{

    sleep(1);
    pthread_mutex_lock(&lk1); // Mutex step3(within the thread) - Locking
    if (no_of_tickets >= 1)
    {

        no_of_tickets = no_of_tickets - 1;
        printf("Ticket booked to thread #: %d\n", pthread_self());
    }
    else
        printf("Sorry,all tickets are booked\n");

    pthread_mutex_unlock(&lk1); // Mutex step4(within the thread) -Unlocking

    return NULL;
}

int main(void)
{
    int i = 0;
    int err;

    if (pthread_mutex_init(&lk1, NULL) != 0) // Mutex step2 - Initialization
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    while (i < 5)
    {
        err = pthread_create(&(tid[i]), NULL, bookticket, NULL);
        if (err != 0)
            printf("\ncan't create thread\n");
        i++;
    }

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    pthread_join(tid[2], NULL);
    pthread_join(tid[3], NULL);
    pthread_join(tid[4], NULL);

    pthread_mutex_destroy(&lk1); // Mutex step5 - Destroy

    printf("The main thread resumes excution\n");
    return 0;
}
