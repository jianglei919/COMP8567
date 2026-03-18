#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

static void die(const char *msg)
{
    perror(msg);
    exit(1);
}

// A helper function to print the process info (name, PID, PPID)
static void say(const char *who)
{
    printf("This is from %s and this is my PID=%d and PPID=%d\n",
           who, (int)getpid(), (int)getppid());
    fflush(stdout);
}

int main()
{
    pid_t main_pid = getpid();

    // Use a pipe to pass GC1's PID to GC2 (so GC2 can wake GC1 to return to step 1)
    int p_gc1_to_gc2[2];
    if (pipe(p_gc1_to_gc2) < 0)
        die("pipe");

    // --------- main performs two consecutive forks to create C1 and C2 ----------
    pid_t f1 = fork();
    if (f1 < 0)
        die("fork1");

    pid_t f2 = fork();
    if (f2 < 0)
        die("fork2");

    // Handle the extra process (f1==0 and f2==0) by exiting immediately
    if (f1 == 0 && f2 == 0)
        _exit(0);

    // ---------------- MAIN ----------------
    if (f1 > 0 && f2 > 0)
    {
        pid_t c2_pid = f2;

        // Main immediately stops itself
        kill(getpid(), SIGSTOP);

        while (1)
        {
            // Print after being resumed by C1
            say("Main");
            sleep(2);

            // Resume C2
            kill(c2_pid, SIGCONT);

            // Stop again and wait for the next round
            kill(getpid(), SIGSTOP);
        }
    }

    // ---------------- C1 ----------------
    if (f1 == 0 && f2 > 0)
    {
        pid_t gc1_pid = fork();
        if (gc1_pid < 0)
            die("fork gc1");

        if (gc1_pid == 0)
        {
            // -------- GC1 --------
            close(p_gc1_to_gc2[0]); // write end only

            // Write GC1's PID to GC2 through the pipe
            pid_t me = getpid();
            if (write(p_gc1_to_gc2[1], &me, sizeof(me)) != sizeof(me))
                die("write");

            // GC1 sleeps for 5 seconds
            sleep(5);

            while (1)
            {
                say("GC1");
                sleep(2);

                // Resume C1
                kill(getppid(), SIGCONT);

                // Stop again
                kill(getpid(), SIGSTOP);
            }
        }
        else
        {
            // -------- C1 --------
            close(p_gc1_to_gc2[1]); // C1 does not write to the pipe
            // C1 immediately stops itself
            kill(getpid(), SIGSTOP);

            while (1)
            {
                // Resumed by GC1
                say("C1");
                sleep(2);

                // Resume Main
                kill(main_pid, SIGCONT);

                // Stop again and wait for the next round
                kill(getpid(), SIGSTOP);
            }
        }
    }

    // ---------------- C2 ----------------
    if (f1 > 0 && f2 == 0)
    {
        pid_t gc2_pid = fork();
        if (gc2_pid < 0)
            die("fork gc2");

        if (gc2_pid == 0)
        {
            // -------- GC2 --------
            close(p_gc1_to_gc2[1]); // read end only

            // GC2 immediately stops itself after creation
            kill(getpid(), SIGSTOP);

            // Read GC1's PID from the pipe (used to wake GC1 and return to step 1)
            pid_t gc1_pid = -1;
            if (read(p_gc1_to_gc2[0], &gc1_pid, sizeof(gc1_pid)) != sizeof(gc1_pid))
                die("read");

            while (1)
            {
                // Resumed by C2
                say("GC2");
                sleep(2);

                // Return to step 1: resume GC1
                kill(gc1_pid, SIGCONT);

                // Stop again
                kill(getpid(), SIGSTOP);
            }
        }
        else
        {
            // -------- C2 --------
            close(p_gc1_to_gc2[0]); // C2 does not read from the pipe

            // C2 immediately stops itself
            kill(getpid(), SIGSTOP);

            while (1)
            {
                // Resumed by Main
                say("C2");
                sleep(2);

                // Resume GC2
                kill(gc2_pid, SIGCONT);

                // Stop again
                kill(getpid(), SIGSTOP);
            }
        }
    }

    return 0;
}