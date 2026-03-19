#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void build_path(char *buf, size_t bufsz, const char *sub) {
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "HOME is not set\n");
        exit(EXIT_FAILURE);
    }
    // e.g. ~/lab5/GC1
    snprintf(buf, bufsz, "%s/%s", home, sub);
}

int main(void) {
    pid_t pid1, pid2;

    // ---- 1st fork: create C1 ----
    pid1 = fork();
    if (pid1 < 0) die("fork pid1");

    // ---- 2nd fork: executed by BOTH parent and C1 ----
    pid2 = fork();
    if (pid2 < 0) die("fork pid2");

    // Identify roles by (pid1, pid2)
    // M  : pid1 > 0, pid2 > 0
    // C1 : pid1 == 0, pid2 > 0
    // C2 : pid1 > 0, pid2 == 0
    // GC1: pid1 == 0, pid2 == 0

    // ---------------- GC1 ----------------
    if (pid1 == 0 && pid2 == 0) {
        printf("GC1 will now differentiate\n");
        fflush(stdout);

        char gc1dir[512];
        build_path(gc1dir, sizeof(gc1dir), "lab5/GC1");

        if (chdir(gc1dir) != 0) die("chdir GC1");

        // "differentiate GC1 with ls -l ~/lab5/GC1 using execvp()"
        char *argv[] = {"ls", "-l", gc1dir, NULL};
        execvp("ls", argv);

        // if execvp returns, it failed
        die("execvp ls (GC1)");
    }

    // ---------------- C1 ----------------
    if (pid1 == 0 && pid2 > 0) {
        printf("C1 is waiting for GC1\n");
        fflush(stdout);

        int status_gc1;
        pid_t w = waitpid(pid2, &status_gc1, 0); // wait for GC1 specifically
        if (w < 0) die("waitpid GC1 (C1)");

        // C1 done
        exit(EXIT_SUCCESS);
    }

    // ---------------- C2 ----------------
    if (pid1 > 0 && pid2 == 0) {
        // In C2, invoke another fork() to create GC2
        pid_t gc2pid = fork();
        if (gc2pid < 0) die("fork GC2 (C2)");

        // -------- GC2 --------
        if (gc2pid == 0) {
            printf("GC2 will now differentiate to pwd\n");
            fflush(stdout);

            char gc2dir[512];
            build_path(gc2dir, sizeof(gc2dir), "lab5/GC2");

            if (chdir(gc2dir) != 0) die("chdir GC2");

            // "differentiate ... with pwd using execvp()"
            char *argv[] = {"pwd", NULL};
            execvp("pwd", argv);

            die("execvp pwd (GC2)");
        }

        // -------- C2 waits for GC2 --------
        printf("C2 waiting for GC2\n");
        fflush(stdout);

        int status_gc2;
        pid_t ret = waitpid(gc2pid, &status_gc2, 0);
        if (ret < 0) die("waitpid GC2 (C2)");

        // After C2 emerges from waiting, print return value and exit status macros
        printf("C2: waitpid() returned: %d\n", (int)ret);

        if (WIFEXITED(status_gc2)) {
            printf("C2: GC2 exited normally, exit code = %d\n", WEXITSTATUS(status_gc2));
        } else if (WIFSIGNALED(status_gc2)) {
            printf("C2: GC2 killed by signal %d\n", WTERMSIG(status_gc2));
        } else {
            printf("C2: GC2 ended with other status\n");
        }

        exit(EXIT_SUCCESS);
    }

    // ---------------- M (original parent) ----------------
    if (pid1 > 0 && pid2 > 0) {
        // To avoid zombies, wait for C1 and C2 (order doesn't matter)
        int st;
        while (wait(&st) > 0) {
            // keep waiting until no more children
        }
        // wait() returns -1 with errno=ECHILD when no children remain
        return 0;
    }

    // Should never reach here
    return 0;
}
