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

typedef struct ProcList
{
    Proc *proc_items;
    size_t count;
} ProcList;

/**
 * 用途：读取 /proc/<pid>/stat 中的 PPid 字段，保存在 ppid_out 中。
 */
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

/**
 * 用途：读取 /proc/<pid>/stat 和 /proc/<pid>/status 中的信息，填充到 proc_out 中。
 */
static int read_proc_info(pid_t pid, Proc *proc_out)
{
    if (!proc_out)
        return -1;

    memset(proc_out, 0, sizeof(*proc_out));
    proc_out->pid = pid;
    proc_out->ppid = -1;
    proc_out->state = '\0';
    proc_out->start_ticks = -1;
    proc_out->vmrss_bytes = -1;

    char stat_path[64];
    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", (int)pid);

    FILE *fp = fopen(stat_path, "r");
    if (!fp)
        return -1;

    char line[4096];
    if (!fgets(line, sizeof(line), fp))
    {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    char *right_paren = strrchr(line, ')');
    if (!right_paren || *(right_paren + 1) != ' ')
        return -1;

    char *cursor = right_paren + 2;
    int field_no = 3;
    char *saveptr = NULL;
    char *token = strtok_r(cursor, " ", &saveptr);

    while (token)
    {
        if (field_no == 3)
            proc_out->state = token[0];
        else if (field_no == 4)
            proc_out->ppid = (pid_t)strtol(token, NULL, 10);
        else if (field_no == 14)
            proc_out->utime_ticks = strtol(token, NULL, 10);
        else if (field_no == 15)
            proc_out->stime_ticks = strtol(token, NULL, 10);
        else if (field_no == 22)
            proc_out->start_ticks = strtoll(token, NULL, 10);

        field_no++;
        token = strtok_r(NULL, " ", &saveptr);
    }

    char status_path[64];
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", (int)pid);
    fp = fopen(status_path, "r");
    if (!fp)
        return 0;

    char status_line[256];
    while (fgets(status_line, sizeof(status_line), fp))
    {
        if (strncmp(status_line, "Name:", 5) == 0)
        {
            char *p = status_line + 5;
            while (*p && isspace((unsigned char)*p))
                p++;
            strncpy(proc_out->name, p, sizeof(proc_out->name) - 1);
            proc_out->name[sizeof(proc_out->name) - 1] = '\0';
            proc_out->name[strcspn(proc_out->name, "\n")] = '\0';
        }
        else if (strncmp(status_line, "VmRSS:", 6) == 0)
        {
            char *p = status_line + 6;
            while (*p && isspace((unsigned char)*p))
                p++;
            long long vmrss_kb = strtoll(p, NULL, 10);
            if (vmrss_kb >= 0)
                proc_out->vmrss_bytes = vmrss_kb * 1024;
        }
    }
    fclose(fp);

    proc_out->is_bash = (strcmp(proc_out->name, "bash") == 0);
    return 0;
}

/**
 * 用途：把一个 pid 追加到一个动态数组（pid_t 列表）末尾，并更新数组指针和元素个数。
 */
static int append_pid(pid_t **list, size_t *count, pid_t pid)
{
    pid_t *new_list = realloc(*list, (*count + 1) * sizeof(**list));
    if (!new_list)
        return -1;

    *list = new_list;
    (*list)[*count] = pid;
    (*count)++;
    return 0;
}

static int append_proc(ProcList *list, const Proc *proc)
{
    Proc *new_list = realloc(list->proc_items, (list->count + 1) * sizeof(*list->proc_items));
    if (!new_list)
        return -1;

    list->proc_items = new_list;
    list->proc_items[list->count] = *proc;
    list->count++;
    return 0;
}

static void free_proc_list(ProcList *list)
{
    if (!list)
        return;

    free(list->proc_items);
    list->proc_items = NULL;
    list->count = 0;
}

static int proc_pid_in_list(pid_t pid, const Proc *list, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (list[i].pid == pid)
            return 1;
    }
    return 0;
}

/**
 * 用途：从 /proc/stat 中读取系统启动时间（btime 字段），保存在 boot_time_out 中，单位是 epoch 秒。
 */
static int read_boot_time_epoch(time_t *boot_time_out)
{
    if (!boot_time_out)
        return -1;

    FILE *fp = fopen("/proc/stat", "r");
    if (!fp)
        return -1;

    char line[256];
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "btime ", 6) == 0)
        {
            char *p = line + 6;
            long long val = strtoll(p, NULL, 10);
            fclose(fp);
            if (val <= 0)
                return -1;
            *boot_time_out = (time_t)val;
            return 0;
        }
    }

    fclose(fp);
    return -1;
}

