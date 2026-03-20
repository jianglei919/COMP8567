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

// Connection type between command segments
typedef enum OperatorType
{
    OP_PIPE,
    OP_AND,
    OP_OR,
    OP_SEQ,
    OP_REVERSE_PIPE
} OperatorType;

// Command node type
typedef enum NodeType
{
    NODE_SIMPLE,     // regular command
    NODE_BG,         // background command
    NODE_FIFO_W,     // FIFO writer side
    NODE_FIFO_R,     // FIFO reader side
    NODE_WORD_COUNT, // special word-count command
    NODE_TXT_CAT,    // text concatenation command (+)
    NODE_TXT_APP     // text append command (++)
} NodeType;

// Token types produced by lexical analysis
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

// Token structure produced by lexical analysis
typedef struct Token
{
    TokType type;        // token type
    char text[MAX_LINE]; // token text, up to MAX_LINE-1 chars plus trailing '\0'
} Token;

// Parsed command structure
typedef struct Command
{
    char *argv[MAX_ARGC + 1]; // required by execvp; last entry must be NULL
    int argc;                 // argument count including command name, range 1-4

    NodeType node_type; // type of this command, used by executor dispatch

    char raw[1024]; // raw command text for debugging/error reporting

    pid_t pid;        // pid of the process running this command
    int exit_status;  // exit status after foreground execution
    int has_executed; // whether command has been executed

    int run_in_background; // whether to run in background (mainly for simple/last segment)

    // redirection info
    char input_file[512];  // <
    char output_file[512]; // > or >>
    int redirect_in;       // whether input redirection exists
    int redirect_out;      // whether output redirection exists
    int append_out;        // 1 means >>, 0 means >

} Command;

// Command-line structure: multiple commands and connecting operators
typedef struct CommandLine
{
    Command cmds[MAX_CMDS]; // supports up to MAX_CMDS command segments
    int cmd_count;          // actual command segment count, range 1..MAX_CMDS

    OperatorType ops[MAX_CMDS - 1]; // operators connecting commands, one fewer than command count
    int op_count;                   // actual operator count, range 0..MAX_CMDS-1
} CommandLine;

// Command execution type
typedef enum ExecType
{
    EXEC_SINGLE,       // single command, dispatched by node_type
    EXEC_SEQUENCE,     // ; sequential command execution
    EXEC_CONDITIONAL,  // conditional execution with && and ||
    EXEC_PIPE,         // pipeline command
    EXEC_REVERSE_PIPE, // reverse pipeline command
    EXEC_INVALID       // unsupported mixed-operator combination
} ExecType;

// Parse error codes
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

// Trim leading and trailing whitespace from string
static void trim_inplace(char *s)
{
    if (s == NULL)
        return;

    int start = 0;
    int end = strlen(s) - 1;
    int i, j = 0;

    // find first non-space from the left
    while (s[start] != '\0' && isspace((unsigned char)s[start]))
        start++;

    // if the string is all spaces
    if (s[start] == '\0')
    {
        s[0] = '\0';
        return;
    }

    // find first non-space from the right
    while (end >= 0 && isspace((unsigned char)s[end]))
        end--;

    // shift the valid middle segment to the front
    for (i = start; i <= end; i++)
    {
        s[j] = s[i];
        j++;
    }

    s[j] = '\0';
}

// discard the rest of the input line until newline or EOF
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

// Check whether command name is a builtin
static bool is_builtin(const char *name)
{
    int i;
    for (i = 0; BUILTIN_NAMES[i] != NULL; i++)
        if (strcmp(name, BUILTIN_NAMES[i]) == 0)
            return true;
    return false;
}

// Push one token into array: 0 on success, negative on error
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

// Check whether token type is a command connector (|, ||, |||, &&, ;, ~, or EOF)
static bool is_command_connector(TokType type)
{
    return type == TOK_PIPE || type == TOK_DAMP || type == TOK_DPIPE ||
           type == TOK_SEMI || type == TOK_TILDE || type == TOK_EOF;
}

// Append one argument to Command.argv: 0 on success, negative on error
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

