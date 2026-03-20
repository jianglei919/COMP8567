#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
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

#define FIFO_DIR_L1 "Assignments"
#define FIFO_DIR_L2 "Assignment3"
#define FIFO_FILE_NAME "minibash_fifo"

pid_t bg_pids[MAX_BG];
int bg_count;

pid_t last_bg_pid = -1;
pid_t last_stopped_pid = -1;

// 命令之间的连接类型
typedef enum OperatorType
{
    OP_PIPE,
    OP_AND,
    OP_OR,
    OP_SEQ,
    OP_REVERSE_PIPE
} OperatorType;

// 命令节点的类型
typedef enum NodeType
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
typedef enum TokType
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
typedef struct Token
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
typedef struct CommandLine
{
    Command cmds[MAX_CMDS]; // 最多支持 MAX_CMDS 段命令
    int cmd_count;          // 实际的命令段数，范围 1~MAX_CMDS

    OperatorType ops[MAX_CMDS - 1]; // 连接命令的操作符，数量比命令段数少 1
    int op_count;                   // 实际的操作符数量，范围 0~MAX_CMDS-1
} CommandLine;

// 命令执行类型
typedef enum ExecType
{
    EXEC_SINGLE,       // 单命令，根据 node_type 分发
    EXEC_SEQUENCE,     // ; 顺序执行命令
    EXEC_CONDITIONAL,  // && 和 || 条件执行命令
    EXEC_PIPE,         // | 管道命令
    EXEC_REVERSE_PIPE, // ~ 反向管道命令
    EXEC_INVALID       // 不支持的混合操作符组合
} ExecType;

// 解析错误码
typedef enum ParseErrorCode
{
    PARSE_ERR_INVALID_INPUT = -1,
    PARSE_ERR_TOO_MANY_TOKENS = -2,
    PARSE_ERR_TOKEN_TOO_LONG = -3,
    PARSE_ERR_ARG_TOO_LONG = -4,
    PARSE_ERR_TOO_MANY_ARGS = -5,
    PARSE_ERR_INPUT_FILE_TOO_LONG = -6,
    PARSE_ERR_OUTPUT_FILE_TOO_LONG = -7,
    PARSE_ERR_TOO_MANY_COMMANDS = -8,
    PARSE_ERR_TOO_MANY_OPERATORS = -9,
    PARSE_ERR_EXPECTED_OPERATOR = -10,
    PARSE_ERR_EXPECTED_COMMAND = -11,
    PARSE_ERR_INPUT_LINE_TOO_LONG = -12
} ParseErrorCode;

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

// 丢弃输入行剩余的内容，直到遇到换行符或 EOF
static void discard_rest_of_line(void)
{
    int ch;
    do
    {
        ch = getchar();
    } while (ch != '\n' && ch != EOF);
}