/**
 * 从 /proc 中收集 root_pid 的所有后代进程信息，保存在 descendants_out 中，并将后代数量保存在 descendants_count_out 中。
 * 实现思路：
 * 遍历 /proc 中的每个 pid，对每个 pid 向上追父链，如果在追溯过程中遇到 root_pid 就把这个 pid 的信息加入 descendants_out 中，并且这个 pid 不再继续追父链了
 */
static int collect_descendants(pid_t root_pid, ProcList *descendants_out)
{
    if (!descendants_out)
        return -1;

    DIR *dir = opendir("/proc");
    if (!dir)
        return -1;

    ProcList descendants = {0};

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
        if (pid == root_pid)
            continue;

        pid_t current = pid;
        while (current > 0)
        {
            pid_t parent = -1;
            if (read_ppid_from_proc(current, &parent) != 0)
                break;

            if (parent == root_pid)
            {
                Proc proc;
                if (read_proc_info(pid, &proc) != 0)
                    break;

                if (append_proc(&descendants, &proc) != 0)
                {
                    free_proc_list(&descendants);
                    closedir(dir);
                    return -1;
                }
                break;
            }

            if (parent <= 1 || parent == current)
                break;

            current = parent;
        }
    }

    closedir(dir);
    *descendants_out = descendants;
    return 0;
}

/**
 * 用途：从 /proc 中收集 process_id 的所有兄弟进程信息，保存在 siblings_out 中，并将兄弟数量保存在 siblings_count_out 中。
 */
static int collect_siblings(pid_t process_id, ProcList *siblings_out)
{
    if (!siblings_out)
        return -1;

    pid_t parent_pid = -1;
    if (read_ppid_from_proc(process_id, &parent_pid) != 0 || parent_pid <= 0)
        return -1;

    DIR *dir = opendir("/proc");
    if (!dir)
        return -1;

    ProcList siblings = {0};

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

        Proc proc;
        if (read_proc_info(pid, &proc) != 0)
            continue;

        if (proc.ppid != parent_pid)
            continue;

        if (append_proc(&siblings, &proc) != 0)
        {
            free_proc_list(&siblings);
            closedir(dir);
            return -1;
        }
    }

    closedir(dir);
    *siblings_out = siblings;
    return 0;
}

static int cmp_proc_start_desc(const void *a, const void *b)
{
    const Proc *ea = (const Proc *)a;
    const Proc *eb = (const Proc *)b;

    if (ea->start_ticks < eb->start_ticks)
        return 1;
    if (ea->start_ticks > eb->start_ticks)
        return -1;
    if (ea->pid < eb->pid)
        return 1;
    if (ea->pid > eb->pid)
        return -1;
    return 0;
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

    ProcList descendants = {0};
    if (collect_descendants(bash_pid, &descendants) != 0)
        die_perror("collect_descendants");

    printf("%zu\n", descendants.count);
    free_proc_list(&descendants);
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

    closedir(dir);

    pid_t *all_pids = NULL;
    size_t all_count = 0;

    for (size_t i = 0; i < bash_count; i++)
    {
        ProcList descendants = {0};
        if (collect_descendants(bash_pids[i], &descendants) != 0)
        {
            free_proc_list(&descendants);
            free(all_pids);
            free(bash_pids);
            die_perror("collect_descendants");
        }

        for (size_t j = 0; j < descendants.count; j++)
        {
            pid_t pid = descendants.proc_items[j].pid;
            if (pid_in_list(pid, all_pids, all_count)) // 去重
                continue;

            if (append_pid(&all_pids, &all_count, pid) != 0)
            {
                free_proc_list(&descendants);
                free(all_pids);
                free(bash_pids);
                die_perror("realloc");
            }
        }

        free_proc_list(&descendants);
    }

    printf("%zu\n", all_count);
    free(all_pids);
    free(bash_pids);
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
    ProcList descendants = {0};
    if (collect_descendants(process_id, &descendants) != 0)
        die_perror("collect_descendants");

    printf("%zu\n", descendants.count);
    free_proc_list(&descendants);
}