// Validate whether path is a .txt file
static bool is_txt_file_path(const char *path)
{
    size_t len;

    if (path == NULL)
        return false;

    len = strlen(path);
    return len >= 4 && strcmp(path + len - 4, ".txt") == 0;
}

// Read entire file into memory buffer (caller frees)
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

        // n can be 0 on EOF or error; check ferror to distinguish
        if (n > 0)
        {
            if (len + n > cap)
            {
                size_t new_cap = (cap == 0) ? 4096 : cap;
                char *tmp;
                while (new_cap < len + n)
                    new_cap *= 2;

                tmp = realloc(buf, new_cap);
                if (tmp == NULL) // realloc failure does not free original buffer, so we can still clean up properly
                {
                    perror("realloc");
                    fclose(fp);
                    free(buf);
                    return -1;
                }
                buf = tmp;
                cap = new_cap;
            }

            // append chunk to buf
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

// Append memory buffer to end of target file
static int append_buffer_to_file(const char *path, const char *buf, size_t len)
{
    FILE *fp = fopen(path, "a");
    if (fp == NULL)
    {
        perror(path);
        return -1;
    }

    // if len is 0, we still want to create the file if it does not exist,
    // but we should not call fwrite with a NULL buffer even if it is valid according to C standard,
    // because some implementations may not handle it well.
    if (len > 0 && fwrite(buf, 1, len, fp) != len)
    {
        perror(path);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

// Build and ensure common FIFO exists; return 0 on success and output fifo_path.
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

/*
 * do_lex - convert one raw input line into a flat token sequence.
 *
 * High-level flow:
 * 1. Validate pointers and token-buffer capacity.
 * 2. Scan input left-to-right with cursor p.
 * 3. Skip whitespace.
 * 4. Match multi-character operators first (|||, ||, &&, >>, ++)
 *    so they are not split into shorter tokens.
 * 5. Match single-character operators and meta symbols
 *    (|, &, ;, ~, +, #, <, >).
 * 6. Otherwise parse a word token until whitespace/operator boundary.
 * 7. Detect word overflow and token-array overflow through helpers.
 * 8. Append TOK_EOF sentinel and return number of non-EOF tokens.
 *
 * Return value:
 * - >= 0: number of produced tokens (excluding TOK_EOF sentinel).
 * - < 0 : ParseErrorCode describing the first lexical failure.
 */
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

// Parse command connector into OperatorType
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

/*
 * parse_command - parse exactly one command segment from token stream.
 *
 * Inputs:
 * - tokens/ntok: lexical tokens from do_lex.
 * - pos        : in/out cursor; starts at first token of this segment and
 *                advances to connector/EOF or parse error.
 * - out        : destination Command, fully reset by this function.
 *
 * Segment boundary:
 * - Parsing stops when current token is a command connector
 *   (|, &&, ||, ;, ~) or TOK_EOF.
 *
 * What this function handles:
 * - Regular argv words.
 * - Node-type markers (#, |||, +, ++, &).
 * - Redirections (<, >, >>) with strict "next token must be WORD" validation.
 *
 * Special notes:
 * - '&' marks NODE_BG and ends this segment immediately.
 * - '|||' is interpreted as reader or writer depending on whether argv
 *   already has content (argc==0 -> reader, otherwise writer).
 * - A valid command segment must contain at least one argv word.
 *
 * Return value:
 * - 1  : parsed one valid command segment.
 * - <0 : ParseErrorCode or validation failure.
 */
int parse_command(const Token *tokens, int ntok, int *pos, Command *out)
{
    if (tokens == NULL || pos == NULL || out == NULL || *pos >= ntok)
        return PARSE_ERR_INVALID_INPUT;

    memset(out, 0, sizeof(*out));
    out->node_type = NODE_SIMPLE;

    while (*pos < ntok && !is_command_connector(tokens[*pos].type))
    {
        // we can safely read tokens[*pos] here because the loop condition ensures *pos < ntok

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

/*
 * parse_command_line - parse full token stream into CommandLine.
 *
 * Build strategy:
 * 1. Reset output CommandLine.
 * 2. Repeatedly parse one command segment via parse_command().
 * 3. After each command (except at EOF), parse exactly one connector
 *    via parse_operator().
 * 4. Enforce MAX_CMDS and MAX_CMDS-1 operator limits.
 * 5. Stop cleanly at TOK_EOF.
 *
 * Structural guarantees on success:
 * - cmd_count > 0
 * - op_count == cmd_count - 1  (for multi-command input)
 * - commands/operators are stored in left-to-right user order.
 *
 * Return value:
 * - 0  : fully parsed command line.
 * - <0 : ParseErrorCode at first structural/semantic failure.
 */
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

// Free argv strings allocated in Command
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

// Free argv strings allocated in CommandLine
static void free_parsed_command_line(CommandLine *cmdline)
{
    int i;

    if (cmdline == NULL)
        return;

    for (i = 0; i < cmdline->cmd_count; i++)
        free_parsed_command(&cmdline->cmds[i]);
}

/*
 * run_cmd - the single fork+exec helper used by all executors.
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
 * run_pipe - Handle both forward (|) and reverse (~) pipelines in one helper
 *
 * Parameters:
 * - expected_op: expected connector type (OP_PIPE or OP_REVERSE_PIPE)
 * - where:       used in error messages to identify caller
 * - reverse:     false: execute cmd0->cmdN-1; true: execute cmdN-1->cmd0
 *
 * Overall strategy:
 * 1. pre-create N-1 pipes.
 * 2. fork N children and wire stdin/stdout by stage(i).
 * 3. children close all original pipe fds to avoid leaks/EOF hangs.
 * 4. child then applies its own < / > / >> redirection (overrides pipeline wiring).
 * 5. parent closes all pipe fds and waits for all children.
 * 6. return exit status of the last stage.
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

    // here i is stage index in the execution chain, not original cmd index.
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
            // stage i reads from previous pipe and writes to next pipe.
            // this wiring rule is identical for forward and reverse modes;
            // the only difference is which command cmd_idx maps to.
            if (i > 0)
                dup2(pipes[i - 1][0], STDIN_FILENO);
            if (i < cmd_count - 1)
                dup2(pipes[i][1], STDOUT_FILENO);

            // close all original pipe fds in child:
            // otherwise write ends may remain open and readers never see EOF.
            for (j = 0; j < cmd_count - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // file redirection has higher priority than pipeline wiring:
            // for example in cmd < in.txt | ..., stdin ultimately comes from file.
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

    // parent does not read/write data; must close all pipe fds to avoid EOF-related blocking.
    for (i = 0; i < cmd_count - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // wait for all stages; current implementation returns exit code of last stage (i=cmd_count-1).
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

// Given pid, check whether it runs the same executable as current process
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

// Check whether target pid is running on a different terminal
static bool is_on_different_terminal_from_self(pid_t pid)
{
    char self_tty[PATH_MAX];
    char other_tty[PATH_MAX];
    char fd0_link[64];
    ssize_t n;

    // current process terminal (e.g., /dev/pts/3)
    if (ttyname_r(STDIN_FILENO, self_tty, sizeof(self_tty)) != 0)
        return false;

    if (snprintf(fd0_link, sizeof(fd0_link), "/proc/%d/fd/0", (int)pid) >= (int)sizeof(fd0_link))
        return false;

    // read terminal device that target process stdin points to
    n = readlink(fd0_link, other_tty, sizeof(other_tty) - 1);
    if (n < 0)
        return false;
    other_tty[n] = '\0';

    return strcmp(self_tty, other_tty) != 0;
}

// Send signal to other minibash instances (excluding self), return successful count
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

        // killallmb targets minibash instances on different terminals.
        if (!is_on_different_terminal_from_self(pid))
            continue;

        if (kill(pid, sig) == 0)
            count++;
    }

    closedir(dir);
    return count;
}

// Dispatch to execution function by command node type (NodeType)
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

// Execute builtin command: return 0 if handled, negative if not builtin or failed
static int exc_builtin_cmd(const Command *cmd)
{
    const char *name = cmd->argv[0];

    if (strcmp(name, "killmb") == 0)
    {
        printf("minibash: exiting.\n");
        // terminate current minibash terminal session.
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

// Execute single command, handling builtin and external commands
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
 * Execute command sequence in order, regardless of previous result.
 * Example: `cmd1 ; cmd2 ; cmd3` executes cmd1 -> cmd2 -> cmd3.
 * 1. validate cmdline input
 * 2. validate operator chain is pure ; (OP_SEQ)
 * 3. iterate through each Command in order
 * 4. dispatch to matching exc_* function via switch(cmd->node_type)
 * 5. return exit status of the last command
 */
static int exc_sequence_cmd(CommandLine *cmdline)
{
    int i;
    int last_rc = 0; // return code of the last command

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
 * Execute conditional commands left-to-right based on operator and previous result.
 * Operators must be only OP_AND or OP_OR; no other types allowed.
 * Example: `cmd1 && cmd2 || cmd3` executes cmd1 first; run cmd2 if cmd1 succeeds (0), otherwise run cmd3.
 * 1. first command executes unconditionally
 * 2. then evaluate left-to-right with short-circuit rules
 * 3. &&: run next only if previous succeeded (last_rc == 0)
 * 4. ||: run next only if previous failed (last_rc != 0)
 */
static int exc_conditional_cmd(CommandLine *cmdline)
{
    int i;
    int last_rc = 0;

    // basic validity checks
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
 * exc_pipe_cmd - Execute pipeline: cmd0 | cmd1 | ... | cmdN-1
 * Strategy:
 * 1. pre-create N-1 pipes (pipes[i] connects stdout of cmd[i] to stdin of cmd[i+1]).
 * 2. create all N children before closing parent fds to avoid inheritance timing issues.
 * 3. after all children are created, close all pipe fds in parent.
 * 4. wait for all children; return exit status of last child.
 */
static int exc_pipe_cmd(CommandLine *cmdline)
{
    // forward pipeline: cmd0 | cmd1 | ... | cmdN-1
    return run_pipe(cmdline, OP_PIPE, "pipe", false);
}

// reverse pipeline: cmd0 ~ cmd1 ~ ... ~ cmdN-1
static int exc_reverse_pipe_cmd(CommandLine *cmdline)
{
    // execute right-to-left: cmdN-1 produces first and output ultimately flows to cmd0.
    return run_pipe(cmdline, OP_REVERSE_PIPE, "reverse pipe", true);
}

/**
 * exc_fifo_write_cmd - Execute FIFO write command: cmd >|||
 * Strategy:
 * 1. argument check: FIFO writer requires executable command (cmd->argc > 0), not builtin
 * 2. FIFO path: ~/Assignments/Assignment3/minibash_fifo
 * 3. create FIFO if missing, mode 0666
 * 4. open writer in non-blocking mode (O_WRONLY | O_NONBLOCK):
 *      shell does not hang when no reader exists
 *      print explicit error message
 * 5. redirection: run_cmd(cmd, ..., stdout_fd=fifo_fd) writes command output to FIFO
 * 6. error handling: invalid args, builtin misuse, or FIFO create/open failure returns -1 with message
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

    // open writer non-blocking: if no reader exists, return error instead of hanging shell.
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
    // redirect current command stdout to the shared FIFO.
    {
        int rc = run_cmd(cmd, foreground, -1, fd);
        close(fd);
        return rc;
    }
}

/**
 * exc_fifo_read_cmd - Execute FIFO read command: cmd <|||
 * Strategy:
 * 1. argument check: FIFO reader requires executable command (cmd->argc > 0), not builtin
 * 2. FIFO path: ~/Assignments/Assignment3/minibash_fifo, resolved by ensure_common_fifo(...)
 * 3. Open FIFO reader side with open(fifo_path, O_RDONLY) in blocking mode; wait if no writer exists
 * 4. execute command with run_cmd(cmd, foreground, fifo_fd, -1) so FIFO feeds stdin
 * 5. cleanup: parent closes fd
 * 6. error handling: invalid args, builtin misuse, FIFO create/open failure
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

    // reader uses blocking open: waits for writer/data when absent.
    fd = open(fifo_path, O_RDONLY);
    if (fd < 0)
    {
        perror(fifo_path);
        return -1;
    }

    foreground = !cmd->run_in_background;
    // use shared FIFO as current command stdin.
    {
        int rc = run_cmd(cmd, foreground, fd, -1);
        close(fd);
        return rc;
    }
}

/**
 * exc_word_count_cmd - Execute word-count command: # filename.txt
 * * Strategy:
 * 1. argument check: must be exactly one argument
 * 2. file type check: argument must end with .txt
 * 3. file read: split by whitespace and count words
 * 4. output: print total word count (number only)
 * 5. error handling: invalid args, non-.txt, file open/read failure return -1
 */
static int exc_word_count_cmd(const Command *cmd)
{
    FILE *fp;
    const char *path;
    char word[MAX_LINE];
    long long count = 0;

    // Per spec, # is a unary operation: # sample.txt
    // require exactly one argument and it must be a .txt file.
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

    // read words separated by whitespace.
    while (fscanf(fp, "%511s", word) == 1)
        count++;

    if (ferror(fp))
    {
        perror(path);
        fclose(fp);
        return -1;
    }

    fclose(fp);

    // print word count for this file.
    printf("%lld\n", count);
    return 0;
}

/**
 * exc_txt_cat_cmd - Execute text concatenation command: + file1.txt file2.txt ...
 * Strategy:
 * 1. argument check: requires at least one argument
 * 2. file type check: each argument must end with .txt
 * 3. file read: read each text file in user-specified order
 * 4. output: concatenate all text contents to stdout
 * 5. error handling: invalid args, non-.txt, file open/read failure return -1
 */
static int exc_txt_cat_cmd(const Command *cmd)
{
    int i;

    if (cmd == NULL || cmd->argc <= 0)
    {
        fprintf(stderr, "minibash: +: expected at least one .txt file\n");
        return -1;
    }

    // read and print each text file in user input order.
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
 * exc_txt_app_cmd - Execute text append command: file1.txt ++ file2.txt
 * Strategy:
 * 1. argument check: must be exactly two arguments
 * 2. file type check: each argument must end with .txt
 * 3. read original file1 content into memory buf1
 * 4. then read original file2 content into memory buf2
 * 5. append buf2 to file1
 * 6. append buf1 to file2
 * 7. file write: append original file2 content to file1, and original file1 content to file2
 * 8. error handling: invalid args, non-.txt, file open/read/write failure return -1
 */
static int exc_txt_app_cmd(const Command *cmd)
{
    const char *path1;
    const char *path2;
    char *buf1 = NULL;
    char *buf2 = NULL;
    size_t buf1_len = 0;
    size_t buf2_len = 0;

    // Per spec, ++ is a binary operation: file1.txt ++ file2.txt
    // append original file2 content to file1 and original file1 content to file2.
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

    // read original snapshots of both files first to avoid cross-overwrite.
    if (read_file_all(path1, &buf1, &buf1_len) < 0)
        return -1;

    if (read_file_all(path2, &buf2, &buf2_len) < 0)
    {
        free(buf1);
        return -1;
    }

    // append file2 original snapshot to file1.
    if (append_buffer_to_file(path1, buf2, buf2_len) < 0)
    {
        free(buf1);
        free(buf2);
        return -1;
    }

    // append file1 original snapshot to file2.
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

// Call different executors based on parsed command/operator types
static int do_execute(CommandLine *cmdline)
{
    int i;
    ExecType dispatch = EXEC_SINGLE;

    if (cmdline == NULL || cmdline->cmd_count == 0)
        return -1;

    // single command: dispatch directly by node_type
    if (cmdline->cmd_count == 1)
        return do_dispatch_cmd(&cmdline->cmds[0], "single");

    // multiple commands: dispatch by connector type
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

        line[strcspn(line, "\n")] = '\0'; // strip trailing newline

        trim_inplace(line);

        if (line[0] == '\0')
        {
            continue;
        }

        Token tokens[MAX_TOKENS];
        CommandLine parsed;

        // lexical and syntax analysis
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

        // execute by command types parsed into parsed structure
        do_execute(&parsed);

        // free memory allocated in parsed result
        free_parsed_command_line(&parsed);
    }

    return 0;
}