static void report_parse_error(int rc)
{
    switch (rc)
    {
    case PARSE_ERR_INVALID_INPUT:
        fprintf(stderr, "minibash: invalid parser input\n");
        break;
    case PARSE_ERR_TOO_MANY_TOKENS:
        fprintf(stderr, "minibash: too many tokens (max %d)\n", MAX_TOKENS - 1);
        break;
    case PARSE_ERR_TOKEN_TOO_LONG:
        fprintf(stderr, "minibash: token too long\n");
        break;
    case PARSE_ERR_ARG_TOO_LONG:
        fprintf(stderr, "minibash: argument too long\n");
        break;
    case PARSE_ERR_TOO_MANY_ARGS:
        fprintf(stderr, "minibash: too many arguments (max %d)\n", MAX_ARGC);
        break;
    case PARSE_ERR_INPUT_FILE_TOO_LONG:
        fprintf(stderr, "minibash: input filename too long\n");
        break;
    case PARSE_ERR_OUTPUT_FILE_TOO_LONG:
        fprintf(stderr, "minibash: output filename too long\n");
        break;
    case PARSE_ERR_TOO_MANY_COMMANDS:
        fprintf(stderr, "minibash: too many command segments (max %d)\n", MAX_CMDS);
        break;
    case PARSE_ERR_TOO_MANY_OPERATORS:
        fprintf(stderr, "minibash: too many operators (max %d)\n", MAX_CMDS - 1);
        break;
    case PARSE_ERR_EXPECTED_OPERATOR:
        fprintf(stderr, "minibash: expected command operator\n");
        break;
    case PARSE_ERR_EXPECTED_COMMAND:
        fprintf(stderr, "minibash: expected command content\n");
        break;
    case PARSE_ERR_INPUT_LINE_TOO_LONG:
        fprintf(stderr, "minibash: input line too long (max %d chars)\n", MAX_LINE - 1);
        break;
    default:
        fprintf(stderr, "minibash: parse failed\n");
        break;
    }
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

/* Check whether a pid is still tracked in background list. */
static bool bg_has_pid(pid_t pid)
{
    int i;
    for (i = 0; i < bg_count; i++)
    {
        if (bg_pids[i] == pid)
            return true;
    }
    return false;
}

/* Remove an entry from bg_pids by index. */
static void bg_remove_at(int idx)
{
    int i;
    pid_t removed;

    if (idx < 0 || idx >= bg_count)
        return;

    removed = bg_pids[idx];

    for (i = idx; i < bg_count - 1; i++)
        bg_pids[i] = bg_pids[i + 1];

    bg_count--;

    if (removed == last_bg_pid)
        last_bg_pid = (bg_count > 0) ? bg_pids[bg_count - 1] : -1;
    if (removed == last_stopped_pid)
        last_stopped_pid = -1;
}

/* Reap finished background children and keep bg_count accurate. */
static void bg_reap(void)
{
    int i = 0;

    while (i < bg_count)
    {
        pid_t pid = bg_pids[i];
        int status;
        pid_t rc = waitpid(pid, &status, WNOHANG);

        if (rc == 0)
        {
            i++;
            continue;
        }

        if (rc < 0 && errno != ECHILD)
            perror("waitpid");

        bg_remove_at(i);
    }
}

// 检查命令名是否是内置命令
static bool is_builtin(const char *name)
{
    int i;
    for (i = 0; BUILTIN_NAMES[i] != NULL; i++)
        if (strcmp(name, BUILTIN_NAMES[i]) == 0)
            return true;
    return false;
}

// 把一个 token 压入 token 数组，返回 0 成功，负数表示错误
static int push_token(Token *tokens, int max_tokens, int *count, TokType type, const char *text)
{
    if (*count >= max_tokens - 1)
        return PARSE_ERR_TOO_MANY_TOKENS;

    if (text != NULL && strlen(text) >= sizeof(tokens[*count].text))
        return PARSE_ERR_TOKEN_TOO_LONG;

    tokens[*count].type = type;
    if (text == NULL)
        tokens[*count].text[0] = '\0';
    else
    {
        strncpy(tokens[*count].text, text, sizeof(tokens[*count].text) - 1);
        tokens[*count].text[sizeof(tokens[*count].text) - 1] = '\0';
    }
    (*count)++;
    return 0;
}

// 判断 token 类型是否是命令连接符（|、||、|||、&&、;、~ 或 EOF）
static bool is_command_connector(TokType type)
{
    return type == TOK_PIPE || type == TOK_DAMP || type == TOK_DPIPE ||
           type == TOK_SEMI || type == TOK_TILDE || type == TOK_EOF;
}

// 追加一个参数到 Command 结构的 argv 中，返回 0 成功，负数表示错误
static int append_command_arg(Command *cmd, const char *text)
{
    if (cmd->argc >= MAX_ARGC)
        return PARSE_ERR_TOO_MANY_ARGS;

    cmd->argv[cmd->argc] = strdup(text);
    if (cmd->argv[cmd->argc] == NULL)
    {
        perror("strdup");
        return PARSE_ERR_INVALID_INPUT;
    }
    cmd->argc++;
    cmd->argv[cmd->argc] = NULL;
    return 0;
}

// 校验路径是否是 .txt 文件
static bool is_txt_file_path(const char *path)
{
    size_t len;

    if (path == NULL)
        return false;

    len = strlen(path);
    return len >= 4 && strcmp(path + len - 4, ".txt") == 0;
}

// 读取整个文件到内存缓冲区（由调用方 free）
static int read_file_all(const char *path, char **out_buf, size_t *out_len)
{
    FILE *fp;
    char chunk[4096];
    char *buf = NULL;
    size_t len = 0;
    size_t cap = 0;

    if (path == NULL || out_buf == NULL || out_len == NULL)
        return -1;

    fp = fopen(path, "r");
    if (fp == NULL)
    {
        perror(path);
        return -1;
    }

    while (1)
    {
        size_t n = fread(chunk, 1, sizeof(chunk), fp);

        if (n > 0)
        {
            if (len + n > cap)
            {
                size_t new_cap = (cap == 0) ? 4096 : cap;
                char *tmp;
                while (new_cap < len + n)
                    new_cap *= 2;

                tmp = realloc(buf, new_cap);
                if (tmp == NULL)
                {
                    perror("realloc");
                    fclose(fp);
                    free(buf);
                    return -1;
                }
                buf = tmp;
                cap = new_cap;
            }

            memcpy(buf + len, chunk, n);
            len += n;
        }

        if (n < sizeof(chunk))
        {
            if (ferror(fp))
            {
                perror(path);
                fclose(fp);
                free(buf);
                return -1;
            }
            break;
        }
    }

    fclose(fp);
    *out_buf = buf;
    *out_len = len;
    return 0;
}

// 追加内存缓冲区到目标文件末尾
static int append_buffer_to_file(const char *path, const char *buf, size_t len)
{
    FILE *fp = fopen(path, "a");
    if (fp == NULL)
    {
        perror(path);
        return -1;
    }

    if (len > 0 && fwrite(buf, 1, len, fp) != len)
    {
        perror(path);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

// 构建并确保公共 FIFO 存在，成功时返回 0，并写出 fifo_path。
static int ensure_common_fifo(char *fifo_path, size_t fifo_path_sz)
{
    const char *home;
    char dir1[PATH_MAX];
    char dir2[PATH_MAX];

    if (fifo_path == NULL || fifo_path_sz == 0)
        return -1;

    home = getenv("HOME");
    if (home == NULL || home[0] == '\0')
    {
        fprintf(stderr, "minibash: HOME is not set\n");
        return -1;
    }

    if (snprintf(dir1, sizeof(dir1), "%s/%s", home, FIFO_DIR_L1) >= (int)sizeof(dir1) ||
        snprintf(dir2, sizeof(dir2), "%s/%s/%s", home, FIFO_DIR_L1, FIFO_DIR_L2) >= (int)sizeof(dir2) ||
        snprintf(fifo_path, fifo_path_sz, "%s/%s/%s/%s", home, FIFO_DIR_L1, FIFO_DIR_L2, FIFO_FILE_NAME) >= (int)fifo_path_sz)
    {
        fprintf(stderr, "minibash: fifo path too long\n");
        return -1;
    }

    // dir1 = ~/Assignments
    if (mkdir(dir1, 0755) < 0 && errno != EEXIST)
    {
        perror(dir1);
        return -1;
    }

    // dir2 = ~/Assignments/Assignment3
    if (mkdir(dir2, 0755) < 0 && errno != EEXIST)
    {
        perror(dir2);
        return -1;
    }

    // fifo_path = ~/Assignments/Assignment3/minibash_fifo
    if (mkfifo(fifo_path, 0666) < 0 && errno != EEXIST)
    {
        perror(fifo_path);
        return -1;
    }

    return 0;
}

// 解析命令行字符串，构建 CommandLine 结构
int do_lex(const char *line, Token *tokens, int max_tokens)
{
    const char *p;
    int count = 0;
    int rc;

    if (line == NULL || tokens == NULL || max_tokens < 2)
        return PARSE_ERR_INVALID_INPUT;

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
            rc = push_token(tokens, max_tokens, &count, TOK_TPIPE, "|||");
            if (rc < 0)
                return rc;
            p += 3;
        }
        else if (p[0] == '|' && p[1] == '|')
        {
            rc = push_token(tokens, max_tokens, &count, TOK_DPIPE, "||");
            if (rc < 0)
                return rc;
            p += 2;
        }
        else if (p[0] == '&' && p[1] == '&')
        {
            rc = push_token(tokens, max_tokens, &count, TOK_DAMP, "&&");
            if (rc < 0)
                return rc;
            p += 2;
        }
        else if (p[0] == '>' && p[1] == '>')
        {
            rc = push_token(tokens, max_tokens, &count, TOK_REDIR_APP, ">>");
            if (rc < 0)
                return rc;
            p += 2;
        }
        else if (*p == '|')
        {
            rc = push_token(tokens, max_tokens, &count, TOK_PIPE, "|");
            if (rc < 0)
                return rc;
            p++;
        }
        else if (*p == '&')
        {
            rc = push_token(tokens, max_tokens, &count, TOK_AMP, "&");
            if (rc < 0)
                return rc;
            p++;
        }
        else if (*p == ';')
        {
            rc = push_token(tokens, max_tokens, &count, TOK_SEMI, ";");
            if (rc < 0)
                return rc;
            p++;
        }
        else if (*p == '~')
        {
            rc = push_token(tokens, max_tokens, &count, TOK_TILDE, "~");
            if (rc < 0)
                return rc;
            p++;
        }
        else if (p[0] == '+' && p[1] == '+')
        {
            rc = push_token(tokens, max_tokens, &count, TOK_DPLUS, "++");
            if (rc < 0)
                return rc;
            p += 2;
        }
        else if (*p == '+')
        {
            rc = push_token(tokens, max_tokens, &count, TOK_PLUS, "+");
            if (rc < 0)
                return rc;
            p++;
        }
        else if (*p == '#')
        {
            rc = push_token(tokens, max_tokens, &count, TOK_HASH, "#");
            if (rc < 0)
                return rc;
            p++;
        }
        else if (*p == '<')
        {
            rc = push_token(tokens, max_tokens, &count, TOK_REDIR_IN, "<");
            if (rc < 0)
                return rc;
            p++;
        }
        else if (*p == '>')
        {
            rc = push_token(tokens, max_tokens, &count, TOK_REDIR_OUT, ">");
            if (rc < 0)
                return rc;
            p++;
        }
        else
        {
            char word[MAX_LINE];
            int word_len = 0;
            bool overflow = false;

            while (*p != '\0' && !isspace((unsigned char)*p) &&
                   *p != '|' && *p != '&' && *p != ';' && *p != '~' &&
                   *p != '+' && *p != '<' && *p != '>' && *p != '#')
            {
                if (word_len < (int)sizeof(word) - 1)
                    word[word_len++] = *p;
                else
                    overflow = true;
                p++;
            }
            word[word_len] = '\0';

            if (overflow)
                return PARSE_ERR_ARG_TOO_LONG;

            rc = push_token(tokens, max_tokens, &count, TOK_WORD, word);
            if (rc < 0)
                return rc;
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
        return PARSE_ERR_INVALID_INPUT;

    memset(out, 0, sizeof(*out));
    out->node_type = NODE_SIMPLE;

    while (*pos < ntok && !is_command_connector(tokens[*pos].type))
    {
        const Token *tok = &tokens[*pos];

        switch (tok->type)
        {
        case TOK_WORD:
        {
            int rc = append_command_arg(out, tok->text);
            if (rc < 0)
                return rc;
            (*pos)++;
            break;
        }
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
                return PARSE_ERR_EXPECTED_COMMAND;
            if (strlen(tokens[*pos].text) >= sizeof(out->input_file))
                return PARSE_ERR_INPUT_FILE_TOO_LONG;
            strncpy(out->input_file, tokens[*pos].text, sizeof(out->input_file) - 1);
            out->input_file[sizeof(out->input_file) - 1] = '\0';
            out->redirect_in = 1;
            (*pos)++;
            break;
        case TOK_REDIR_OUT:
        case TOK_REDIR_APP:
            (*pos)++;
            if (*pos >= ntok || tokens[*pos].type != TOK_WORD)
                return PARSE_ERR_EXPECTED_COMMAND;
            if (strlen(tokens[*pos].text) >= sizeof(out->output_file))
                return PARSE_ERR_OUTPUT_FILE_TOO_LONG;
            strncpy(out->output_file, tokens[*pos].text, sizeof(out->output_file) - 1);
            out->output_file[sizeof(out->output_file) - 1] = '\0';
            out->redirect_out = 1;
            out->append_out = (tok->type == TOK_REDIR_APP);
            (*pos)++;
            break;
        default:
            return PARSE_ERR_EXPECTED_COMMAND;
        }
    }

    return out->argc > 0 ? 1 : PARSE_ERR_EXPECTED_COMMAND;
}

// 解析命令行字符串，构建 CommandLine 结构
int parse_command_line(const Token *tokens, int ntok, CommandLine *out)
{
    int pos = 0;
    int rc;

    if (tokens == NULL || out == NULL)
        return PARSE_ERR_INVALID_INPUT;

    memset(out, 0, sizeof(*out));

    while (pos < ntok && tokens[pos].type != TOK_EOF)
    {
        if (out->cmd_count >= MAX_CMDS)
            return PARSE_ERR_TOO_MANY_COMMANDS;

        rc = parse_command(tokens, ntok, &pos, &out->cmds[out->cmd_count]);
        if (rc <= 0)
            return (rc < 0) ? rc : PARSE_ERR_EXPECTED_COMMAND;
        out->cmd_count++;

        if (pos >= ntok || tokens[pos].type == TOK_EOF)
            break;

        if (out->op_count >= MAX_CMDS - 1)
            return PARSE_ERR_TOO_MANY_OPERATORS;

        if (!parse_operator(tokens, ntok, &pos, &out->ops[out->op_count]))
            return PARSE_ERR_EXPECTED_OPERATOR;
        out->op_count++;
    }

    return out->cmd_count > 0 ? 0 : PARSE_ERR_EXPECTED_COMMAND;
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

/*
 * run_pipe – 统一处理正向管道(|)和反向管道(~)
 *
 * 参数说明：
 * - expected_op: 期望的连接符类型（OP_PIPE 或 OP_REVERSE_PIPE）
 * - where:       用于报错信息，标识调用方
 * - reverse:     false=按 cmd0->cmdN-1 执行；true=按 cmdN-1->cmd0 执行
 *
 * 整体策略：
 * 1. 预创建 N-1 个 pipe。
 * 2. fork N 个子进程，每个子进程按 stage(i) 接好 stdin/stdout。
 * 3. 子进程关闭所有原始 pipe fd，避免泄漏和 EOF 卡住。
 * 4. 子进程再处理该命令自己的 < / > / >> 重定向（优先级覆盖管道接线）。
 * 5. 父进程关闭全部 pipe fd，并 wait 所有子进程。
 * 6. 返回最后一个 stage 的退出码。
 */
static int run_pipe(CommandLine *cmdline, OperatorType expected_op, const char *where, bool reverse)
{
    int i, j;
    int cmd_count;
    int pipes[MAX_CMDS - 1][2];
    pid_t pids[MAX_CMDS];
    int last_status = 0;

    if (cmdline == NULL || cmdline->cmd_count < 2)
        return -1;

    cmd_count = cmdline->cmd_count;

    for (i = 0; i < cmdline->op_count; i++)
    {
        if (cmdline->ops[i] != expected_op)
        {
            fprintf(stderr, "minibash: %s: unexpected operator\n", where);
            return -1;
        }
    }

    for (i = 0; i < cmd_count - 1; i++)
    {
        if (pipe(pipes[i]) < 0)
        {
            perror("pipe");
            for (j = 0; j < i; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return -1;
        }
    }

    // 这里的 i 表示“执行链中的 stage 序号”，不是原始 cmd 下标。
    // reverse=false: cmd_idx=i
    // reverse=true : cmd_idx=cmd_count-1-i
    for (i = 0; i < cmd_count; i++)
    {
        int cmd_idx = reverse ? (cmd_count - 1 - i) : i;
        const Command *cmd = &cmdline->cmds[cmd_idx];
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
            for (j = 0; j < cmd_count - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            for (j = 0; j < i; j++)
            {
                int s;
                waitpid(pids[j], &s, 0);
            }
            return -1;
        }

        if (pid == 0)
        {
            // stage i 的输入来自上一根管道读端，输出去下一根管道写端。
            // 不论正向/反向，这个接线规则都保持一致；
            // 差异只在于 cmd_idx 映射到哪个命令。
            if (i > 0)
                dup2(pipes[i - 1][0], STDIN_FILENO);
            if (i < cmd_count - 1)
                dup2(pipes[i][1], STDOUT_FILENO);

            // 关闭子进程中所有原始管道 fd：
            // 否则可能导致写端未真正关闭，读端收不到 EOF。
            for (j = 0; j < cmd_count - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // 文件重定向优先级高于管道接线：
            // 例如 cmd < in.txt | ...，该 cmd 的 stdin 最终来自文件。
            if (cmd->redirect_in)
            {
                int fd = open(cmd->input_file, O_RDONLY);
                if (fd < 0)
                {
                    perror(cmd->input_file);
                    exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            if (cmd->redirect_out)
            {
                int flags = O_WRONLY | O_CREAT | (cmd->append_out ? O_APPEND : O_TRUNC);
                int fd = open(cmd->output_file, flags, 0644);
                if (fd < 0)
                {
                    perror(cmd->output_file);
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            execvp(cmd->argv[0], cmd->argv);
            fprintf(stderr, "minibash: %s: %s\n", cmd->argv[0], strerror(errno));
            exit(123);
        }

        pids[i] = pid;
    }

    // 父进程不参与读写，必须关闭全部 pipe fd，避免子进程读端阻塞等 EOF。
    for (i = 0; i < cmd_count - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // 等待所有 stage；按当前实现返回“最后一个 stage(i=cmd_count-1)”的退出码。
    for (i = 0; i < cmd_count; i++)
    {
        int status;
        waitpid(pids[i], &status, 0);
        if (i == cmd_count - 1)
            last_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    return last_status;
}

static int exc_builtin_cmd(const Command *cmd);
static int exc_single_cmd(const Command *cmd);
static int exc_fifo_write_cmd(const Command *cmd);
static int exc_fifo_read_cmd(const Command *cmd);
static int exc_word_count_cmd(const Command *cmd);
static int exc_txt_cat_cmd(const Command *cmd);
static int exc_txt_app_cmd(const Command *cmd);

// 给定 pid，判断是否和当前进程运行的是同一个可执行文件
static bool is_same_executable_as_self(pid_t pid)
{
    char self_exe[PATH_MAX];
    char other_exe[PATH_MAX];
    char proc_link[64];
    ssize_t n1, n2;

    n1 = readlink("/proc/self/exe", self_exe, sizeof(self_exe) - 1);
    if (n1 < 0)
        return false;
    self_exe[n1] = '\0';

    if (snprintf(proc_link, sizeof(proc_link), "/proc/%d/exe", (int)pid) >= (int)sizeof(proc_link))
        return false;

    n2 = readlink(proc_link, other_exe, sizeof(other_exe) - 1);
    if (n2 < 0)
        return false;
    other_exe[n2] = '\0';

    return strcmp(self_exe, other_exe) == 0;
}

// 判断目标 pid 是否运行在“不同终端”上
static bool is_on_different_terminal_from_self(pid_t pid)
{
    char self_tty[PATH_MAX];
    char other_tty[PATH_MAX];
    char fd0_link[64];
    ssize_t n;

    // 当前进程终端（例如 /dev/pts/3）
    if (ttyname_r(STDIN_FILENO, self_tty, sizeof(self_tty)) != 0)
        return false;

    if (snprintf(fd0_link, sizeof(fd0_link), "/proc/%d/fd/0", (int)pid) >= (int)sizeof(fd0_link))
        return false;

    // 读取目标进程 stdin 指向的终端设备
    n = readlink(fd0_link, other_tty, sizeof(other_tty) - 1);
    if (n < 0)
        return false;
    other_tty[n] = '\0';

    return strcmp(self_tty, other_tty) != 0;
}

// 向其它 minibash 实例发送信号（不包含当前进程），返回成功发送的数量
static int signal_other_minibash_instances(int sig)
{
    DIR *dir;
    struct dirent *ent;
    pid_t self = getpid();
    int count = 0;

    dir = opendir("/proc");
    if (dir == NULL)
    {
        perror("/proc");
        return -1;
    }

    while ((ent = readdir(dir)) != NULL)
    {
        char *endptr;
        long lpid;
        pid_t pid;

        if (!isdigit((unsigned char)ent->d_name[0]))
            continue;

        lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0' || lpid <= 0)
            continue;

        pid = (pid_t)lpid;
        if (pid == self)
            continue;

        if (!is_same_executable_as_self(pid))
            continue;

        // killallmb 作用于“不同终端”上的 minibash。
        if (!is_on_different_terminal_from_self(pid))
            continue;

        if (kill(pid, sig) == 0)
            count++;
    }

    closedir(dir);
    return count;
}

// 根据命令节点类型(NodeType)分发到对应的执行函数
static int do_dispatch_cmd(const Command *cmd, const char *where)
{
    switch (cmd->node_type)
    {
    case NODE_SIMPLE:
    case NODE_BG:
        return exc_single_cmd(cmd);
    case NODE_FIFO_W:
        return exc_fifo_write_cmd(cmd);
    case NODE_FIFO_R:
        return exc_fifo_read_cmd(cmd);
    case NODE_WORD_COUNT:
        return exc_word_count_cmd(cmd);
    case NODE_TXT_CAT:
        return exc_txt_cat_cmd(cmd);
    case NODE_TXT_APP:
        return exc_txt_app_cmd(cmd);
    default:
        fprintf(stderr, "minibash: unknown command node type in %s\n", where);
        return -1;
    }
}

// 执行内置命令，返回 0 表示成功执行了内置命令，负数表示不是内置命令或执行失败
static int exc_builtin_cmd(const Command *cmd)
{
    const char *name = cmd->argv[0];

    if (strcmp(name, "killmb") == 0)
    {
        printf("minibash: exiting.\n");
        // 结束当前 minibash 终端会话。
        exit(0);
    }

    if (strcmp(name, "killallmb") == 0)
    {
        int n = signal_other_minibash_instances(SIGTERM);
        if (n < 0)
            return -1;

        printf("[killallmb] signaled %d minibash instance(s).\n", n);
        return 0;
    }

    if (strcmp(name, "pstop") == 0)
    {
        if (last_bg_pid == -1 || !bg_has_pid(last_bg_pid))
            fprintf(stderr, "minibash: no background process to stop.\n");
        else if (kill(last_bg_pid, SIGSTOP) < 0)
            fprintf(stderr, "minibash: pstop: %s\n", strerror(errno));
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
            fprintf(stderr, "minibash: cont: %s\n", strerror(errno));
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
                fprintf(stderr, "minibash: killbp: %s\n", strerror(errno));
            else
                printf("[killbp] killed pid %d\n", (int)bg_pids[i]);
        }
        bg_count = 0;
        last_bg_pid = -1;
        last_stopped_pid = -1;
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
        return exc_builtin_cmd(cmd);

    bool foreground = !cmd->run_in_background;
    return run_cmd(cmd, foreground, -1, -1);
}

/**
 * 执行命令序列，按照顺序依次执行每个命令，不管前一个命令的结果如何。
 * 例如：`cmd1 ; cmd2 ; cmd3` 会先执行 cmd1 -> cmd2 -> cmd3。
 * 1. 校验 cmdline 合法性
 * 2. 校验这条命令链确实是纯 ;（OP_SEQ）序列
 * 3. 按顺序遍历每个 Command
 * 4. 用 switch (cmd->node_type) 分发到对应 exc_* 函数
 * 5. 返回最后一个命令的退出码
 */
static int exc_sequence_cmd(CommandLine *cmdline)
{
    int i;
    int last_rc = 0; // 最后一个命令的返回码

    if (cmdline == NULL || cmdline->cmd_count <= 0)
        return -1;

    if (cmdline->op_count != cmdline->cmd_count - 1)
        return -1;

    for (i = 0; i < cmdline->op_count; i++)
    {
        if (cmdline->ops[i] != OP_SEQ)
        {
            fprintf(stderr, "minibash: sequence only supports ';' operators\n");
            return -1;
        }
    }

    for (i = 0; i < cmdline->cmd_count; i++)
    {
        const Command *cmd = &cmdline->cmds[i];

        last_rc = do_dispatch_cmd(cmd, "sequence");
        if (last_rc < 0)
            return last_rc;
    }

    return last_rc;
}

/**
 * 执行条件命令，按照左到右的顺序执行每个命令，根据连接符和前一个命令的结果决定是否执行下一个命令。
 * 操作符必须全是 OP_AND 或 OP_OR，不能混合其他类型的操作符。
 * 例如：`cmd1 && cmd2 || cmd3` 会先执行 cmd1，如果 cmd1 成功（返回码 0）则执行 cmd2，否则执行 cmd3。
 * 1. 第一条命令无条件执行
 * 2. 后续按短路规则从左到右执行
 * 3. &&: 上一条成功(last_rc == 0)才执行下一条
 * 4. ||: 上一条失败(last_rc != 0)才执行下一条
 */
static int exc_conditional_cmd(CommandLine *cmdline)
{
    int i;
    int last_rc = 0;

    // 基本合法性检查
    if (cmdline == NULL || cmdline->cmd_count <= 0)
        return -1;

    if (cmdline->op_count != cmdline->cmd_count - 1)
        return -1;

    for (i = 0; i < cmdline->op_count; i++)
    {
        if (cmdline->ops[i] != OP_AND && cmdline->ops[i] != OP_OR)
        {
            fprintf(stderr, "minibash: conditional only supports '&&' and '||' operators\n");
            return -1;
        }
    }

    /* Execute first command unconditionally. */
    last_rc = do_dispatch_cmd(&cmdline->cmds[0], "conditional");
    if (last_rc < 0)
        return last_rc;

    /* Left-to-right short-circuit evaluation. */
    for (i = 1; i < cmdline->cmd_count; i++)
    {
        bool should_run = false;
        const Command *cmd = &cmdline->cmds[i];
        OperatorType op = cmdline->ops[i - 1];

        if (op == OP_AND)
            should_run = (last_rc == 0);
        else if (op == OP_OR)
            should_run = (last_rc != 0);

        if (!should_run)
            continue;

        last_rc = do_dispatch_cmd(cmd, "conditional");
        if (last_rc < 0)
            return last_rc;
    }

    return last_rc;
}

/*
 * exc_pipe_cmd – 执行管道：cmd0 | cmd1 | ... | cmdN-1
 * 策略：
 * 1. 预先创建 N-1 个管道（pipes[i] 连接 cmd[i] 的标准输出 → cmd[i+1] 的标准输入）。
 * 2. 在关闭父进程中的任何内容之前，先创建所有 N 个子进程，以避免在下一个子进程继承之前意外关闭写入结束符。
 * 3. 在所有子进程创建完成后，关闭父进程中的所有管道文件描述符。
 * 4. 等待所有子进程完成；返回最后一个子进程的退出状态。
 */
static int exc_pipe_cmd(CommandLine *cmdline)
{
    // 正向管道：cmd0 | cmd1 | ... | cmdN-1
    return run_pipe(cmdline, OP_PIPE, "pipe", false);
}

// 反向管道：cmd0 ~ cmd1 ~ ... ~ cmdN-1
static int exc_reverse_pipe_cmd(CommandLine *cmdline)
{
    // 按右到左执行，即 cmdN-1 先产出，最终流向 cmd0。
    return run_pipe(cmdline, OP_REVERSE_PIPE, "reverse pipe", true);
}

/**
 * exc_fifo_write_cmd – 执行 FIFO 写命令：cmd >|||
 * 策略：
 * 1. 参数校验：||| 写端必须有可执行命令（cmd->argc > 0），且不能是内置命令
 * 2. FIFO 路径：~/Assignments/Assignment3/minibash_fifo
 * 3. 创建 FIFO：如果不存在则创建，权限 0666
 * 4. 非阻塞打开写端（O_WRONLY | O_NONBLOCK）：
 *      无读端时不会卡死 shell
 *      给出明确错误提示
 * 5. 重定向：调用 run_cmd(cmd, ..., stdout_fd=fifo_fd) 把命令输出写入 FIFO
 * 6. 错误处理：参数不对、内置命令、FIFO 创建/打开失败都会报错并返回 -1
 */
static int exc_fifo_write_cmd(const Command *cmd)
{
    char fifo_path[PATH_MAX];
    int fd;
    bool foreground;

    if (cmd == NULL || cmd->argc <= 0)
    {
        fprintf(stderr, "minibash: ||| write requires a command\n");
        return -1;
    }

    if (ensure_common_fifo(fifo_path, sizeof(fifo_path)) < 0)
        return -1;

    // 非阻塞打开写端：若当前没有读端，直接返回错误而不是挂住 shell。
    fd = open(fifo_path, O_WRONLY | O_NONBLOCK);
    if (fd < 0)
    {
        if (errno == ENXIO)
            fprintf(stderr, "minibash: no FIFO reader available (use |||cmd in another shell first)\n");
        else
            perror(fifo_path);
        return -1;
    }

    foreground = !cmd->run_in_background;
    // 把当前命令标准输出重定向到公共 FIFO。
    {
        int rc = run_cmd(cmd, foreground, -1, fd);
        close(fd);
        return rc;
    }
}

/**
 * exc_fifo_read_cmd – 执行 FIFO 读命令：cmd <|||
 * 策略：
 * 1. 参数校验：||| 读端必须有可执行命令（cmd->argc > 0），且不能是内置命令
 * 2. FIFO 路径：~/Assignments/Assignment3/minibash_fifo， 调用 ensure_common_fifo(...) 获取路径
 * 3. 打开 FIFO 读端：open(fifo_path, O_RDONLY)（阻塞模式），没有写端时等待数据到来
 * 4. 执行命令：run_cmd(cmd, foreground, fifo_fd, -1)，把 FIFO 接到命令 stdin
 * 5. 清理：父进程关闭 fd
 * 6. 错误处理：参数不对、内置命令、FIFO 创建/打开失败都会报错并返回
 */
static int exc_fifo_read_cmd(const Command *cmd)
{
    char fifo_path[PATH_MAX];
    int fd;
    bool foreground;

    if (cmd == NULL || cmd->argc <= 0)
    {
        fprintf(stderr, "minibash: ||| read requires a command\n");
        return -1;
    }

    if (ensure_common_fifo(fifo_path, sizeof(fifo_path)) < 0)
        return -1;

    // 读端使用阻塞打开：没有写端时等待数据到来。
    fd = open(fifo_path, O_RDONLY);
    if (fd < 0)
    {
        perror(fifo_path);
        return -1;
    }

    foreground = !cmd->run_in_background;
    // 把公共 FIFO 作为当前命令的标准输入。
    {
        int rc = run_cmd(cmd, foreground, fd, -1);
        close(fd);
        return rc;
    }
}

/**
 * exc_word_count_cmd – 执行单词计数命令：# filename.txt
 * * 策略：
 * 1. 参数校验：必须是且仅是 1 个参数
 * 2. 文件类型校验：参数必须以 .txt 结尾
 * 3. 文件读取：按空白分词逐个统计
 * 4. 输出：打印单词总数（仅数字）
 * 5. 错误处理：参数不对、非 .txt、文件打开/读取失败都会报错并返回 -1
 */
static int exc_word_count_cmd(const Command *cmd)
{
    FILE *fp;
    const char *path;
    char word[MAX_LINE];
    long long count = 0;

    // 规范里 # 是一元操作：# sample.txt
    // 这里要求正好一个参数，并且是 .txt 文件。
    if (cmd == NULL || cmd->argc != 1)
    {
        fprintf(stderr, "minibash: #: expected exactly one .txt file\n");
        return -1;
    }

    path = cmd->argv[0];
    if (!is_txt_file_path(path))
    {
        fprintf(stderr, "minibash: #: only .txt files are supported\n");
        return -1;
    }

    fp = fopen(path, "r");
    if (fp == NULL)
    {
        perror(path);
        return -1;
    }

    // 以空白符分隔读取“单词”。
    while (fscanf(fp, "%511s", word) == 1)
        count++;

    if (ferror(fp))
    {
        perror(path);
        fclose(fp);
        return -1;
    }

    fclose(fp);

    // 输出该文件的单词数量。
    printf("%lld\n", count);
    return 0;
}

/**
 * exc_txt_cat_cmd – 执行文本连接命令：+ file1.txt file2.txt ...
 * 策略：
 * 1. 参数校验：必须至少有 1 个参数
 * 2. 文件类型校验：每个参数必须以 .txt 结尾
 * 3. 文件读取：按用户输入顺序依次读取每个文本文件内容
 * 4. 输出：将所有文本文件内容连接输出到标准输出
 * 5. 错误处理：参数不对、非 .txt、文件打开/读取失败都会报错并返回 -1
 */
static int exc_txt_cat_cmd(const Command *cmd)
{
    int i;

    if (cmd == NULL || cmd->argc <= 0)
    {
        fprintf(stderr, "minibash: +: expected at least one .txt file\n");
        return -1;
    }

    // 按用户输入顺序依次读取并输出每个文本文件内容。
    for (i = 0; i < cmd->argc; i++)
    {
        const char *path = cmd->argv[i];
        char *buf = NULL;
        size_t len = 0;

        if (!is_txt_file_path(path))
        {
            fprintf(stderr, "minibash: +: only .txt files are supported (%s)\n", path);
            return -1;
        }

        if (read_file_all(path, &buf, &len) < 0)
            return -1;

        if (len > 0 && fwrite(buf, 1, len, stdout) != len)
        {
            perror("stdout");
            free(buf);
            return -1;
        }

        free(buf);
    }

    return 0;
}

/**
 * exc_txt_app_cmd – 执行文本追加命令：file1.txt ++ file2.txt
 * 策略：
 * 1. 参数校验：必须且仅有 2 个参数
 * 2. 文件类型校验：每个参数必须以 .txt 结尾
 * 3. 先读取 file1 原始内容到内存 buf1
 * 4. 再读取 file2 原始内容到内存 buf2
 * 5. 把 buf2 追加到 file1
 * 6. 把 buf1 追加到 file2
 * 7. 文件写入：将 file2.txt 的原始内容追加到 file1.txt，将 file1.txt 的原始内容追加到 file2.txt
 * 8. 错误处理：参数不对、非 .txt、文件打开/读取/写入失败都会报错并返回 -1
 */
static int exc_txt_app_cmd(const Command *cmd)
{
    const char *path1;
    const char *path2;
    char *buf1 = NULL;
    char *buf2 = NULL;
    size_t buf1_len = 0;
    size_t buf2_len = 0;

    // 规范里 ++ 是二元操作：file1.txt ++ file2.txt
    // 需要把 file2 原始内容追加到 file1，同时把 file1 原始内容追加到 file2。
    if (cmd == NULL || cmd->argc != 2)
    {
        fprintf(stderr, "minibash: ++: expected exactly two .txt files\n");
        return -1;
    }

    path1 = cmd->argv[0];
    path2 = cmd->argv[1];

    if (!is_txt_file_path(path1) || !is_txt_file_path(path2))
    {
        fprintf(stderr, "minibash: ++: only .txt files are supported\n");
        return -1;
    }

    if (strcmp(path1, path2) == 0)
    {
        fprintf(stderr, "minibash: ++: requires two different files\n");
        return -1;
    }

    // 先读取两个文件的原始快照，避免互相覆盖污染。
    if (read_file_all(path1, &buf1, &buf1_len) < 0)
        return -1;

    if (read_file_all(path2, &buf2, &buf2_len) < 0)
    {
        free(buf1);
        return -1;
    }

    // 用 file2 的原始快照追加到 file1。
    if (append_buffer_to_file(path1, buf2, buf2_len) < 0)
    {
        free(buf1);
        free(buf2);
        return -1;
    }

    // 用 file1 的原始快照追加到 file2。
    if (append_buffer_to_file(path2, buf1, buf1_len) < 0)
    {
        free(buf1);
        free(buf2);
        return -1;
    }

    free(buf1);
    free(buf2);
    return 0;
}

// 根据解析结果中的命令类型调用不同的执行函数
static int do_execute(CommandLine *cmdline)
{
    int i;
    ExecType dispatch = EXEC_SINGLE;

    if (cmdline == NULL || cmdline->cmd_count == 0)
        return -1;

    // 单命令直接根据 node_type 分发
    if (cmdline->cmd_count == 1)
        return do_dispatch_cmd(&cmdline->cmds[0], "single");

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
        bg_reap(); /* reap any finished bg children and keep bg_count accurate. */
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

        if (strchr(line, '\n') == NULL)
        {
            report_parse_error(PARSE_ERR_INPUT_LINE_TOO_LONG);
            discard_rest_of_line();
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
        int ntok = do_lex(line, tokens, MAX_TOKENS);

        if (ntok < 0)
        {
            report_parse_error(ntok);
            continue;
        }

        int prc = parse_command_line(tokens, ntok, &parsed);
        if (prc < 0)
        {
            report_parse_error(prc);
            continue;
        }

        // 这里根据 parsed 中的命令类型调用不同的执行函数
        do_execute(&parsed);

        // 释放解析结果中分配的内存
        free_parsed_command_line(&parsed);
    }

    return 0;
}