/**
 * -oct：统计 process_id 的后代中已经成为孤儿的数量（即父进程已经不在 process_id 的子树中，或者父进程是 init 进程）。
 * 实现思路：
 * 第一次遍历 /proc 中的每个 pid，对每个 pid 向上追父链，如果在追溯过程中遇到 process_id 就把这个 pid 加入到一个列表中
 * 第二次遍历这个列表，对每个 pid 向上追父链，如果在追溯过程中没有遇到 process_id 就说明这个 pid 已经成为孤儿了，计数 +1
 */
static void opt_oct(pid_t process_id)
{
    ProcList subtree_procs = {0};
    if (collect_descendants(process_id, &subtree_procs) != 0)
        die_perror("collect_descendants");

    long orphan_count = 0;
    for (size_t i = 0; i < subtree_procs.count; i++)
    {
        pid_t parent = subtree_procs.proc_items[i].ppid;
        if (parent <= 0)
            continue;

        if (parent == process_id)
            continue;

        if (parent == 1 || !proc_pid_in_list(parent, subtree_procs.proc_items, subtree_procs.count))
            orphan_count++;
    }

    free_proc_list(&subtree_procs);
    printf("%ld\n", orphan_count);
}

/**
 * -dtm：向 process_id 的所有后代发送 SIGKILL 信号，要求它们终止。要求按照 starttime 从晚到早的顺序发送信号（即先杀死 starttime 晚的进程），以尽量减少对系统的影响。
 * 实现思路：
 * 第一次遍历 /proc 中的每个 pid，对每个 pid 向上追父链，如果在追溯过程中遇到 process_id 就把这个 pid 的信息加入 descendants_out 中，并且这个 pid 不再继续追父链了
 * 第二次对 descendants_out 进行排序，按照 starttime 从晚到早的顺序排序
 * 第三次遍历排序后的 descendants_out，依次发送 SIGKILL 信号给每个 pid
 */
static void opt_dtm(pid_t process_id)
{
    ProcList descendants = {0};
    if (collect_descendants(process_id, &descendants) != 0)
        die_perror("collect_descendants");

    if (descendants.count == 0)
    {
        free_proc_list(&descendants);
        return;
    }

    qsort(descendants.proc_items, descendants.count, sizeof(*descendants.proc_items), cmp_proc_start_desc);

    for (size_t i = 0; i < descendants.count; i++)
    {
        pid_t pid = descendants.proc_items[i].pid;
        if (descendants.proc_items[i].is_bash)
        {
            fprintf(stderr, "Process %d is BASH and will not be terminated\n", pid);
            continue;
        }

        if (kill(pid, SIGKILL) != 0)
        {
            if (errno == ESRCH)
            {
                fprintf(stderr, "Failed to terminate %d: %s\n", pid, strerror(errno));
                continue;
            }

            fprintf(stderr, "Failed to kill %d: %s\n", (int)pid, strerror(errno));
        }
    }

    free_proc_list(&descendants);
}

/**
 * -odt：找到 process_id 的所有后代中最早创建的那个进程（即 starttime 最小的那个进程），输出它的 PID 和创建时间（格式为 "Wed 01 Jan 2020 12:00:00 PM UTC"）。如果有多个后代的 starttime 一样，则选择 PID 最小的那个。
 * 实现思路：
 * 第一次遍历 /proc 中的每个 pid，对每个 pid 向上追父链，如果在追溯过程中遇到 process_id 就把这个 pid 的信息加入 descendants_out 中，并且这个 pid 不再继续追父链了
 * 第二次遍历 descendants_out，找到 starttime 最小的那个进程，如果有多个后代的 starttime 一样，则选择 PID 最小的那个
 * 第三次根据找到的进程的 starttime 和系统启动时间计算出这个进程的创建时间，并按照指定格式输出
 */
