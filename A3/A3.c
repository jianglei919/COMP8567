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
#define MAX_TOKENS 128
#define MAX_CMDS 5

pid_t bg_pids[MAX_BG];
int bg_count;

pid_t last_bg_pid;
pid_t last_stopped_pid;

// 命令之间的连接类型
typedef enum
{
    OP_PIPE,
    OP_AND,
    OP_OR,
    OP_SEQ,
    OP_REVERSE_PIPE
} OperatorType;

// 命令节点的类型
typedef enum
{
    NODE_SIMPLE,     // 普通命令
    NODE_BG,         // 后台命令
    NODE_FIFO_W,     // FIFO 写端
    NODE_FIFO_R,     // FIFO 读端
    NODE_WORD_COUNT, // 统计单词数的特殊命令
    NODE_TXT_CAT,    // 文本连接命令（+）
    NODE_TXT_APP     // 文本追加命令（++）
} NodeType;

// 词法分析得到的 token 类型
typedef enum
{
    TOK_WORD,
    TOK_PIPE,
    TOK_DPIPE,
    TOK_TPIPE, /* | || ||| */

    TOK_AMP,
    TOK_DAMP, /* & && */

    TOK_SEMI,
    TOK_TILDE, /* ; ~ */

    TOK_PLUS,
    TOK_DPLUS, /* + ++ */

    TOK_HASH, /* # */

    TOK_REDIR_IN,
    TOK_REDIR_OUT,
    TOK_REDIR_APP, /* < > >> */

    TOK_EOF
} TokType;

// 词法分析得到的 token 结构
typedef struct
{
    TokType type;        // token 类型
    char text[MAX_LINE]; // token 文本内容，最长 MAX_LINE-1 字符 + 结尾 '\0'
} Token;

// 解析得到的命令结构
typedef struct Command
{
    char *argv[MAX_ARGC + 1]; // execvp 需要，最后一个必须是 NULL
    int argc;                 // 参数个数，含命令名，范围 1~4

    NodeType node_type; // 这个命令的类型，决定了如何执行

    char raw[1024]; // 这一段原始命令文本，便于调试/报错

    pid_t pid;        // 这个命令真正运行后对应的进程 id
    int exit_status;  // 前台执行完成后的退出码
    int has_executed; // 是否执行过

    int run_in_background; // 是否后台运行（主要给简单命令或最后一段使用）

    // 重定向信息
    char input_file[512];  // <
    char output_file[512]; // > 或 >>
    int redirect_in;       // 是否有输入重定向
    int redirect_out;      // 是否有输出重定向
    int append_out;        // 1 表示 >>，0 表示 >

} Command;

// 整条命令行的结构，包含多段命令和连接它们的操作符
typedef struct
{
    Command cmds[MAX_CMDS]; // 最多支持 MAX_CMDS 段命令
    int cmd_count;          // 实际的命令段数，范围 1~MAX_CMDS

    OperatorType ops[MAX_CMDS - 1]; // 连接命令的操作符，数量比命令段数少 1
    int op_count;                   // 实际的操作符数量，范围 0~MAX_CMDS-1
} CommandLine;

// 命令执行类型
typedef enum
{
    EXEC_SINGLE,       // 单命令，根据 node_type 分发
    EXEC_SEQUENCE,     // ; 顺序执行命令
    EXEC_CONDITIONAL,  // && 和 || 条件执行命令
    EXEC_PIPE,         // | 管道命令
    EXEC_REVERSE_PIPE, // ~ 反向管道命令
    EXEC_INVALID       // 不支持的混合操作符组合
} ExecType;

/* List of builtin command names, used only for type detection. */
static const char *BUILTIN_NAMES[] = {
    "killmb", "killallmb", "pstop", "cont", "numbg", "killbp", NULL};

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

static bool is_builtin(const char *name)
{
    int i;
    for (i = 0; BUILTIN_NAMES[i] != NULL; i++)
        if (strcmp(name, BUILTIN_NAMES[i]) == 0)
            return true;
    return false;
}

