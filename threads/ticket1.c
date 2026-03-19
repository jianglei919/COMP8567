#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

pthread_t tid[5];
int no_of_tickets = 2;

// Without mutex
void *bookticket(void *arg)
{
    sleep(1);
    // start of the crtical section  LOCKED
    if (no_of_tickets >= 1)
    {
        no_of_tickets = no_of_tickets - 1;
        printf("Ticket booked to thread #: %d\n", pthread_self());
    }
    else
        printf("Sorry,all tickets are booked\n");
    // end of critical section   UNLOCKED
    return NULL;
}

int main(void)
{
    int i = 0;
    int err;

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
    printf("The main thread resumes execution\n");

    printf("The value of the global variable no_of_tickets is %d\n", no_of_tickets);

    return 0;
}