static void opt_odt(pid_t process_id)
{
    ProcList descendants = {0};
    if (collect_descendants(process_id, &descendants) != 0)
        die_perror("collect_descendants");

    if (descendants.count == 0)
    {
        free_proc_list(&descendants);
        return;
    }

    size_t oldest_idx = (size_t)-1;
    for (size_t i = 0; i < descendants.count; i++)
    {
        if (descendants.proc_items[i].start_ticks < 0)
            continue;

        if (oldest_idx == (size_t)-1)
        {
            oldest_idx = i;
            continue;
        }

        if (descendants.proc_items[i].start_ticks < descendants.proc_items[oldest_idx].start_ticks)
            oldest_idx = i;
        else if (descendants.proc_items[i].start_ticks == descendants.proc_items[oldest_idx].start_ticks &&
                 descendants.proc_items[i].pid < descendants.proc_items[oldest_idx].pid)
            oldest_idx = i;
    }

    if (oldest_idx != (size_t)-1)
    {
        time_t boot_time = 0;
        long clk_tck = sysconf(_SC_CLK_TCK);
        char time_buf[128] = "unknown";

        if (clk_tck > 0 && read_boot_time_epoch(&boot_time) == 0)
        {
            time_t creation_time = boot_time +
                                   (time_t)(descendants.proc_items[oldest_idx].start_ticks / clk_tck);
            struct tm tm_local;
            if (localtime_r(&creation_time, &tm_local) != NULL)
            {
                if (strftime(time_buf, sizeof(time_buf), "%a %d %b %Y %I:%M:%S %p %Z", &tm_local) == 0)
                    strcpy(time_buf, "unknown");
            }
        }

        printf("Oldest descendant of %d is %d, whose creation time is %s\n",
               (int)process_id,
               (int)descendants.proc_items[oldest_idx].pid,
               time_buf);
    }

    free_proc_list(&descendants);
}

/**
 * -ndt：找到 process_id 的所有后代中最新创建的那个进程（即 starttime 最大的那个进程），输出它的 PID。 如果有多个后代的 starttime 一样，则选择 PID 最小的那个。
 * 实现思路：
 * 第一次遍历 /proc 中的每个 pid，对每个 pid 向上追父链，如果在追溯过程中遇到 process_id 就把这个 pid 的信息加入 descendants_out 中，并且这个 pid 不再继续追父链了
 * 第二次遍历 descendants_out，找到 starttime 最大的那个进程，如果有多个后代的 starttime 一样，则选择 PID 最小的那个
 * 第三次输出找到的进程的 PID
 */
static void opt_ndt(pid_t process_id)
{
    ProcList descendants = {0};
    if (collect_descendants(process_id, &descendants) != 0)
        die_perror("collect_descendants");

    if (descendants.count == 0)
    {
        free_proc_list(&descendants);
        return;
    }

    size_t newest_idx = (size_t)-1;
    for (size_t i = 0; i < descendants.count; i++)
    {
        if (descendants.proc_items[i].start_ticks < 0)
            continue;

        if (newest_idx == (size_t)-1)
        {
            newest_idx = i;
            continue;
        }

        if (descendants.proc_items[i].start_ticks > descendants.proc_items[newest_idx].start_ticks)
            newest_idx = i;
        else if (descendants.proc_items[i].start_ticks == descendants.proc_items[newest_idx].start_ticks &&
                 descendants.proc_items[i].pid < descendants.proc_items[newest_idx].pid)
            newest_idx = i;
    }

    if (newest_idx != (size_t)-1)
        printf("%d\n", (int)descendants.proc_items[newest_idx].pid);

    free_proc_list(&descendants);
}

/**
 * -dnd：统计 process_id 的后代中已经死亡的数量（即状态是 Z 的进程数量）。
 * 实现思路：
 * 1. 用 collect_descendants(process_id, &descendants) 拿到所有后代
 * 2. 统计直接子进程数（ppid == process_id）
 * 3. 计算非直接后代数：descendants.count - direct_children_count
 * 4. 输出该数量
 */
static void opt_dnd(pid_t process_id)
{
    ProcList descendants = {0};
    if (collect_descendants(process_id, &descendants) != 0)
        die_perror("collect_descendants");

    size_t direct_children_count = 0;
    for (size_t i = 0; i < descendants.count; i++)
    {
        if (descendants.proc_items[i].ppid == process_id)
            direct_children_count++;
    }

    size_t non_direct_count = descendants.count - direct_children_count;
    printf("%zu\n", non_direct_count);

    free_proc_list(&descendants);
}

/**
 * -sst：向 process_id 的所有后代发送 SIGSTOP 信号，要求它们暂停。要求不暂停 bash 进程（即使 bash 进程是 process_id 的后代），以免影响用户的正常操作。
 * 实现思路：
 * 1. 先读取 process_id 的父进程 PPID
 * 2. 扫描 proc 找所有同 PPID 的 sibling（排除自己）
 * 3. 对每个 sibling 发送 SIGSTOP
 */
