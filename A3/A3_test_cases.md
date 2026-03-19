# A3 Minibash Complete Test Cases

## 1. Scope
This test suite covers all assignment requirements from A3_W26:
- Rule 1: killmb
- Rule 2: killallmb (cross-terminal)
- Rule 3: argc range [1,4]
- Rule 4: argc range [1,4] for each command used with operators
- Special operators and commands: &, pstop, cont, numbg, killbp, |, ~, |||, <, >, >>, ;, &&, ||, ++, #, +
- Error handling and boundary behavior

## 2. Test Environment
- OS: Linux (CS server)
- Build command:
```bash
gcc -Wall -Wextra -std=c11 A3.c -o A3
```
- Program start:
```bash
./A3
```

## 3. Test Data Preparation
Run these commands before testing:
```bash
cd /home/jiang6m/workspace/MyProject/COMP8567/A3
printf "alpha beta\n" > t_wc.txt
printf "left\n" > t_left.txt
printf "right\n" > t_right.txt
printf "hello world\n" > in_redir.txt
rm -f out_redir.txt out_append.txt
```

## 4. Functional Test Cases

### 4.1 Rule 1 and Rule 2

| ID | Requirement | Input in minibash | Expected Result |
|---|---|---|---|
| TC-R1-01 | killmb exits current minibash | `killmb` | Current minibash process exits cleanly. |
| TC-R2-01 | killallmb kills other terminals only | In Terminal A: `killallmb`; Terminal B running minibash idle | Terminal B minibash exits; Terminal A keeps running. |
| TC-R2-02 | killallmb with no other minibash | `killallmb` | No crash; prints signaled count as 0. |

### 4.2 Rule 3 and Rule 4 (argc limits)

| ID | Requirement | Input in minibash | Expected Result |
|---|---|---|---|
| TC-ARG-01 | argc lower bound for command | `pwd` | Runs successfully. |
| TC-ARG-02 | argc upper bound for command | `ls -l -t -a` | Runs successfully. |
| TC-ARG-03 | argc overflow for command | `echo a b c d` | Error: too many arguments (command not executed). |
| TC-ARG-04 | argc limit with pipe left cmd | `ls -l -t -a | wc -w` | Runs successfully. |
| TC-ARG-05 | argc overflow with operator | `echo a b c d | wc` | Error due to first command argc overflow. |

### 4.3 Background and Process Control

| ID | Requirement | Input in minibash | Expected Result |
|---|---|---|---|
| TC-BG-01 | start background process | `sleep 5 &` | Background pid message shown; shell returns immediately. |
| TC-BG-02 | numbg count increases | `sleep 5 &` then `numbg` | Shows at least 1 background process. |
| TC-BG-03 | pstop on latest bg process | `sleep 20 &` then `pstop` | Latest bg process is stopped. |
| TC-BG-04 | cont resumes stopped process fg | After TC-BG-03, run `cont` | Process continues in foreground and shell waits for finish. |
| TC-BG-05 | killbp kills all bg process | `sleep 20 &` then `sleep 20 &` then `killbp` then `numbg` | All bg processes killed; count becomes 0. |
| TC-BG-06 | pstop without bg process | `pstop` | Error message: no background process to stop. |
| TC-BG-07 | cont without stopped process | `cont` | Error message: no stopped process to continue. |

### 4.4 Pipe and Reverse Pipe

| ID | Requirement | Input in minibash | Expected Result |
|---|---|---|---|
| TC-PIPE-01 | normal single pipe | `echo one two | wc -w` | Prints `2`. |
| TC-PIPE-02 | multi-stage pipe | `printf a\\nb\\n | wc -l | wc -w` | Executes full chain correctly. |
| TC-PIPE-03 | reverse pipe basic | `wc -w ~ echo a b c` | Prints `3`. |
| TC-PIPE-04 | reverse pipe multi-stage | `wc -w ~ wc ~ ls -1` | Data flows right-to-left correctly. |

### 4.5 FIFO Common Pipe (|||)

| ID | Requirement | Input in minibash | Expected Result |
|---|---|---|---|
| TC-FIFO-01 | write to FIFO with reader | Terminal A: `echo one two |||`; Terminal B: `|||wc -w` | Reader prints `2`. |
| TC-FIFO-02 | write with no reader | `echo hi |||` | No hang; clear error about missing reader. |
| TC-FIFO-03 | read waits for writer | `|||wc -w` (then send writer from another shell) | Reader blocks then completes when writer writes. |
| TC-FIFO-04 | fifo path auto-created | first use of `|||` | FIFO created under `~/Assignments/Assignment3`. |

