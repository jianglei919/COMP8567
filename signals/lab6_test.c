#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

volatile sig_atomic_t first_sigint = 0;  // Flag to track first Ctrl-C press

void safe_print(const char *msg) {
    write(STDOUT_FILENO, msg, strlen(msg));
}

// SIGINT handler
void sigint_handler(int sig) {
    if (first_sigint) {
        safe_print("\nDetected second Ctrl-C within 4 seconds. Exiting...\n");
        alarm(0); // Cancel the alarm
        _exit(0);
    } else {
        safe_print("\nPressing Ctrl-C once will not work. Press again within 4 seconds.\n");
        first_sigint = 1;
        alarm(4);  // 4 seconds to reset the counter
    }
}

// SIGALRM handler
void sigalrm_handler(int sig) {
    safe_print("\nToo slow! Resetting Ctrl-C counter.\n");
    first_sigint = 0;
}

int main() {
    // set up SIGINT and SIGALRM handlers
    signal(SIGINT, sigint_handler);
    signal(SIGALRM, sigalrm_handler);

    // forever loop to keep the program running, waiting for signals, every second print a message
    printf("Pressing Ctrl-C once will not work.\n");
    printf("Press Ctrl-C twice within a 4 second interval.\n");
    for(;;) {
        sleep(1);
    }

    return 0;
}