static void opt_sst(pid_t process_id)
{
    ProcList siblings = {0};
    if (collect_siblings(process_id, &siblings) != 0)
    {
        fprintf(stderr, "Cannot determine siblings of process %d\n", (int)process_id);
        return;
    }

    for (size_t i = 0; i < siblings.count; i++)
    {
        pid_t pid = siblings.proc_items[i].pid;
        if (siblings.proc_items[i].is_bash)
        {
            fprintf(stderr, "Process %d is BASH and will not be stopped\n", (int)pid);
            continue;
        }

        if (kill(pid, SIGSTOP) != 0)
        {
            if (errno == ESRCH)
                continue;

            fprintf(stderr, "Failed to stop %d: %s\n", (int)pid, strerror(errno));
        }
    }

    free_proc_list(&siblings);
}

/**
 * -sco：向 process_id 的所有后代发送 SIGCONT 信号，要求它们继续。要求不继续 bash 进程（即使 bash 进程是 process_id 的后代），以免影响用户的正常操作。
 * 实现思路：
 * 1. 先读取 process_id 的父进程 PPID
 * 2. 扫描 proc 找到 siblings（同 PPID、排除自己）
 * 3. 仅对状态为 T（stopped）的 sibling 发送 SIGCONT
 */
static void opt_sco(pid_t process_id)
{
    ProcList siblings = {0};
    if (collect_siblings(process_id, &siblings) != 0)
    {
        fprintf(stderr, "Cannot determine siblings of process %d\n", (int)process_id);
        return;
    }

    for (size_t i = 0; i < siblings.count; i++)
    {
        pid_t pid = siblings.proc_items[i].pid;

        if (siblings.proc_items[i].is_bash)
        {
            fprintf(stderr, "Process %d is BASH and will not be continued\n", (int)pid);
            continue;
        }

        if (siblings.proc_items[i].state != 'T')
            continue;

        if (kill(pid, SIGCONT) != 0)
        {
            if (errno == ESRCH)
                continue;

            fprintf(stderr, "Failed to continue %d: %s\n", (int)pid, strerror(errno));
        }
    }

    free_proc_list(&siblings);
}

/**
 * -kgp：向 process_id 的父进程发送 SIGKILL 信号，要求它终止。要求不杀死 bash 进程（即使 bash 进程是 process_id 的父进程），以免影响用户的正常操作。
 * 实现思路：
 * 1. 先读取 process_id 的父进程 PPID，记为 parent_pid
 * 2. 再读取 parent_pid 的父进程 PPID，记为 grandparent_pid
 * 3. 如果 grandparent_pid 不存在或者不大于 1，就说明 parent_pid 没有父进程或者父进程是 init 进程了，这时不杀死 parent_pid
 * 4. 如果 grandparent_pid 是 bash 进程，就不杀死 parent_pid
 * 5. 否则，向 grandparent_pid 发送 SIGKILL 信号
 */
static void opt_kgp(pid_t process_id)
{
    pid_t parent_pid = -1;
    if (read_ppid_from_proc(process_id, &parent_pid) != 0 || parent_pid <= 0)
    {
        fprintf(stderr, "Cannot determine parent process of %d\n", (int)process_id);
        return;
    }

    pid_t grandparent_pid = -1;
    if (read_ppid_from_proc(parent_pid, &grandparent_pid) != 0 || grandparent_pid <= 0)
    {
        fprintf(stderr, "Cannot determine grandparent process of %d\n", (int)process_id);
        return;
    }

    if (grandparent_pid <= 1) // 为系统进程，不能杀死
    {
        fprintf(stderr, "Grandparent is not a user process and will not be terminated\n");
        return;
    }

    if (is_bash_process(grandparent_pid))
    {
        fprintf(stderr, "Grandparent is BASH and will not be terminated\n");
        return;
    }

    if (kill(grandparent_pid, SIGKILL) != 0)
    {
        if (errno == ESRCH)
        {
            fprintf(stderr, "Grandparent process %d no longer exists\n", (int)grandparent_pid);
            return;
        }

        fprintf(stderr, "Failed to kill grandparent %d: %s\n", (int)grandparent_pid, strerror(errno));
        return;
    }

    printf("SIGKILL was sent to process %d\n", (int)grandparent_pid);
}

/**
 * -kgp：向 process_id 的父进程发送 SIGKILL 信号，要求它终止。要求不杀死 bash 进程（即使 bash 进程是 process_id 的父进程），以免影响用户的正常操作。
 * 实现思路：
 * 1. 先读取 process_id 的父进程 PPID，记为 parent_pid
 * 2. 如果 parent_pid 不存在或者不大于 1，就说明 process_id 没有父进程或者父进程是 init 进程了，这时不杀死 parent_pid
 * 3. 如果 parent_pid 是 bash 进程，就不杀死 parent_pid
 * 4. 否则，向 parent_pid 发送 SIGKILL 信号
 */