### 4.6 Redirection

| ID | Requirement | Input in minibash | Expected Result |
|---|---|---|---|
| TC-REDIR-01 | input redirection | `wc -w < in_redir.txt` | Prints `2`. |
| TC-REDIR-02 | output redirection overwrite | `echo first > out_redir.txt` | File created with `first`. |
| TC-REDIR-03 | output redirection append | `echo second >> out_redir.txt` | File contains both lines in order. |
| TC-REDIR-04 | missing input file | `wc -w < no_such_file.txt` | Error shown; no crash. |

### 4.7 Sequence and Conditionals

| ID | Requirement | Input in minibash | Expected Result |
|---|---|---|---|
| TC-SEQ-01 | sequence execution | `echo A ; echo B ; echo C` | Prints A/B/C in order. |
| TC-COND-01 | AND short-circuit true->run | `true && echo ok` | Prints `ok`. |
| TC-COND-02 | AND short-circuit false->skip | `false && echo no` | `echo no` not executed. |
| TC-COND-03 | OR short-circuit false->run | `false || echo fallback` | Prints `fallback`. |
| TC-COND-04 | OR short-circuit true->skip | `true || echo no` | `echo no` not executed. |
| TC-COND-05 | mixed && and || chain | `false && echo a || echo b && echo c` | Evaluates left-to-right short-circuit correctly. |

### 4.8 Text Operators (#, +, ++)

| ID | Requirement | Input in minibash | Expected Result |
|---|---|---|---|
| TC-TXT-01 | word count # | `# t_wc.txt` | Prints `2`. |
| TC-TXT-02 | concatenate + | `t_left.txt + t_right.txt` | stdout is `left` then `right`. |
| TC-TXT-03 | append ++ | `t_left.txt ++ t_right.txt` | Each file gets the other file's original content appended. |
| TC-TXT-04 | # non-txt file | `# A3.c` | Error: only .txt supported. |
| TC-TXT-05 | + with non-txt arg | `t_left.txt + A3.c` | Error: only .txt supported. |
| TC-TXT-06 | ++ same file | `t_left.txt ++ t_left.txt` | Error: requires two different files. |

## 5. Negative and Boundary Cases

| ID | Category | Input in minibash | Expected Result |
|---|---|---|---|
| TC-NEG-01 | empty input | press Enter | Prompt returns, no crash. |
| TC-NEG-02 | operator only | `|` | Parse error message. |
| TC-NEG-03 | operator mix unsupported | `echo a | wc && echo b` | Error for unsupported mixed operators. |
| TC-NEG-04 | too many commands in one line | `echo 1 ; echo 2 ; echo 3 ; echo 4 ; echo 5 ; echo 6` | Error: too many command segments/operators. |
| TC-NEG-05 | very long input > MAX_LINE | paste long line > 511 chars | Error: input line too long; shell remains usable. |

## 6. Cross-Terminal Acceptance Procedure (Mandatory)

### 6.1 killallmb true cross-terminal
1. Open Terminal A and Terminal B.
2. Start minibash in both terminals.
3. In Terminal B, keep shell idle at prompt.
4. In Terminal A, run: `killallmb`.
5. Verify Terminal B minibash exits and Terminal A remains active.
6. In Terminal A, run `killmb` to close self.

Expected:
- B is terminated by A's command.
- A does not kill itself with killallmb.

### 6.2 FIFO cross-terminal producer/consumer
1. Terminal B: run `|||wc -w` (reader blocks).
2. Terminal A: run `echo hello from fifo |||`.
3. Verify Terminal B prints `3`.

Expected:
- Cross-terminal FIFO transfer succeeds.
- No deadlock after transfer.

## 7. Regression Pack (Quick Run Order)
Use this order before final submission:
1. Build: `gcc -Wall -Wextra -std=c11 A3.c -o A3`
2. Core parse: `|`, `;`, `&&`, `||`
3. Operators: `|`, `~`, `|||`, `<`, `>`, `>>`
4. Text ops: `#`, `+`, `++`
5. Background ops: `&`, `pstop`, `cont`, `numbg`, `killbp`
6. Cross-terminal: `killallmb`, FIFO producer/consumer

## 8. Pass Criteria
- All functional test cases pass.
- No segmentation fault, no shell hang in unsupported/error input paths.
- Error messages are clear and consistent.
- killallmb and FIFO behavior verified in real multi-terminal setup.
