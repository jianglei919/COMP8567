#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void build_path(char *buf, size_t bufsz, const char *sub) {
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "HOME is not set\n");
        exit(EXIT_FAILURE);
    }
    // e.g. ~/lab5/GC1
    snprintf(buf, bufsz, "%s/%s", home, sub);
}

int main(void)
{
    // Process IDs for children and grandchildren.
    pid_t pidc1, pidgc1;
    pidc1 = fork();
    if (pidc1 < 0)
    {
        // fork failed
        perror("fork");
        exit(1);
    }
    else if (pidc1 == 0)
    {
        // Child 1: spawns GC1 and waits for it.
        pidgc1 = fork();
        if (pidgc1 < 0)
        {
            // fork failed
            perror("fork");
            _exit(1);
        }
        else if (pidgc1 == 0)
        {
            // Grandchild 1: switch directory, then exec `ls -1`.
            printf("GC1 will now differentiate to ls -1.\n");
            fflush(stdout);

            // Build the path to GC1's working directory, e.g. ~/lab5/GC1
            char gc1dir[512];
            build_path(gc1dir, sizeof(gc1dir), "lab5/GC1");

            // change pwd to ~/lab5/GC1
            int ret = chdir(gc1dir);
            if (ret != 0)
            {
                // failed when change dir
                perror("chdir");
                _exit(1);
            }
            // Replace process image; only returns on error.
            char *args[] = {"ls", "-1", gc1dir, NULL};
            execvp(args[0], args);
            perror("execvp");
            _exit(1);
        }
        else if (pidgc1 > 0)
        {
            // GC1 completion is synchronized here.
            int status;
            waitpid(pidgc1, &status, 0);
            printf("C1 is waiting for GC1.\n");
            fflush(stdout);
        }
        _exit(0);
    }
    else if (pidc1 > 0)
    {
        // this is pidc1's parent process
    }

    // Process IDs for children and grandchildren.
    pid_t pidc2, pidgc2;
    pidc2 = fork();
    if (pidc2 < 0)
    {
        // fork failed
        perror("fork");
        exit(1);
    }
    else if (pidc2 == 0)
    {
        // Child 2: spawns GC2 and waits for it.
        pidgc2 = fork();
        if (pidgc2 < 0)
        {
            // fork failed
            perror("fork");
            _exit(1);
        }
        else if (pidgc2 == 0)
        {
            // Grandchild 2: switch directory, then exec `pwd` command.
            printf("GC2 will now differentiate to pwd.\n");
            fflush(stdout);

            // change pwd to ~/lab5/GC2
            char gc2dir[512];
            build_path(gc2dir, sizeof(gc2dir), "lab5/GC2");
            int ret = chdir(gc2dir);
            if (ret != 0)
            {
                // failed when change dir
                perror("chdir");
                _exit(1);
            }
            // Replace process image; only returns on error.
            char *args[] = {"pwd", NULL};
            execvp(args[0], args);
            perror("execvp");
            _exit(1);
        }
        else if (pidgc2 > 0)
        {
            // GC2 completion and status are synchronized here.
            int status;
            int ret = waitpid(pidgc2, &status, 0);
            printf("C2 waiting for GC2.\n");
            fflush(stdout);

            // Report GC2 exit status for observability.
            if (WIFEXITED(status))
            {
                printf("gc2 exited normally with status %d, waitpid is %d\n", WEXITSTATUS(status), ret);
            }
            else if (WIFSIGNALED(status))
            {
                printf("gc2 was terminated by signal %d,waitpid is %d\n", WTERMSIG(status), ret);
            }
        }
        _exit(0);
    }
    else if (pidc2 > 0)
    {
        // this is pidc2's parent process
    }

    // Parent waits for both children to avoid zombies.
    if (pidc1 > 0 && pidc2 > 0) {
        // To avoid zombies, wait for C1 and C2 (order doesn't matter)
        int st;
        while (wait(&st) > 0) {
            // keep waiting until no more children
        }
        // wait() returns -1 with errno=ECHILD when no children remain
        return 0;
    }
    return 0;
}