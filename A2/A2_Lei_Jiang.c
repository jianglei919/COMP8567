#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

static void die_perror(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

/* 用途：保存单个进程从 /proc 读取到的核心信息。 */
typedef struct Proc
{
    pid_t pid;
    pid_t ppid;
    char state;            /* R,S,T,Z... */
    long utime_ticks;      /* clock ticks */
    long stime_ticks;      /* clock ticks */
    long long start_ticks; /* starttime in clock ticks since boot */
    long long vmrss_bytes; /* VmRSS in bytes, -1 if unknown */
    char name[64];         /* process name, from status Name: */
    int is_bash;           /* 1 if bash, else 0 */
} Proc;

static void load_process_table()
{
    // todo
}

// lists the count of all descendants of process_id
static void opt_cnt(pid_t process_id)
{
    printf("cnt");
    // todo: implement this function
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s root_process process_id [Option]\n"
            "  %s -bcp\n"
            "  %s -bop\n"
            "Options:\n"
            "  -cnt -oct -dtm -odt -ndt -dnd -sst -sco\n"
            "  -kgp -kpp -ksp -kps -kgc -kcp -krp\n"
            "  -mmd -mpd\n",
            prog, prog, prog);
}

int main(int argc, char **argv)
{

    if (argc == 2 && strcmp(argv[1], "-bcp") == 0)
    {
        // ptree26w -bcp (No other arguments) count of the number of processes started under the current bash
        // Includes nested children, background processes, etc

        // todo: implement this function
        printf("bcp");
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "-bop") == 0)
    {
        // ptree26w -bop (No other arguments) count the overall number of processes in all open bash terminals(excluding the bash processes in the count)
        printf("bop");

        // todo: implement this function
        return 0;
    }

    if (argc < 3 || argc > 4)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // get root_process and validate it is a number
    char *end = NULL;
    long argv1 = strtol(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0')
    {
        fprintf(stderr, "Invalid root_process: %s, must be a valid integer.\n", argv[1]);
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (argv1 < 0 || argv1 > INT_MAX)
    {
        fprintf(stderr, "root_process value is out of range: %s\n", argv[1]);
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // get process_id and validate it is a number
    long argv2 = strtol(argv[2], &end, 10);
    if (end == argv[2] || *end != '\0')
    {
        fprintf(stderr, "Invalid process_id: %s, must be a valid integer.\n", argv[2]);
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (argv2 < 0 || argv2 > INT_MAX)
    {
        fprintf(stderr, "process_id value is out of range: %s\n", argv[2]);
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    pid_t root_process = (pid_t)argv1;
    pid_t process_id = (pid_t)argv2;

    if (argc == 3)
    {
        // no option, just print process_id and its ppid
        printf("%d %d\n", process_id, root_process);
        return 0;
    }

    // get opt from command line arguments
    load_process_table();

    char *opt = (argc == 4) ? argv[3] : NULL;
    if (!opt)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(opt, "-cnt") == 0)
        opt_cnt(process_id);
    // else if (strcmp(opt, "-oct") == 0)
    //     opt_oct(process_id, &pv, &cm);
    // else if (strcmp(opt, "-dtm") == 0)
    //     opt_dtm(process_id, &pv, &cm);
    // else if (strcmp(opt, "-odt") == 0)
    //     opt_odt(process_id, &pv, &cm);
    // else if (strcmp(opt, "-ndt") == 0)
    //     opt_ndt(process_id, &pv, &cm);
    // else if (strcmp(opt, "-dnd") == 0)
    //     opt_dnd(process_id, &cm);
    // else if (strcmp(opt, "-sst") == 0)
    //     opt_sst(process_id, &pv, &cm);
    // else if (strcmp(opt, "-sco") == 0)
    //     opt_sco(process_id, &pv, &cm);
    // else if (strcmp(opt, "-kgp") == 0)
    //     opt_kgp(process_id, &pv);
    // else if (strcmp(opt, "-kpp") == 0)
    //     opt_kpp(process_id, &pv);
    // else if (strcmp(opt, "-ksp") == 0)
    //     opt_ksp(process_id, &pv, &cm);
    // else if (strcmp(opt, "-kps") == 0)
    //     opt_kps(process_id, &pv, &cm);
    // else if (strcmp(opt, "-kgc") == 0)
    //     opt_kgc(process_id, &pv, &cm);
    // else if (strcmp(opt, "-kcp") == 0)
    //     opt_kcp(process_id, &pv, &cm);
    // else if (strcmp(opt, "-krp") == 0)
    //     opt_krp(root_process, &pv);
    // else if (strcmp(opt, "-mmd") == 0)
    //     opt_mmd(process_id, &pv, &cm);
    // else if (strcmp(opt, "-mpd") == 0)
    //     opt_mpd(process_id, &pv, &cm);
    else
    {
        printf("Unknown option: %s\n", opt);
        usage(argv[0]);
    }

    return 0;
}