static void opt_kpp(pid_t process_id)
{
    pid_t parent_pid = -1;
    if (read_ppid_from_proc(process_id, &parent_pid) != 0 || parent_pid <= 0)
    {
        fprintf(stderr, "Cannot determine parent process of %d\n", (int)process_id);
        return;
    }

    if (parent_pid <= 1)
    {
        fprintf(stderr, "Parent is not a user process and will not be terminated\n");
        return;
    }

    if (is_bash_process(parent_pid))
    {
        fprintf(stderr, "Parent is BASH and will not be terminated\n");
        return;
    }

    if (kill(parent_pid, SIGKILL) != 0)
    {
        if (errno == ESRCH)
        {
            fprintf(stderr, "Parent process %d no longer exists\n", (int)parent_pid);
            return;
        }

        fprintf(stderr, "Failed to kill parent %d: %s\n", (int)parent_pid, strerror(errno));
        return;
    }

    printf("SIGKILL was sent to process %d\n", (int)parent_pid);
}

/**
 * -kgp：向 process_id 的所有兄弟进程发送 SIGKILL 信号，要求它们终止。要求不杀死 bash 进程（即使 bash 进程是 process_id 的兄弟进程），以免影响用户的正常操作。
 * 实现思路：
 * 1. 复用 collect_siblings(process_id, &siblings) 收集兄弟进程
 * 2. 对每个 sibling 发送 SIGKILL
 * 3. 遵守保护规则：bash sibling 不终止并提示
 * 4. 错误处理：ESRCH 忽略，其它错误打印原因
 */
static void opt_ksp(pid_t process_id)
{
    ProcList siblings = {0};
    if (collect_siblings(process_id, &siblings) != 0)
    {
        fprintf(stderr, "Cannot determine siblings of process %d\n", (int)process_id);
        return;
    }

    for (size_t i = 0; i < siblings.count; i++)
    {
        pid_t pid = siblings.proc_items[i].pid;

        if (siblings.proc_items[i].is_bash)
        {
            fprintf(stderr, "Process %d is BASH and will not be terminated\n", (int)pid);
            continue;
        }

        if (kill(pid, SIGKILL) != 0)
        {
            if (errno == ESRCH)
                continue;

            fprintf(stderr, "Failed to kill sibling %d: %s\n", (int)pid, strerror(errno));
        }
    }

    free_proc_list(&siblings);
}

/**
 * -kgp：向 process_id 的父进程的所有兄弟进程发送 SIGKILL 信号，要求它们终止。要求不杀死 bash 进程（即使 bash 进程是 process_id 的兄弟进程），以免影响用户的正常操作。
 * 实现思路：
 * 1. 先读取 process_id 的父进程 parent_pid
 * 2. 复用 collect_siblings(parent_pid, &parent_siblings) 获取“父进程的 siblings（叔伯）”
 * 3. 对这些进程发送 SIGKILL
 */
static void opt_kps(pid_t process_id)
{
    pid_t parent_pid = -1;
    if (read_ppid_from_proc(process_id, &parent_pid) != 0 || parent_pid <= 0)
    {
        fprintf(stderr, "Cannot determine parent process of %d\n", (int)process_id);
        return;
    }

    ProcList parent_siblings = {0};
    if (collect_siblings(parent_pid, &parent_siblings) != 0)
    {
        fprintf(stderr, "Cannot determine siblings of parent process %d\n", (int)parent_pid);
        return;
    }

    for (size_t i = 0; i < parent_siblings.count; i++)
    {
        pid_t pid = parent_siblings.proc_items[i].pid;

        if (parent_siblings.proc_items[i].is_bash)
        {
            fprintf(stderr, "Process %d is BASH and will not be terminated\n", (int)pid);
            continue;
        }

        if (kill(pid, SIGKILL) != 0)
        {
            if (errno == ESRCH)
                continue;

            fprintf(stderr, "Failed to kill parent sibling %d: %s\n", (int)pid, strerror(errno));
        }
    }

    free_proc_list(&parent_siblings);
}

/**
 * -kgc：向 process_id 的所有孙子进程发送 SIGKILL 信号，要求它们终止。要求不杀死 bash 进程（即使 bash 进程是 process_id 的子进程），以免影响用户的正常操作。
 * 实现思路：
 * 1. 先用 collect_descendants(process_id, &descendants) 收集后代
 * 2. 识别直接子进程（ppid == process_id）
 * 3. 再筛选并终止这些子进程的直接子进程（即 grandchildren）
 */
