#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_LINE 512
#define MAX_ARGC 4
#define MAX_BG 256

pid_t bg_pids[MAX_BG];
int bg_count;

pid_t last_bg_pid;
pid_t last_stopped_pid;

typedef enum
{
    NONE,
    BUILTIN,
    SEQUENTIAL,
    CONDITIONAL,
    PIPE_CHAIN,
    REVERSE_PIPE_CHAIN,
    FIFO_WRITE,
    FIFO_READ,
    REDIRECTION,
    TXT_APPEND_BOTH,
    WORD_COUNT,
    TXT_CONCAT,
    SIMPLE_BACKGROUND,
    SIMPLE
} CommandType;

typedef struct
{
    CommandType type;
    char *argv[MAX_ARGC + 1];
    int argc;
    char *infile;   // filename after <
    char *outfile;  // filename after > or >>
    int append_out; // 1 → >>  (append), 0 → >
} CommandInfo;

// 移除字符串首尾的空白字符
static void trim_inplace(char *s)
{
    if (s == NULL)
        return;

    int start = 0;
    int end = strlen(s) - 1;
    int i, j = 0;

    // 找前面的第一个非空格字符
    while (s[start] != '\0' && isspace((unsigned char)s[start]))
        start++;

    // 如果全是空格
    if (s[start] == '\0')
    {
        s[0] = '\0';
        return;
    }

    // 找后面的第一个非空格字符
    while (end >= 0 && isspace((unsigned char)s[end]))
        end--;

    // 把中间有效内容搬到前面
    for (i = start; i <= end; i++)
    {
        s[j] = s[i];
        j++;
    }

    s[j] = '\0';
}

/* Add a pid to the background list. */
static void bg_add(pid_t pid)
{
    if (bg_count < MAX_BG)
    {
        bg_pids[bg_count++] = pid;
        last_bg_pid = pid;
    }
}

/* List of builtin command names, used only for type detection. */
static const char *BUILTIN_NAMES[] = {
    "killmb", "killallmb", "pstop", "cont", "numbg", "killbp", NULL};

static bool is_builtin(const char *name)
{
    int i;
    for (i = 0; BUILTIN_NAMES[i] != NULL; i++)
        if (strcmp(name, BUILTIN_NAMES[i]) == 0)
            return true;
    return false;
}