static bool push_token(Token *tokens, int max_tokens, int *count, TokType type, const char *text)
{
    if (*count >= max_tokens - 1)
        return false;

    tokens[*count].type = type;
    if (text == NULL)
        tokens[*count].text[0] = '\0';
    else
    {
        strncpy(tokens[*count].text, text, sizeof(tokens[*count].text) - 1);
        tokens[*count].text[sizeof(tokens[*count].text) - 1] = '\0';
    }
    (*count)++;
    return true;
}

static bool is_command_connector(TokType type)
{
    return type == TOK_PIPE || type == TOK_DAMP || type == TOK_DPIPE ||
           type == TOK_SEMI || type == TOK_TILDE || type == TOK_EOF;
}

static int append_command_arg(Command *cmd, const char *text)
{
    if (cmd->argc >= MAX_ARGC)
    {
        fprintf(stderr, "minibash: parser supports at most %d argv entries per command\n", MAX_ARGC);
        return -1;
    }

    cmd->argv[cmd->argc] = strdup(text);
    if (cmd->argv[cmd->argc] == NULL)
    {
        perror("strdup");
        return -1;
    }
    cmd->argc++;
    cmd->argv[cmd->argc] = NULL;
    return 0;
}

// 解析命令行字符串，构建 CommandLine 结构
int lex(const char *line, Token *tokens, int max_tokens)
{
    const char *p;
    int count = 0;

    if (line == NULL || tokens == NULL || max_tokens < 2)
        return -1;

    p = line;
    while (*p != '\0')
    {
        if (isspace((unsigned char)*p))
        {
            p++;
            continue;
        }

        if (p[0] == '|' && p[1] == '|' && p[2] == '|')
        {
            if (!push_token(tokens, max_tokens, &count, TOK_TPIPE, "|||"))
                return -1;
            p += 3;
        }
        else if (p[0] == '|' && p[1] == '|')
        {
            if (!push_token(tokens, max_tokens, &count, TOK_DPIPE, "||"))
                return -1;
            p += 2;
        }
        else if (p[0] == '&' && p[1] == '&')
        {
            if (!push_token(tokens, max_tokens, &count, TOK_DAMP, "&&"))
                return -1;
            p += 2;
        }
        else if (p[0] == '>' && p[1] == '>')
        {
            if (!push_token(tokens, max_tokens, &count, TOK_REDIR_APP, ">>"))
                return -1;
            p += 2;
        }
        else if (*p == '|')
        {
            if (!push_token(tokens, max_tokens, &count, TOK_PIPE, "|"))
                return -1;
            p++;
        }
        else if (*p == '&')
        {
            if (!push_token(tokens, max_tokens, &count, TOK_AMP, "&"))
                return -1;
            p++;
        }
        else if (*p == ';')
        {
            if (!push_token(tokens, max_tokens, &count, TOK_SEMI, ";"))
                return -1;
            p++;
        }
        else if (*p == '~')
        {
            if (!push_token(tokens, max_tokens, &count, TOK_TILDE, "~"))
                return -1;
            p++;
        }
        else if (p[0] == '+' && p[1] == '+')
        {
            if (!push_token(tokens, max_tokens, &count, TOK_DPLUS, "++"))
                return -1;
            p += 2;
        }
        else if (*p == '+')
        {
            if (!push_token(tokens, max_tokens, &count, TOK_PLUS, "+"))
                return -1;
            p++;
        }
        else if (*p == '#')
        {
            if (!push_token(tokens, max_tokens, &count, TOK_HASH, "#"))
                return -1;
            p++;
        }
        else if (*p == '<')
        {
            if (!push_token(tokens, max_tokens, &count, TOK_REDIR_IN, "<"))
                return -1;
            p++;
        }
        else if (*p == '>')
        {
            if (!push_token(tokens, max_tokens, &count, TOK_REDIR_OUT, ">"))
                return -1;
            p++;
        }
        else
        {
            char word[MAX_LINE];
            int word_len = 0;

            while (*p != '\0' && !isspace((unsigned char)*p) &&
                   *p != '|' && *p != '&' && *p != ';' && *p != '~' &&
                   *p != '+' && *p != '<' && *p != '>' && *p != '#')
            {
                if (word_len < (int)sizeof(word) - 1)
                    word[word_len++] = *p;
                p++;
            }
            word[word_len] = '\0';

            if (!push_token(tokens, max_tokens, &count, TOK_WORD, word))
                return -1;
        }
    }

    tokens[count].type = TOK_EOF;
    tokens[count].text[0] = '\0';
    return count;
}

