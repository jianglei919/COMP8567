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

static int read_ppid_from_proc(pid_t pid, pid_t *ppid_out)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);

    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;

    char line[256];
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "PPid:", 5) == 0)
        {
            char *p = line + 5;
            while (*p && isspace((unsigned char)*p))
                p++;

            long ppid_val = strtol(p, NULL, 10);
            fclose(fp);

            if (ppid_val < 0 || ppid_val > INT_MAX)
                return -1;

            *ppid_out = (pid_t)ppid_val;
            return 0;
        }
    }

    fclose(fp);
    return -1;
}

static int is_bash_process(pid_t pid)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/comm", (int)pid);

    FILE *fp = fopen(path, "r");
    if (!fp)
        return 0;

    char name[64] = {0};
    if (!fgets(name, sizeof(name), fp))
    {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    name[strcspn(name, "\n")] = '\0';
    return strcmp(name, "bash") == 0;
}

static int pid_in_list(pid_t pid, const pid_t *list, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (list[i] == pid)
            return 1;
    }
    return 0;
}

static pid_t find_current_bash_ancestor(void)
{
    pid_t current = getpid();
    while (current > 1)
    {
        if (is_bash_process(current))
            return current;

        pid_t parent = -1;
        if (read_ppid_from_proc(current, &parent) != 0)
            break;
        if (parent <= 1 || parent == current)
            break;

        current = parent;
    }

    return -1;
}

/**
 * 向上查父链，判断是否属于 bash 子树
 *
 * 先遍历系统里每个进程 pid
 * 对每个 pid 向上追 PPid 链
 * 只要在追溯过程中遇到 bash_pid，这个 pid 就属于 bash 子树，计数 +1
 */
static void opt_bcp()
{
    pid_t bash_pid = find_current_bash_ancestor();
    if (bash_pid <= 0)
    {
        fprintf(stderr, "Cannot locate current bash ancestor process.\n");
        exit(EXIT_FAILURE);
    }

    long count = 0;

    DIR *dir = opendir("/proc");
    if (!dir)
        die_perror("opendir /proc");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (!isdigit((unsigned char)entry->d_name[0]))
            continue;

        char *end = NULL;
        long value = strtol(entry->d_name, &end, 10);
        if (!end || *end != '\0' || value <= 0 || value > INT_MAX)
            continue;

        pid_t pid = (pid_t)value;
        if (pid == bash_pid)
            continue;

        pid_t current = pid;
        while (current > 0)
        {
            pid_t parent = -1;
            if (read_ppid_from_proc(current, &parent) != 0)
                break;

            if (parent == bash_pid)
            {
                count++;
                break;
            }

            if (parent <= 1 || parent == current)
                break;

            current = parent;
        }
    }

    closedir(dir);
    printf("%ld\n", count);
}

/**
 * 统计所有打开的 bash 终端（不包括 bash 进程本身）中进程的总数
 *
 * 第一次遍历 proc：收集所有 bash 的 PID
 * 第二次遍历 proc：对每个非 bash 进程向上追父链，只要遇到一个 bash_pid 就计数 +1，并且这个进程不再继续追父链了
 *
 */
static void opt_bop()
{
    DIR *dir = opendir("/proc");
    if (!dir)
        die_perror("opendir /proc");

    pid_t *bash_pids = NULL;
    size_t bash_count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (!isdigit((unsigned char)entry->d_name[0]))
            continue;

        char *end = NULL;
        long value = strtol(entry->d_name, &end, 10);
        if (!end || *end != '\0' || value <= 0 || value > INT_MAX)
            continue;

        pid_t pid = (pid_t)value;
        if (!is_bash_process(pid))
            continue;

        printf("Found bash process: %d\n", (int)pid);
        pid_t *new_list = realloc(bash_pids, (bash_count + 1) * sizeof(*bash_pids));
        if (!new_list)
        {
            free(bash_pids);
            closedir(dir);
            die_perror("realloc");
        }

        bash_pids = new_list;
        bash_pids[bash_count++] = pid;
    }

    rewinddir(dir); // 重新遍历 proc 统计所有 bash 终端中的进程数

    long count = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        if (!isdigit((unsigned char)entry->d_name[0]))
            continue;

        char *end = NULL;
        long value = strtol(entry->d_name, &end, 10);
        if (!end || *end != '\0' || value <= 0 || value > INT_MAX)
            continue;

        pid_t pid = (pid_t)value;
        if (pid_in_list(pid, bash_pids, bash_count))
            continue;

        pid_t current = pid;
        while (current > 0)
        {
            pid_t parent = -1;
            if (read_ppid_from_proc(current, &parent) != 0)
                break;

            if (pid_in_list(parent, bash_pids, bash_count))
            {
                count++;
                break;
            }

            if (parent <= 1 || parent == current)
                break;

            current = parent;
        }
    }

    closedir(dir);
    free(bash_pids);
    printf("%ld\n", count);
}

static int opt_default(pid_t process_id, pid_t root_process)
{
    pid_t current = process_id;

    while (current > 0)
    {
        if (current == root_process)
        {
            printf("%d %d\n", (int)process_id, (int)root_process);
            return 0;
        }

        pid_t parent = -1;
        if (read_ppid_from_proc(current, &parent) != 0)
            break;

        if (parent <= 0 || parent == current)
            break;

        current = parent;
    }

    printf("Process %d does not belong to the process subtree rooted at %d\n",
           (int)process_id, (int)root_process);
    return -1;
}

/**
 * -cnt：统计 process_id 的后代总数。
 * 实现思路：遍历 /proc 中的每个 pid，对每个 pid 向上追父链，如果在追溯过程中遇到 process_id 就计数 +1
 */
static void opt_cnt(pid_t process_id)
{
    DIR *dir = opendir("/proc");
    if (!dir)
        die_perror("opendir /proc");

    long count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (!isdigit((unsigned char)entry->d_name[0]))
            continue;

        char *end = NULL;
        long value = strtol(entry->d_name, &end, 10);
        if (!end || *end != '\0' || value <= 0 || value > INT_MAX)
            continue;

        pid_t pid = (pid_t)value;
        if (pid == process_id)
            continue;

        pid_t current = pid;
        while (current > 0)
        {
            pid_t parent = -1;
            if (read_ppid_from_proc(current, &parent) != 0)
                break;

            if (parent == process_id)
            {
                count++;
                break;
            }

            if (parent <= 1 || parent == current)
                break;

            current = parent;
        }
    }

    closedir(dir);
    printf("%ld\n", count);
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
        opt_bcp();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "-bop") == 0)
    {
        // count the overall number of processes in all open bash terminals(excluding the bash processes in the count)
        opt_bop();
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

    // no option, do default process
    if (argc == 3)
    {
        if (opt_default(process_id, root_process) != 0)
            return EXIT_FAILURE;
        return 0;
    }
    if (opt_default(process_id, root_process) != 0)
        return EXIT_FAILURE;

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