// 解析命令，识别命令类型并调用对应的执行函数
static CommandType resolve_command_type(char *line)
{
    // 解析命令，返回CommandType
    if (line[0] == '\0')
        return NONE;

    /* Builtin commands are handled first (no fork needed).
     * Only check the first token (command name) against known builtins. */
    {
        char tmp[MAX_LINE];
        char *sp = NULL;
        strncpy(tmp, line, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        char *first = strtok_r(tmp, " \t", &sp);
        if (first != NULL && is_builtin(first))
            return BUILTIN;
    }

    /* ';' sequential */
    if (strchr(line, ';') != NULL)
        return SEQUENTIAL;

    /* '|||' FIFO  – must be before single '|' check */
    if (strstr(line, "|||") != NULL)
    {
        if (strncmp(line, "|||", 3) == 0)
            return FIFO_READ; /* |||cmd  */
        return FIFO_WRITE;    /* cmd|||  */
    }

    /* '&&' or '||' conditional */
    if (strstr(line, "&&") != NULL || strstr(line, "||") != NULL)
        return CONDITIONAL;

    /* '~' reverse pipe */
    if (strchr(line, '~') != NULL)
        return REVERSE_PIPE_CHAIN;

    /* '|' forward pipe */
    if (strchr(line, '|') != NULL)
        return PIPE_CHAIN;

    /* '<' '>' '>>' redirection */
    if (strchr(line, '<') != NULL || strchr(line, '>') != NULL)
        return REDIRECTION;

    /* '&' background (trailing) */
    if (strchr(line, '&') != NULL)
        return SIMPLE_BACKGROUND;

    /* '++' bidirectional txt append – before single '+' */
    if (strstr(line, "++") != NULL)
        return TXT_APPEND_BOTH;

    /* '#' word-count shorthand */
    if (line[0] == '#')
        return WORD_COUNT;

    /* '+' txt concatenation */
    if (strchr(line, '+') != NULL)
        return TXT_CONCAT;

    /* Plain command */
    return SIMPLE;
}

// 把命令参数解析到CommandInfo结构体中，识别重定向符号和参数
static bool resolve_command(char *line, CommandInfo *cmd_out)
{
    char *saveptr = NULL;
    char *token;
    char buf[MAX_LINE];

    memset(cmd_out, 0, sizeof(*cmd_out));
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    token = strtok_r(buf, " \t", &saveptr);
    while (token != NULL)
    {
        if (strcmp(token, "<") == 0)
        {
            char *infile = strtok_r(NULL, " \t", &saveptr);
            cmd_out->infile = infile ? strdup(infile) : NULL;
        }
        else if (strcmp(token, ">>") == 0)
        {
            char *outfile = strtok_r(NULL, " \t", &saveptr);
            cmd_out->outfile = outfile ? strdup(outfile) : NULL;
            cmd_out->append_out = 1;
        }
        else if (strcmp(token, ">") == 0)
        {
            char *outfile = strtok_r(NULL, " \t", &saveptr);
            cmd_out->outfile = outfile ? strdup(outfile) : NULL;
            cmd_out->append_out = 0;
        }
        else
        {
            // Inline redirection: tok starts with < > >>
            if (token[0] == '>' && token[1] == '>')
            {
                cmd_out->outfile = strdup(token + 2);
                cmd_out->append_out = 1;
            }
            else if (token[0] == '>')
            {
                cmd_out->outfile = strdup(token + 1);
                cmd_out->append_out = 0;
            }
            else if (token[0] == '<')
            {
                cmd_out->infile = strdup(token + 1);
            }
            else
            {
                /* Skip the '&' background marker – it is a shell operator,
                 * not an argument to the command. Also handle the case where
                 * '&' is glued to the last token (e.g. "sleep 5&"). */
                size_t tlen = strlen(token);
                if (tlen > 0 && token[tlen - 1] == '&')
                    token[tlen - 1] = '\0'; /* strip trailing & in-place */

                if (strcmp(token, "&") == 0 || token[0] == '\0')
                {
                    /* standalone '&' or now-empty token → skip */
                }
                else if (cmd_out->argc < MAX_ARGC)
                {
                    /* strdup: token points into local buf which is freed on
                     * return; we need heap-allocated copies that outlive this
                     * stack frame so execvp can see valid strings. */
                    cmd_out->argv[cmd_out->argc] = strdup(token);
                    if (cmd_out->argv[cmd_out->argc] == NULL)
                    {
                        perror("strdup");
                        exit(1);
                    }
                    cmd_out->argc++;
                }
            }
        }
        token = strtok_r(NULL, " \t", &saveptr);
    }

    cmd_out->argv[cmd_out->argc] = NULL;

    if (cmd_out->argc < 1 || cmd_out->argc > MAX_ARGC)
    {
        fprintf(stderr, "minibash: invalid argument count %d (max %d) in command: %s\n",
                cmd_out->argc, MAX_ARGC, line);
        /* free any argv already strdup'd before returning failure */
        int k;
        for (k = 0; k < cmd_out->argc; k++)
        {
            free(cmd_out->argv[k]);
            cmd_out->argv[k] = NULL;
        }
        return false;
    }

    // 识别命令类型
    CommandType type_result = resolve_command_type(line);
    cmd_out->type = type_result;

    return true;
}

// 设置输入重定向
static void free_cmdinfo(CommandInfo *c)
{
    int i;
    for (i = 0; i < c->argc; i++)
    {
        free(c->argv[i]);
        c->argv[i] = NULL;
    }
    free(c->infile);
    c->infile = NULL;
    free(c->outfile);
    c->outfile = NULL;
}

// 设置输入重定向
static void setup_redirection_in(const CommandInfo *cmd_info)
{
    int fd;
    fd = open(cmd_info->infile, O_RDONLY);
    if (fd < 0)
    {
        perror(cmd_info->infile);
        exit(1);
    }
    if (dup2(fd, STDIN_FILENO) < 0)
    {
        perror("dup2 stdin");
        exit(1);
    }
    close(fd);
}

// 设置输出重定向，支持 > 和 >> 两种模式
static void setup_redirection_out(const CommandInfo *cmd_info)
{
    int fd;
    int flags = O_WRONLY | O_CREAT | (cmd_info->append_out ? O_APPEND : O_TRUNC);
    fd = open(cmd_info->outfile, flags, 0644);
    if (fd < 0)
    {
        perror(cmd_info->outfile);
        exit(1);
    }
    if (dup2(fd, STDOUT_FILENO) < 0)
    {
        perror("dup2 stdout");
        exit(1);
    }
    close(fd);
}

/*
 * run_cmd – the single fork+exec helper used by all executors.
 *
 * If foreground=true:  parent waits and returns exit status.
 * If foreground=false: parent returns immediately; pid stored in bg list.
 *
 * stdin_fd / stdout_fd: pass -1 to keep current fd; otherwise dup2 in child.
 * (Used by the pipe chain builder.)
 */
static int run_cmd(CommandInfo *cmd_info, bool foreground, int stdin_fd, int stdout_fd)
{
    pid_t pid;
    int status = 0;

    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return -1;
    }

    if (pid == 0)
    {
        /* ---- child ---- */
        if (stdin_fd != -1)
        {
            dup2(stdin_fd, STDIN_FILENO);
            close(stdin_fd);
        }
        if (stdout_fd != -1)
        {
            dup2(stdout_fd, STDOUT_FILENO);
            close(stdout_fd);
        }

        // 处理输入重定向
        if (cmd_info->infile != NULL)
        {
            setup_redirection_in(cmd_info);
        }
        // 处理输出重定向
        if (cmd_info->outfile != NULL)
        {
            setup_redirection_out(cmd_info);
        }
        execvp(cmd_info->argv[0], cmd_info->argv);
        fprintf(stderr, "minibash: %s: %s\n", cmd_info->argv[0], strerror(errno));
        exit(123); // execvp failed
    }

    /* ---- parent ---- */
    if (foreground)
    {
        if (waitpid(pid, &status, 0) < 0)
            perror("waitpid");
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    else
    {
        bg_add(pid);
        printf("[bg] pid %d started\n", (int)pid);
        return 0;
    }
}

// 普通命令的执行函数, 直接调用系统函数执行命令
static int execute_simple_command(CommandInfo *cmd_info)
{
    // foreground cmd
    run_cmd(cmd_info, true, -1, -1);
    return 0;
}

// 后台命令
static int execute_background_command(CommandInfo *cmd_info)
{
    // background cmd
    run_cmd(cmd_info, false, -1, -1);
    return 0;
}

// 内置命令：执行所有内建命令逻辑
static int handle_builtin(CommandInfo *cmd_info)
{
    const char *name = cmd_info->argv[0];

    /* killmb – exit this minibash instance */
    if (strcmp(name, "killmb") == 0)
    {
        printf("minibash: exiting.\n");
        exit(0);
    }

    /* killallmb – kill every minibash process on the system */
    if (strcmp(name, "killallmb") == 0)
    {
        /* TODO: scan process list and kill all other minibash instances.
         *       Use getpid() to exclude self. */
        fprintf(stderr, "TODO: killallmb\n");
        return 0;
    }

    /* pstop – SIGSTOP the most recently started background process */
    if (strcmp(name, "pstop") == 0)
    {
        if (last_bg_pid == -1)
        {
            fprintf(stderr, "minibash: no background process to stop.\n");
        }
        else
        {
            if (kill(last_bg_pid, SIGSTOP) < 0)
                perror("pstop");
            else
            {
                last_stopped_pid = last_bg_pid;
                printf("[pstop] stopped pid %d\n", (int)last_bg_pid);
            }
        }
        return 0;
    }

    /* cont – SIGCONT the most recently stopped process and wait for it */
    if (strcmp(name, "cont") == 0)
    {
        if (last_stopped_pid == -1)
        {
            fprintf(stderr, "minibash: no stopped process to continue.\n");
        }
        else
        {
            if (kill(last_stopped_pid, SIGCONT) < 0)
                perror("cont");
            else
            {
                printf("[cont] continuing pid %d in foreground\n", (int)last_stopped_pid);
                int status;
                waitpid(last_stopped_pid, &status, 0);
                last_stopped_pid = -1;
            }
        }
        return 0;
    }

    /* numbg – print current live background process count */
    if (strcmp(name, "numbg") == 0)
    {
        printf("Background processes: %d\n", bg_count);
        return 0;
    }

    /* killbp – kill all background processes in current minibash */
    if (strcmp(name, "killbp") == 0)
    {
        int i;
        for (i = 0; i < bg_count; i++)
        {
            if (kill(bg_pids[i], SIGKILL) < 0)
                perror("killbp");
            else
                printf("[killbp] killed pid %d\n", (int)bg_pids[i]);
        }
        bg_count = 0;
        return 0;
    }

    return -1; /* not a builtin */
}

// 顺序执行：按 ';' 拆分原始行，依次执行每个子命令（最多 4 个）
static int execute_sequential(char *line)
{
    char buf[MAX_LINE];
    char *saveptr = NULL;
    char *segment;
    int seg_count = 0;
    int ret = 0;

    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    segment = strtok_r(buf, ";", &saveptr);
    while (segment != NULL)
    {
        trim_inplace(segment);

        if (segment[0] == '\0')
        {
            segment = strtok_r(NULL, ";", &saveptr);
            continue;
        }

        seg_count++;
        if (seg_count > 4)
        {
            fprintf(stderr, "minibash: ';' supports at most 4 commands\n");
            return -1;
        }

        CommandInfo seg_cmd;
        if (resolve_command(segment, &seg_cmd))
        {
            ret = run_cmd(&seg_cmd, true, -1, -1);
            free_cmdinfo(&seg_cmd);
        }

        segment = strtok_r(NULL, ";", &saveptr);
    }

    return ret;
}

// 条件执行
// 条件执行：按 '&&' / '||' 拆分原始行，左到右执行（最多 4 个运算符）
// &&：前一条成功(ret==0)才执行下一条
// ||：前一条失败(ret!=0)才执行下一条
static int execute_conditional(char *line)
{
    char buf[MAX_LINE];
    char *segments[5]; /* 最多 4 个运算符 → 5 个段 */
    int ops[4];        /* 1 = &&, 0 = || */
    int nseg = 0;
    char *p, *seg_start;

    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    p = buf;
    seg_start = p;

    while (*p != '\0')
    {
        bool is_and = (p[0] == '&' && p[1] == '&');
        /* 只匹配 ||, 排除 ||| (FIFO 操作符) */
        bool is_or = (p[0] == '|' && p[1] == '|' && p[2] != '|');

        if (is_and || is_or)
        {
            if (nseg >= 4)
            {
                fprintf(stderr, "minibash: '&&'/'||' supports at most 4 operators\n");
                return -1;
            }
            /* NUL-terminate 当前段并记录 */
            *p = '\0';
            trim_inplace(seg_start);
            segments[nseg] = seg_start;
            ops[nseg] = is_and ? 1 : 0;
            nseg++;

            p += 2;
            seg_start = p;
        }
        else
        {
            p++;
        }
    }

    /* 收尾最后一段 */
    trim_inplace(seg_start);
    segments[nseg++] = seg_start;

    /* 按运算符规则逐段执行 */
    int prev_ret = 0;
    int i;
    for (i = 0; i < nseg; i++)
    {
        bool should_run;
        if (i == 0)
            should_run = true;
        else if (ops[i - 1] == 1) /* && */
            should_run = (prev_ret == 0);
        else /* || */
            should_run = (prev_ret != 0);

        if (should_run && segments[i][0] != '\0')
        {
            CommandInfo seg_cmd;
            if (resolve_command(segments[i], &seg_cmd))
            {
                prev_ret = run_cmd(&seg_cmd, true, -1, -1);
                free_cmdinfo(&seg_cmd);
            }
            else
            {
                prev_ret = -1;
            }
        }
    }

    return prev_ret;
}

// 普通管道：按 '|' 拆分原始行，依次建立管道连接各子命令（最多 4 次管道）
static int execute_pipe_chain(char *line)
{
    char buf[MAX_LINE];
    char *segs[5]; /* 最多 4 个 '|' → 5 段 */
    int nseg = 0;
    char *p, *seg_start;

    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* 扫描时跳过 '|||'，只切单个 '|' */
    p = buf;
    seg_start = p;
    while (*p != '\0')
    {
        if (p[0] == '|' && p[1] != '|')
        {
            if (nseg >= 4)
            {
                fprintf(stderr, "minibash: '|' supports at most 4 pipe operations\n");
                return -1;
            }
            *p = '\0';
            trim_inplace(seg_start);
            segs[nseg++] = seg_start;
            seg_start = p + 1;
            p = seg_start;
        }
        else
        {
            p++;
        }
    }
    trim_inplace(seg_start);
    segs[nseg++] = seg_start;

    if (nseg < 2)
        return execute_simple_command(NULL); /* 不该到这里，保险处理 */

    /* 为 nseg-1 对相邻命令建立 pipe */
    int pipefds[4][2]; /* 最多 4 条 pipe */
    int i;
    for (i = 0; i < nseg - 1; i++)
    {
        if (pipe(pipefds[i]) < 0)
        {
            perror("pipe");
            return -1;
        }
    }

    pid_t pids[5];
    for (i = 0; i < nseg; i++)
    {
        CommandInfo seg_cmd;
        if (!resolve_command(segs[i], &seg_cmd))
        {
            /* 关闭已创建的所有 pipe 端口再退出 */
            int j;
            for (j = 0; j < nseg - 1; j++)
            {
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }
            return -1;
        }

        pids[i] = fork();
        if (pids[i] < 0)
        {
            perror("fork");
            free_cmdinfo(&seg_cmd);
            return -1;
        }

        if (pids[i] == 0)
        {
            /* 子进程：把上一段的读端接到 stdin */
            if (i > 0)
            {
                dup2(pipefds[i - 1][0], STDIN_FILENO);
            }
            /* 把下一段的写端接到 stdout */
            if (i < nseg - 1)
            {
                dup2(pipefds[i][1], STDOUT_FILENO);
            }
            /* 关闭所有 pipe 端口（子进程不需要保留） */
            int j;
            for (j = 0; j < nseg - 1; j++)
            {
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }
            /* 处理可能存在的文件重定向 */
            if (seg_cmd.infile != NULL)
                setup_redirection_in(&seg_cmd);
            if (seg_cmd.outfile != NULL)
                setup_redirection_out(&seg_cmd);

            execvp(seg_cmd.argv[0], seg_cmd.argv);
            fprintf(stderr, "minibash: %s: %s\n", seg_cmd.argv[0], strerror(errno));
            exit(127);
        }

        free_cmdinfo(&seg_cmd);
    }

    /* 父进程关闭所有 pipe 端口，等待所有子进程结束 */
    for (i = 0; i < nseg - 1; i++)
    {
        close(pipefds[i][0]);
        close(pipefds[i][1]);
    }

    int last_ret = 0;
    for (i = 0; i < nseg; i++)
    {
        int status;
        waitpid(pids[i], &status, 0);
        if (i == nseg - 1)
            last_ret = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    return last_ret;
}

// 反向管道：a ~ b ~ c 等价于 c | b | a
static int execute_reverse_pipe_chain(char *line)
{
    char buf[MAX_LINE];
    char *segs[5]; /* 最多 4 个 '~' -> 5 段 */
    int nseg = 0;
    char *saveptr = NULL;
    char *segment;

    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    segment = strtok_r(buf, "~", &saveptr);
    while (segment != NULL)
    {
        trim_inplace(segment);
        if (segment[0] != '\0')
        {
            if (nseg >= 5)
            {
                fprintf(stderr, "minibash: '~' supports at most 4 pipe operations\n");
                return -1;
            }
            segs[nseg++] = segment;
        }
        segment = strtok_r(NULL, "~", &saveptr);
    }

    if (nseg < 2)
    {
        fprintf(stderr, "minibash: reverse pipe requires at least 2 commands\n");
        return -1;
    }

    /* reverse in-place */
    for (int i = 0; i < nseg / 2; i++)
    {
        char *tmp = segs[i];
        segs[i] = segs[nseg - 1 - i];
        segs[nseg - 1 - i] = tmp;
    }

    int pipefds[4][2];
    for (int i = 0; i < nseg - 1; i++)
    {
        if (pipe(pipefds[i]) < 0)
        {
            perror("pipe");
            return -1;
        }
    }

    pid_t pids[5];
    for (int i = 0; i < nseg; i++)
    {
        CommandInfo seg_cmd;
        if (!resolve_command(segs[i], &seg_cmd))
        {
            for (int j = 0; j < nseg - 1; j++)
            {
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }
            return -1;
        }

        pids[i] = fork();
        if (pids[i] < 0)
        {
            perror("fork");
            free_cmdinfo(&seg_cmd);
            return -1;
        }

        if (pids[i] == 0)
        {
            if (i > 0)
            {
                dup2(pipefds[i - 1][0], STDIN_FILENO);
            }
            if (i < nseg - 1)
            {
                dup2(pipefds[i][1], STDOUT_FILENO);
            }

            for (int j = 0; j < nseg - 1; j++)
            {
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }

            if (seg_cmd.infile != NULL)
                setup_redirection_in(&seg_cmd);
            if (seg_cmd.outfile != NULL)
                setup_redirection_out(&seg_cmd);

            execvp(seg_cmd.argv[0], seg_cmd.argv);
            fprintf(stderr, "minibash: %s: %s\n", seg_cmd.argv[0], strerror(errno));
            exit(127);
        }

        free_cmdinfo(&seg_cmd);
    }

    for (int i = 0; i < nseg - 1; i++)
    {
        close(pipefds[i][0]);
        close(pipefds[i][1]);
    }

    int last_ret = 0;
    for (int i = 0; i < nseg; i++)
    {
        int status;
        waitpid(pids[i], &status, 0);
        if (i == nseg - 1)
            last_ret = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    return last_ret;
}

// FIFO管道写入：cmd|||  将cmd输出写入公共FIFO
static int execute_fifo_write(char *line)
{
    char buf[MAX_LINE];
    char *op;
    char *cmd_part;
    const char *home;
    char dir1[PATH_MAX];
    char dir2[PATH_MAX];
    char fifo_path[PATH_MAX];

    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    op = strstr(buf, "|||");
    if (op == NULL)
    {
        fprintf(stderr, "minibash: fifo write syntax error, expected cmd|||\n");
        return -1;
    }

    *op = '\0';
    cmd_part = buf;
    trim_inplace(cmd_part);
    if (cmd_part[0] == '\0')
    {
        fprintf(stderr, "minibash: fifo write requires a command before '|||'\n");
        return -1;
    }

    home = getenv("HOME");
    if (home == NULL)
        home = ".";

    if (strlen(home) + strlen("/Assignments") >= sizeof(dir1) ||
        strlen(home) + strlen("/Assignments/Assignment3") >= sizeof(dir2) ||
        strlen(home) + strlen("/Assignments/Assignment3/minibash_fifo") >= sizeof(fifo_path))
    {
        fprintf(stderr, "minibash: fifo path too long\n");
        return -1;
    }

    snprintf(dir1, sizeof(dir1), "%s/Assignments", home);
    snprintf(dir2, sizeof(dir2), "%s/Assignments/Assignment3", home);
    snprintf(fifo_path, sizeof(fifo_path), "%s/minibash_fifo", dir2);

    if (mkdir(dir1, 0755) < 0 && errno != EEXIST)
    {
        perror("mkdir Assignments");
        return -1;
    }
    if (mkdir(dir2, 0755) < 0 && errno != EEXIST)
    {
        perror("mkdir Assignment3");
        return -1;
    }
    if (mkfifo(fifo_path, 0666) < 0 && errno != EEXIST)
    {
        perror("mkfifo");
        return -1;
    }

    CommandInfo cmd;
    if (!resolve_command(cmd_part, &cmd))
    {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        free_cmdinfo(&cmd);
        return -1;
    }

    if (pid == 0)
    {
        int fifo_fd = open(fifo_path, O_WRONLY);
        if (fifo_fd < 0)
        {
            perror("open fifo for write");
            exit(1);
        }

        if (dup2(fifo_fd, STDOUT_FILENO) < 0)
        {
            perror("dup2 fifo stdout");
            close(fifo_fd);
            exit(1);
        }
        close(fifo_fd);

        if (cmd.infile != NULL)
            setup_redirection_in(&cmd);

        execvp(cmd.argv[0], cmd.argv);
        fprintf(stderr, "minibash: %s: %s\n", cmd.argv[0], strerror(errno));
        exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    free_cmdinfo(&cmd);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
// FIFO管道读取：|||cmd  从公共FIFO读取并作为cmd的stdin
static int execute_fifo_read(char *line)
{
    char buf[MAX_LINE];
    char *cmd_part;
    const char *home;
    char dir1[PATH_MAX];
    char dir2[PATH_MAX];
    char fifo_path[PATH_MAX];

    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    if (strncmp(buf, "|||", 3) != 0)
    {
        fprintf(stderr, "minibash: fifo read syntax error, expected |||cmd\n");
        return -1;
    }

    cmd_part = buf + 3;
    trim_inplace(cmd_part);
    if (cmd_part[0] == '\0')
    {
        fprintf(stderr, "minibash: fifo read requires a command after '|||'\n");
        return -1;
    }

    home = getenv("HOME");
    if (home == NULL)
        home = ".";

    if (strlen(home) + strlen("/Assignments") >= sizeof(dir1) ||
        strlen(home) + strlen("/Assignments/Assignment3") >= sizeof(dir2) ||
        strlen(home) + strlen("/Assignments/Assignment3/minibash_fifo") >= sizeof(fifo_path))
    {
        fprintf(stderr, "minibash: fifo path too long\n");
        return -1;
    }

    snprintf(dir1, sizeof(dir1), "%s/Assignments", home);
    snprintf(dir2, sizeof(dir2), "%s/Assignments/Assignment3", home);
    snprintf(fifo_path, sizeof(fifo_path), "%s/minibash_fifo", dir2);

    if (mkdir(dir1, 0755) < 0 && errno != EEXIST)
    {
        perror("mkdir Assignments");
        return -1;
    }
    if (mkdir(dir2, 0755) < 0 && errno != EEXIST)
    {
        perror("mkdir Assignment3");
        return -1;
    }
    if (mkfifo(fifo_path, 0666) < 0 && errno != EEXIST)
    {
        perror("mkfifo");
        return -1;
    }

    CommandInfo cmd;
    if (!resolve_command(cmd_part, &cmd))
    {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        free_cmdinfo(&cmd);
        return -1;
    }

    if (pid == 0)
    {
        int fifo_fd = open(fifo_path, O_RDONLY);
        if (fifo_fd < 0)
        {
            perror("open fifo for read");
            exit(1);
        }

        if (dup2(fifo_fd, STDIN_FILENO) < 0)
        {
            perror("dup2 fifo stdin");
            close(fifo_fd);
            exit(1);
        }
        close(fifo_fd);

        if (cmd.outfile != NULL)
            setup_redirection_out(&cmd);

        execvp(cmd.argv[0], cmd.argv);
        fprintf(stderr, "minibash: %s: %s\n", cmd.argv[0], strerror(errno));
        exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    free_cmdinfo(&cmd);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// 重定向
static int execute_redirection(CommandInfo *cmd_info)
{
    // 至少需要有目标文件
    if (cmd_info->infile == NULL && cmd_info->outfile == NULL)
    {
        fprintf(stderr, "minibash: redirection: no file specified\n");
        return -1;
    }
    return run_cmd(cmd_info, true, -1, -1);
}

static bool has_txt_extension(const char *name)
{
    size_t n;
    if (name == NULL)
        return false;
    n = strlen(name);
    return (n >= 4 && strcmp(name + n - 4, ".txt") == 0);
}

static int read_all_bytes(const char *path, char **out_buf, size_t *out_len)
{
    FILE *fp;
    long sz;
    char *buf;

    *out_buf = NULL;
    *out_len = 0;

    fp = fopen(path, "rb");
    if (fp == NULL)
    {
        perror(path);
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        perror("fseek");
        return -1;
    }

    sz = ftell(fp);
    if (sz < 0)
    {
        fclose(fp);
        perror("ftell");
        return -1;
    }

    if (fseek(fp, 0, SEEK_SET) != 0)
    {
        fclose(fp);
        perror("fseek");
        return -1;
    }

    if (sz == 0)
    {
        fclose(fp);
        return 0;
    }

    buf = (char *)malloc((size_t)sz);
    if (buf == NULL)
    {
        fclose(fp);
        perror("malloc");
        return -1;
    }

    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz)
    {
        free(buf);
        fclose(fp);
        perror("fread");
        return -1;
    }

    fclose(fp);
    *out_buf = buf;
    *out_len = (size_t)sz;
    return 0;
}

// 文本双向追加 ++
static int execute_txt_append_both(CommandInfo *cmd_info)
{
    char *f1 = NULL;
    char *f2 = NULL;
    char *buf1 = NULL;
    char *buf2 = NULL;
    size_t len1 = 0;
    size_t len2 = 0;
    FILE *fp;

    if (cmd_info->argc == 3 && strcmp(cmd_info->argv[1], "++") == 0)
    {
        f1 = cmd_info->argv[0];
        f2 = cmd_info->argv[2];
    }
    else if (cmd_info->argc == 1)
    {
        char *op = strstr(cmd_info->argv[0], "++");
        if (op != NULL)
        {
            *op = '\0';
            f1 = cmd_info->argv[0];
            f2 = op + 2;
            trim_inplace(f1);
            trim_inplace(f2);
        }
    }

    if (f1 == NULL || f2 == NULL || f1[0] == '\0' || f2[0] == '\0')
    {
        fprintf(stderr, "minibash: usage: file1.txt ++ file2.txt\n");
        return -1;
    }

    if (!has_txt_extension(f1) || !has_txt_extension(f2))
    {
        fprintf(stderr, "minibash: '++' requires two .txt files\n");
        return -1;
    }

    /* 先读两边原始内容，再执行双向追加，避免先写后读污染数据。 */
    if (read_all_bytes(f1, &buf1, &len1) < 0)
        return -1;
    if (read_all_bytes(f2, &buf2, &len2) < 0)
    {
        free(buf1);
        return -1;
    }

    fp = fopen(f1, "ab");
    if (fp == NULL)
    {
        perror(f1);
        free(buf1);
        free(buf2);
        return -1;
    }
    if (len2 > 0 && fwrite(buf2, 1, len2, fp) != len2)
    {
        perror("fwrite");
        fclose(fp);
        free(buf1);
        free(buf2);
        return -1;
    }
    fclose(fp);

    fp = fopen(f2, "ab");
    if (fp == NULL)
    {
        perror(f2);
        free(buf1);
        free(buf2);
        return -1;
    }
    if (len1 > 0 && fwrite(buf1, 1, len1, fp) != len1)
    {
        perror("fwrite");
        fclose(fp);
        free(buf1);
        free(buf2);
        return -1;
    }
    fclose(fp);

    free(buf1);
    free(buf2);
    return 0;
}

// 单词计数 #
static int execute_word_count(CommandInfo *cmd_info)
{
    const char *filename = NULL;
    FILE *fp;
    int ch;
    int in_word = 0;
    int count = 0;

    /* 支持两种形式：
     * 1) # sample.txt   -> argv[0]="#", argv[1]="sample.txt"
     * 2) #sample.txt    -> argv[0]="#sample.txt"
     */
    if (cmd_info->argc >= 2 && strcmp(cmd_info->argv[0], "#") == 0)
    {
        filename = cmd_info->argv[1];
    }
    else if (cmd_info->argc >= 1 && cmd_info->argv[0][0] == '#')
    {
        filename = cmd_info->argv[0] + 1;
        while (*filename != '\0' && isspace((unsigned char)*filename))
            filename++;
    }

    if (filename == NULL || filename[0] == '\0')
    {
        fprintf(stderr, "minibash: usage: # file.txt\n");
        return -1;
    }

    if (!has_txt_extension(filename))
    {
        fprintf(stderr, "minibash: '#' requires a .txt file\n");
        return -1;
    }

    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror(filename);
        return -1;
    }

    while ((ch = fgetc(fp)) != EOF)
    {
        if (isspace((unsigned char)ch))
        {
            in_word = 0;
        }
        else if (!in_word)
        {
            in_word = 1;
            count++;
        }
    }

    fclose(fp);
    printf("%d\n", count);
    return 0;
}

// 文本拼接 +
static int execute_txt_concat(char *line)
{
    char buf[MAX_LINE];
    char *saveptr = NULL;
    char *part;
    int op_count = 0;
    int file_count = 0;

    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    part = strtok_r(buf, "+", &saveptr);
    while (part != NULL)
    {
        FILE *fp;
        char io_buf[1024];
        size_t n;

        trim_inplace(part);
        if (part[0] == '\0')
        {
            fprintf(stderr, "minibash: invalid '+' expression\n");
            return -1;
        }

        if (!has_txt_extension(part))
        {
            fprintf(stderr, "minibash: '+' requires .txt files only\n");
            return -1;
        }

        file_count++;
        if (file_count > 5)
        {
            fprintf(stderr, "minibash: '+' supports at most 4 operations\n");
            return -1;
        }

        fp = fopen(part, "rb");
        if (fp == NULL)
        {
            perror(part);
            return -1;
        }

        while ((n = fread(io_buf, 1, sizeof(io_buf), fp)) > 0)
        {
            if (fwrite(io_buf, 1, n, stdout) != n)
            {
                perror("fwrite");
                fclose(fp);
                return -1;
            }
        }
        if (ferror(fp))
        {
            perror("fread");
            fclose(fp);
            return -1;
        }

        fclose(fp);
        part = strtok_r(NULL, "+", &saveptr);
        if (part != NULL)
            op_count++;
    }

    if (file_count < 2 || op_count < 1)
    {
        fprintf(stderr, "minibash: '+' requires at least two .txt files\n");
        return -1;
    }

    return 0;
}

int main(void)
{
    char line[MAX_LINE];

    while (1)
    {
        // bg_reap(); /* reap any finished bg children  */
        printf("minibash$ ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            if (feof(stdin))
            {
                putchar('\n');
                break;
            }
            perror("fgets");
            continue;
        }
        line[strcspn(line, "\n")] = '\0'; // 去掉末尾的换行符

        trim_inplace(line);

        if (line[0] == '\0')
        {
            continue;
        }

        CommandInfo cmd_out;

        if (!resolve_command(line, &cmd_out))
        {
            continue; // 解析失败，跳过执行
        }

        CommandType cmdType = cmd_out.type;
        // 根据cmdType调用相应的执行函数
        switch (cmdType)
        {
        case BUILTIN:
            handle_builtin(&cmd_out);
            break;
        case SEQUENTIAL:
            execute_sequential(line);
            break;
        case CONDITIONAL:
            execute_conditional(line);
            break;
        case PIPE_CHAIN:
            execute_pipe_chain(line);
            break;
        case REVERSE_PIPE_CHAIN:
            execute_reverse_pipe_chain(line);
            break;
        case FIFO_WRITE:
            execute_fifo_write(line);
            break;
        case FIFO_READ:
            execute_fifo_read(line);
            break;
        case REDIRECTION:
            execute_redirection(&cmd_out);
            break;
        case TXT_APPEND_BOTH:
            execute_txt_append_both(&cmd_out);
            break;
        case WORD_COUNT:
            execute_word_count(&cmd_out);
            break;
        case TXT_CONCAT:
            execute_txt_concat(line);
            break;
        case SIMPLE_BACKGROUND:
            execute_background_command(&cmd_out);
            break;
        case SIMPLE:
            execute_simple_command(&cmd_out);
            break;
        default:
            fprintf(stderr, "Unknown command type.\n");
        }
        free_cmdinfo(&cmd_out);
    }

    return 0;
}