// 解析命令连接符，构建 OperatorType
int parse_operator(const Token *tokens, int ntok, int *pos, OperatorType *out)
{
    if (tokens == NULL || pos == NULL || out == NULL || *pos >= ntok)
        return 0;

    switch (tokens[*pos].type)
    {
    case TOK_PIPE:
        *out = OP_PIPE;
        (*pos)++;
        return 1;
    case TOK_DAMP:
        *out = OP_AND;
        (*pos)++;
        return 1;
    case TOK_DPIPE:
        *out = OP_OR;
        (*pos)++;
        return 1;
    case TOK_SEMI:
        *out = OP_SEQ;
        (*pos)++;
        return 1;
    case TOK_TILDE:
        *out = OP_REVERSE_PIPE;
        (*pos)++;
        return 1;
    default:
        return 0;
    }
}

// 解析单个命令，构建 Command 结构
int parse_command(const Token *tokens, int ntok, int *pos, Command *out)
{
    if (tokens == NULL || pos == NULL || out == NULL || *pos >= ntok)
        return 0;

    memset(out, 0, sizeof(*out));
    out->node_type = NODE_SIMPLE;

    while (*pos < ntok && !is_command_connector(tokens[*pos].type))
    {
        const Token *tok = &tokens[*pos];

        switch (tok->type)
        {
        case TOK_WORD:
            if (append_command_arg(out, tok->text) < 0)
                return -1;
            (*pos)++;
            break;
        case TOK_HASH:
            out->node_type = NODE_WORD_COUNT;
            (*pos)++;
            break;
        case TOK_TPIPE:
            out->node_type = (out->argc == 0) ? NODE_FIFO_R : NODE_FIFO_W;
            (*pos)++;
            break;
        case TOK_AMP:
            out->node_type = NODE_BG;
            out->run_in_background = 1;
            (*pos)++;
            return out->argc > 0 ? 1 : -1;
        case TOK_PLUS:
            out->node_type = NODE_TXT_CAT;
            (*pos)++;
            break;
        case TOK_DPLUS:
            out->node_type = NODE_TXT_APP;
            (*pos)++;
            break;
        case TOK_REDIR_IN:
            (*pos)++;
            if (*pos >= ntok || tokens[*pos].type != TOK_WORD)
                return -1;
            strncpy(out->input_file, tokens[*pos].text, sizeof(out->input_file) - 1);
            out->input_file[sizeof(out->input_file) - 1] = '\0';
            out->redirect_in = 1;
            (*pos)++;
            break;
        case TOK_REDIR_OUT:
        case TOK_REDIR_APP:
            (*pos)++;
            if (*pos >= ntok || tokens[*pos].type != TOK_WORD)
                return -1;
            strncpy(out->output_file, tokens[*pos].text, sizeof(out->output_file) - 1);
            out->output_file[sizeof(out->output_file) - 1] = '\0';
            out->redirect_out = 1;
            out->append_out = (tok->type == TOK_REDIR_APP);
            (*pos)++;
            break;
        default:
            return -1;
        }
    }

    return out->argc > 0 ? 1 : -1;
}

// 解析命令行字符串，构建 CommandLine 结构
int parse_command_line(const Token *tokens, int ntok, CommandLine *out)
{
    int pos = 0;

    if (tokens == NULL || out == NULL)
        return -1;

    memset(out, 0, sizeof(*out));

    while (pos < ntok && tokens[pos].type != TOK_EOF)
    {
        if (out->cmd_count >= MAX_CMDS)
            return -1;

        if (parse_command(tokens, ntok, &pos, &out->cmds[out->cmd_count]) <= 0)
            return -1;
        out->cmd_count++;

        if (pos >= ntok || tokens[pos].type == TOK_EOF)
            break;

        if (out->op_count >= MAX_CMDS - 1)
            return -1;

        if (!parse_operator(tokens, ntok, &pos, &out->ops[out->op_count]))
            return -1;
        out->op_count++;
    }

    return out->cmd_count > 0 ? 0 : -1;
}