static void opt_kgc(pid_t process_id)
{
    ProcList descendants = {0};
    if (collect_descendants(process_id, &descendants) != 0)
    {
        fprintf(stderr, "Cannot collect descendants of process %d\n", (int)process_id);
        return;
    }

    pid_t *child_pids = NULL;
    size_t child_count = 0;

    for (size_t i = 0; i < descendants.count; i++)
    {
        if (descendants.proc_items[i].ppid == process_id)
        {
            if (append_pid(&child_pids, &child_count, descendants.proc_items[i].pid) != 0)
            {
                free(child_pids);
                free_proc_list(&descendants);
                die_perror("realloc");
            }
        }
    }

    for (size_t i = 0; i < descendants.count; i++)
    {
        Proc *proc = &descendants.proc_items[i];

        if (!pid_in_list(proc->ppid, child_pids, child_count))
            continue;

        if (proc->is_bash)
        {
            fprintf(stderr, "Process %d is BASH and will not be terminated\n", (int)proc->pid);
            continue;
        }

        if (kill(proc->pid, SIGKILL) != 0)
        {
            if (errno == ESRCH)
                continue;

            fprintf(stderr, "Failed to kill grandchild %d: %s\n", (int)proc->pid, strerror(errno));
        }
    }

    free(child_pids);
    free_proc_list(&descendants);
}

/**
 * -kcp：向 process_id 的所有子进程发送 SIGKILL 信号，要求它们终止。要求不杀死 bash 进程（即使 bash 进程是 process_id 的子进程），以免影响用户的正常操作。
 * 实现思路：
 * 1. 先用 collect_descendants(process_id, &descendants) 收集后代
 * 2. 识别直接子进程（ppid == process_id）
 * 3. 对这些直接子进程发送 SIGKILL
 */
static void opt_kcp(pid_t process_id)
{
    ProcList descendants = {0};
    if (collect_descendants(process_id, &descendants) != 0)
    {
        fprintf(stderr, "Cannot collect descendants of process %d\n", (int)process_id);
        return;
    }

    for (size_t i = 0; i < descendants.count; i++)
    {
        Proc *proc = &descendants.proc_items[i];

        if (proc->ppid != process_id)
            continue;

        if (proc->is_bash)
        {
            fprintf(stderr, "Process %d is BASH and will not be terminated\n", (int)proc->pid);
            continue;
        }

        if (kill(proc->pid, SIGKILL) != 0)
        {
            if (errno == ESRCH)
                continue;

            fprintf(stderr, "Failed to kill child %d: %s\n", (int)proc->pid, strerror(errno));
        }
    }

    free_proc_list(&descendants);
}

/**
 * -krp：向 root_process 发送 SIGKILL 信号，要求它终止。要求不杀死 bash 进程（即使 bash 进程是 root_process），以免影响用户的正常操作。
 * 实现思路：
 * 1. 如果 root_process 不存在或者不大于 1，就说明 root_process 没有父进程或者父进程是 init 进程了，这时不杀死 root_process
 * 2. 如果 root_process 是 bash 进程，就不杀死 root_process
 * 3. 否则，向 root_process 发送 SIGKILL 信号
 */
static void opt_krp(pid_t root_process)
{
    if (root_process <= 1)
    {
        fprintf(stderr, "Root process is not a user process and will not be terminated\n");
        return;
    }

    if (is_bash_process(root_process))
    {
        fprintf(stderr, "Root process is BASH and will not be terminated\n");
        return;
    }

    if (kill(root_process, SIGKILL) != 0)
    {
        if (errno == ESRCH)
        {
            fprintf(stderr, "Root process %d no longer exists\n", (int)root_process);
            return;
        }

        fprintf(stderr, "Failed to kill root process %d: %s\n", (int)root_process, strerror(errno));
        return;
    }

    printf("SIGKILL was sent to process %d\n", (int)root_process);
}

/**
 * -mmd：列出 process_id 的后代中占用内存最多的进程（VmRSS 最大）以及该 VmRSS 值。
 * 实现思路：
 * 1. 先用 collect_descendants(process_id, &descendants) 收集后代
 * 2. 遍历 descendants，找出最大 vmrss_bytes
 * 3. 再遍历一次，输出所有 vmrss_bytes 等于最大值的后代（处理并列）
 * 4. 输出最大 VmRSS（bytes）
 */
