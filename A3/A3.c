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

static int handle_builtin2(char *line)
{
    /* killmb – exit this minibash instance */
    if (strcmp(line, "killmb") == 0)
    {
        printf("minibash: exiting.\n");
        exit(0);
    }

    /* killallmb – kill every minibash process visible in the system */
    if (strcmp(line, "killallmb") == 0)
    {
        /* TODO: scan /proc or use kill(-1, SIGTERM) with name filter.
         *       Use getpid() to exclude self.                          */
        fprintf(stderr, "TODO: killallmb\n");
        return 0;
    }

    /* pstop – SIGSTOP the most recently started background process */
    if (strcmp(line, "pstop") == 0)
    {
        if (last_bg_pid == -1)
        {
            fprintf(stderr, "minibash: no background process to stop.\n");
        }
        else
        {
            /* TODO: send SIGSTOP, update last_stopped_pid */
            fprintf(stderr, "TODO: pstop pid=%d\n", (int)last_bg_pid);
            last_stopped_pid = last_bg_pid;
        }
        return 0;
    }

    /* cont – SIGCONT the most recently stopped process; wait for it */
    if (strcmp(line, "cont") == 0)
    {
        if (last_stopped_pid == -1)
        {
            fprintf(stderr, "minibash: no stopped process to continue.\n");
        }
        else
        {
            /* TODO: send SIGCONT, waitpid in foreground */
            fprintf(stderr, "TODO: cont pid=%d\n", (int)last_stopped_pid);
        }
        return 0;
    }

    /* numbg – print current background process count */
    if (strcmp(line, "numbg") == 0)
    {
        printf("Background processes: %d\n", bg_count);
        return 0;
    }

    /* killbp – kill all processes in current bash except bash & minibash */
    if (strcmp(line, "killbp") == 0)
    {
        /* TODO: iterate bg_pids[], send SIGKILL to each. */
        fprintf(stderr, "TODO: killbp\n");
        return 0;
    }

    return -1; /* not a builtin */
}

// 解析命令，识别命令类型并调用对应的执行函数
static CommandType resolve_command_type(char *line)
{
    // 解析命令，返回CommandType
    if (line[0] == '\0')
        return NONE;

    /* Builtin commands are handled first (no fork needed). */
    if (handle_builtin2(line) != -1)
        return BUILTIN;

    /* ';' sequential */
    if (strchr(line, ';') != NULL)
        return SEQUENTIAL;

    /* '&&' or '||' conditional  –  note: check AFTER '|||' would be wrong, but
     * '|||' does not contain '&&', so the order here is safe. */
    if (strstr(line, "&&") != NULL || strstr(line, "||") != NULL)
        return CONDITIONAL;

    /* '|||' FIFO  – must be before single '|' check */
    if (strstr(line, "|||") != NULL)
    {
        if (strncmp(line, "|||", 3) == 0)
            return FIFO_READ; /* |||cmd  */
        return FIFO_WRITE;    /* cmd|||  */
    }

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
        return BUILTIN; // or some other appropriate command type

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
                if (cmd_out->argc < MAX_ARGC)
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

// 内置命令
static int handle_builtin(CommandInfo *cmd_info)
{
    fprintf(stderr, "TODO handle_builtin: %s\n", cmd_info->argv[0]);
    return 0;
}

// 顺序执行
static int execute_sequential(CommandInfo *cmd_info)
{
    fprintf(stderr, "TODO execute_sequential: %s\n", cmd_info->argv[0]);
    return 0;
}

// 条件执行
static int execute_conditional(CommandInfo *cmd_info)
{
    fprintf(stderr, "TODO execute_conditional: %s\n", cmd_info->argv[0]);
    return 0;
}

// 普通管道
static int execute_pipe_chain(CommandInfo *cmd_info)
{
    fprintf(stderr, "TODO execute_pipe_chain: %s\n", cmd_info->argv[0]);
    return 0;
}

// 反向管道
static int execute_reverse_pipe_chain(CommandInfo *cmd_info)
{
    fprintf(stderr, "TODO execute_reverse_pipe_chain: %s\n", cmd_info->argv[0]);
    return 0;
}

// FIFO管道写入
static int execute_fifo_write(CommandInfo *cmd_info)
{
    fprintf(stderr, "TODO execute_fifo_write: %s\n", cmd_info->argv[0]);
    return 0;
}
// FIFO管道读取
static int execute_fifo_read(CommandInfo *cmd_info)
{
    fprintf(stderr, "TODO execute_fifo_read: %s\n", cmd_info->argv[0]);
    return 0;
}

// 重定向
static int execute_redirection(CommandInfo *cmd_info)
{
    fprintf(stderr, "TODO execute_redirection: %s\n", cmd_info->argv[0]);
    return 0;
}

// 文本双向追加 ++
static int execute_txt_append_both(CommandInfo *cmd_info)
{
    fprintf(stderr, "TODO execute_txt_append_both: %s\n", cmd_info->argv[0]);
    return 0;
}

// 单词计数 #
static int execute_word_count(CommandInfo *cmd_info)
{
    fprintf(stderr, "TODO execute_word_count: %s\n", cmd_info->argv[0]);
    return 0;
}

// 文本拼接 +
static int execute_txt_concat(CommandInfo *cmd_info)
{
    fprintf(stderr, "TODO execute_txt_concat: %s\n", cmd_info->argv[0]);
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
            execute_sequential(&cmd_out);
            break;
        case CONDITIONAL:
            execute_conditional(&cmd_out);
            break;
        case PIPE_CHAIN:
            execute_pipe_chain(&cmd_out);
            break;
        case REVERSE_PIPE_CHAIN:
            execute_reverse_pipe_chain(&cmd_out);
            break;
        case FIFO_WRITE:
            execute_fifo_write(&cmd_out);
            break;
        case FIFO_READ:
            execute_fifo_read(&cmd_out);
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
            execute_txt_concat(&cmd_out);
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