// 释放 Command 中分配的 argv 字符串
static void free_parsed_command(Command *cmd)
{
    int i;
    for (i = 0; i < cmd->argc; i++)
    {
        free(cmd->argv[i]);
        cmd->argv[i] = NULL;
    }
    cmd->argc = 0;
}

// 释放 CommandLine 中分配的 argv 字符串
static void free_parsed_command_line(CommandLine *cmdline)
{
    int i;

    if (cmdline == NULL)
        return;

    for (i = 0; i < cmdline->cmd_count; i++)
        free_parsed_command(&cmdline->cmds[i]);
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
static int run_cmd(const Command *cmd_info, bool foreground, int stdin_fd, int stdout_fd)
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
        /* Child process */
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

        /* Handle file redirections */
        if (cmd_info->redirect_in)
        {
            int fd = open(cmd_info->input_file, O_RDONLY);
            if (fd < 0)
            {
                perror(cmd_info->input_file);
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (cmd_info->redirect_out)
        {
            int flags = O_WRONLY | O_CREAT | (cmd_info->append_out ? O_APPEND : O_TRUNC);
            int fd = open(cmd_info->output_file, flags, 0644);
            if (fd < 0)
            {
                perror(cmd_info->output_file);
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(cmd_info->argv[0], cmd_info->argv);
        fprintf(stderr, "minibash: %s: %s\n", cmd_info->argv[0], strerror(errno));
        exit(123);
    }

    /* Parent process */
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

static int exc_builtin(const Command *cmd)
{
    const char *name = cmd->argv[0];

    if (strcmp(name, "killmb") == 0)
    {
        printf("minibash: exiting.\n");
        exit(0);
    }

    if (strcmp(name, "killallmb") == 0)
    {
        fprintf(stderr, "TODO: killallmb\n");
        return 0;
    }

    if (strcmp(name, "pstop") == 0)
    {
        if (last_bg_pid == -1)
            fprintf(stderr, "minibash: no background process to stop.\n");
        else if (kill(last_bg_pid, SIGSTOP) < 0)
            perror("pstop");
        else
        {
            last_stopped_pid = last_bg_pid;
            printf("[pstop] stopped pid %d\n", (int)last_bg_pid);
        }
        return 0;
    }

    if (strcmp(name, "cont") == 0)
    {
        if (last_stopped_pid == -1)
            fprintf(stderr, "minibash: no stopped process to continue.\n");
        else if (kill(last_stopped_pid, SIGCONT) < 0)
            perror("cont");
        else
        {
            int s;
            printf("[cont] continuing pid %d in foreground\n", (int)last_stopped_pid);
            waitpid(last_stopped_pid, &s, 0);
            last_stopped_pid = -1;
        }
        return 0;
    }

    if (strcmp(name, "numbg") == 0)
    {
        printf("Background processes: %d\n", bg_count);
        return 0;
    }

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

    return -1;
}

// 执行单个命令，处理内置命令和外部命令
static int exc_single_cmd(const Command *cmd)
{
    if (cmd->argc <= 0)
        return -1;

    if (is_builtin(cmd->argv[0]))
        return exc_builtin(cmd);

    bool foreground = !cmd->run_in_background;
    return run_cmd(cmd, foreground, -1, -1);
}

static int exc_sequence_cmd(CommandLine *cmdline)
{
    (void)cmdline;
    fprintf(stderr, "TODO: exc_sequence_cmd\n");
    return -1;
}

static int exc_conditional_cmd(CommandLine *cmdline)
{
    (void)cmdline;
    fprintf(stderr, "TODO: exc_conditional_cmd\n");
    return -1;
}

static int exc_pipe_cmd(CommandLine *cmdline)
{
    (void)cmdline;
    fprintf(stderr, "TODO: exc_pipe_cmd\n");
    return -1;
}

static int exc_reverse_pipe_cmd(CommandLine *cmdline)
{
    (void)cmdline;
    fprintf(stderr, "TODO: exc_reverse_pipe_cmd\n");
    return -1;
}

static int exc_fifo_write_cmd(const Command *cmd)
{
    (void)cmd;
    fprintf(stderr, "TODO: exc_fifo_write_cmd\n");
    return -1;
}

static int exc_fifo_read_cmd(const Command *cmd)
{
    (void)cmd;
    fprintf(stderr, "TODO: exc_fifo_read_cmd\n");
    return -1;
}

static int exc_word_count_cmd(const Command *cmd)
{
    (void)cmd;
    fprintf(stderr, "TODO: exc_word_count_cmd\n");
    return -1;
}

static int exc_txt_cat_cmd(const Command *cmd)
{
    (void)cmd;
    fprintf(stderr, "TODO: exc_txt_cat_cmd\n");
    return -1;
}

static int exc_txt_app_cmd(const Command *cmd)
{
    (void)cmd;
    fprintf(stderr, "TODO: exc_txt_app_cmd\n");
    return -1;
}

static int exc_parsed(CommandLine *cmdline)
{
    int i;
    ExecType dispatch = EXEC_SINGLE;

    if (cmdline == NULL || cmdline->cmd_count == 0)
        return -1;

    // 单命令直接根据 node_type 分发
    if (cmdline->cmd_count == 1)
    {
        switch (cmdline->cmds[0].node_type)
        {
        case NODE_SIMPLE:
        case NODE_BG:
            return exc_single_cmd(&cmdline->cmds[0]);
        case NODE_FIFO_W:
            return exc_fifo_write_cmd(&cmdline->cmds[0]);
        case NODE_FIFO_R:
            return exc_fifo_read_cmd(&cmdline->cmds[0]);
        case NODE_WORD_COUNT:
            return exc_word_count_cmd(&cmdline->cmds[0]);
        case NODE_TXT_CAT:
            return exc_txt_cat_cmd(&cmdline->cmds[0]);
        case NODE_TXT_APP:
            return exc_txt_app_cmd(&cmdline->cmds[0]);
        default:
            fprintf(stderr, "minibash: unknown single command node type\n");
            return -1;
        }
    }

    // 多命令需要根据连接符类型分发
    for (i = 0; i < cmdline->op_count; i++)
    {
        switch (cmdline->ops[i])
        {
        case OP_SEQ:
            dispatch = EXEC_SEQUENCE;
            break;
        case OP_AND:
        case OP_OR:
            if (dispatch != EXEC_SINGLE && dispatch != EXEC_CONDITIONAL)
                return -1;
            dispatch = EXEC_CONDITIONAL;
            break;
        case OP_PIPE:
            if (dispatch != EXEC_SINGLE && dispatch != EXEC_PIPE)
                return -1;
            dispatch = EXEC_PIPE;
            break;
        case OP_REVERSE_PIPE:
            if (dispatch != EXEC_SINGLE && dispatch != EXEC_REVERSE_PIPE)
                return -1;
            dispatch = EXEC_REVERSE_PIPE;
            break;
        default:
            dispatch = EXEC_INVALID;
            break;
        }

        if (dispatch == EXEC_SEQUENCE)
            break;
        if (dispatch == EXEC_INVALID)
            break;
    }

    switch (dispatch)
    {
    case EXEC_SEQUENCE:
        return exc_sequence_cmd(cmdline);
    case EXEC_CONDITIONAL:
        return exc_conditional_cmd(cmdline);
    case EXEC_PIPE:
        return exc_pipe_cmd(cmdline);
    case EXEC_REVERSE_PIPE:
        return exc_reverse_pipe_cmd(cmdline);
    case EXEC_SINGLE:
        return exc_single_cmd(&cmdline->cmds[0]);
    case EXEC_INVALID:
    default:
        fprintf(stderr, "minibash: unsupported mixed operators in one command line\n");
        return -1;
    }
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

        Token tokens[MAX_TOKENS];
        CommandLine parsed;

        // 词法分析和语法分析
        int ntok = lex(line, tokens, MAX_TOKENS);

        if (ntok < 0)
        {
            fprintf(stderr, "minibash: lex failed\n");
            continue;
        }

        if (parse_command_line(tokens, ntok, &parsed) < 0)
        {
            fprintf(stderr, "minibash: parse failed\n");
            continue;
        }

        // 这里根据 parsed 中的命令类型调用不同的执行函数
        exc_parsed(&parsed);

        // 释放解析结果中分配的内存
        free_parsed_command_line(&parsed);
    }

    return 0;
}