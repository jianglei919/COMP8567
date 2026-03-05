#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

// Global variable to track if CTR-C has been pressed once, using sig_atomic_t for safe access in signal handlers
volatile sig_atomic_t is_first_enter = 0;  // Flag to track first CTR-C press. 0: false, 1: true

// Function to safely print messages in signal handlers
void safe_print(const char *msg) {
    write(STDOUT_FILENO, msg, strlen(msg));
}

// SIGINT handler
void sigint_handler(int sig) {
    if (is_first_enter) { // Second time pressing CTR-C
        safe_print("\nYou pressed CTR-C twice! Exiting now.\n");
        alarm(0); // Cancel the last alarm
        _exit(0);
    } else { // First time pressing CTR-C
        is_first_enter = 1; // Set the flag to indicate the first CTR-C has been pressed
        safe_print("\n\nYou have already pressed CTR-C once.\n\n");
        alarm(5);  // 5 seconds to reset the counter
    }
}

// SIGALRM handler
void sigalrm_handler(int sig) {
    safe_print("\nYou haven't entered CTR-C twice within a 5 seconds yet! CTR-C counter will be reset.\n\n");
    is_first_enter = 0;
}

int main() {
    // Register SIGINT and SIGALRM handlers
    signal(SIGINT, sigint_handler);
    signal(SIGALRM, sigalrm_handler);

    // forever loop to keep the program running, waiting for signals, every second print two messages
    for(;;) {
        printf("Entering CTR-C only once will not work.\n");
        printf("You need to enter it twice within a 5-second interval.\n");
        sleep(1);
    }

    return 0;
}