static void opt_mmd(pid_t process_id)
{
    ProcList descendants = {0};
    if (collect_descendants(process_id, &descendants) != 0)
        die_perror("collect_descendants");

    if (descendants.count == 0)
    {
        free_proc_list(&descendants);
        return;
    }

    long long max_vmrss = -1;
    for (size_t i = 0; i < descendants.count; i++)
    {
        if (descendants.proc_items[i].vmrss_bytes > max_vmrss)
            max_vmrss = descendants.proc_items[i].vmrss_bytes;
    }

    if (max_vmrss < 0)
    {
        fprintf(stderr, "No VmRSS information available for descendants of %d\n", (int)process_id);
        free_proc_list(&descendants);
        return;
    }

    for (size_t i = 0; i < descendants.count; i++)
    {
        if (descendants.proc_items[i].vmrss_bytes == max_vmrss)
        {
            printf("%d is the descendant of %d consuming the most memory.\n",
                   (int)descendants.proc_items[i].pid,
                   (int)process_id);
        }
    }
    printf("VmRSS: %lld bytes\n", max_vmrss);

    free_proc_list(&descendants);
}

/**
 * -mpd：列出 process_id 的后代中累计 CPU 时间最多的进程（utime+stime 最大），并输出该累计时钟 tick 值。
 * 实现思路：
 * 1. 先用 collect_descendants(process_id, &descendants) 收集后代
 * 2. 对每个后代计算 cpu_ticks = utime_ticks + stime_ticks，找出最大值
 * 3. 再遍历一次，输出所有 cpu_ticks 等于最大值的后代（处理并列）
 * 4. 输出最大累计 CPU 时间（clock ticks）
 */
static void opt_mpd(pid_t process_id)
{
    ProcList descendants = {0};
    if (collect_descendants(process_id, &descendants) != 0)
        die_perror("collect_descendants");

    if (descendants.count == 0)
    {
        free_proc_list(&descendants);
        return;
    }

    long max_cpu_ticks = -1;
    for (size_t i = 0; i < descendants.count; i++)
    {
        long cpu_ticks = descendants.proc_items[i].utime_ticks + descendants.proc_items[i].stime_ticks;
        if (cpu_ticks > max_cpu_ticks)
            max_cpu_ticks = cpu_ticks;
    }

    for (size_t i = 0; i < descendants.count; i++)
    {
        long cpu_ticks = descendants.proc_items[i].utime_ticks + descendants.proc_items[i].stime_ticks;
        if (cpu_ticks == max_cpu_ticks)
        {
            printf("%d is the descendant of %d that has used the most CPU time.\n",
                   (int)descendants.proc_items[i].pid,
                   (int)process_id);
        }
    }
    printf("Total CPU time: %ld clock ticks\n", max_cpu_ticks);

    free_proc_list(&descendants);
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
    else if (strcmp(opt, "-oct") == 0)
        opt_oct(process_id);
    else if (strcmp(opt, "-dtm") == 0)
        opt_dtm(process_id);
    else if (strcmp(opt, "-odt") == 0)
        opt_odt(process_id);
    else if (strcmp(opt, "-ndt") == 0)
        opt_ndt(process_id);
    else if (strcmp(opt, "-dnd") == 0)
        opt_dnd(process_id);
    else if (strcmp(opt, "-sst") == 0)
        opt_sst(process_id);
    else if (strcmp(opt, "-sco") == 0)
        opt_sco(process_id);
    else if (strcmp(opt, "-kgp") == 0)
        opt_kgp(process_id);
    else if (strcmp(opt, "-kpp") == 0)
        opt_kpp(process_id);
    else if (strcmp(opt, "-ksp") == 0)
        opt_ksp(process_id);
    else if (strcmp(opt, "-kps") == 0)
        opt_kps(process_id);
    else if (strcmp(opt, "-kgc") == 0)
        opt_kgc(process_id);
    else if (strcmp(opt, "-kcp") == 0)
        opt_kcp(process_id);
    else if (strcmp(opt, "-krp") == 0)
        opt_krp(root_process);
    else if (strcmp(opt, "-mmd") == 0)
        opt_mmd(process_id);
    else if (strcmp(opt, "-mpd") == 0)
        opt_mpd(process_id);
    else
    {
        printf("Unknown option: %s\n", opt);
        usage(argv[0]);
    }

    return 0;
}