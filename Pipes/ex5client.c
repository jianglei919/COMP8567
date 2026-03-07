// ex5client.c
// FIFO client: writes keyboard input into named pipe "server".
// Type text, press Enter, etc.
// Press Ctrl-D to send EOF and exit client.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // write(), close(), sleep()
#include <fcntl.h>  // open()
#include <errno.h>  // errno
#include <string.h> // strerror()
#include <signal.h> // signal(), SIGPIPE

#define FIFO_NAME "server"

static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(void)
{
    int fd;

    // If server exits early and client writes to FIFO without reader,
    // OS may send SIGPIPE and terminate the client by default.
    // For learning/debugging, we can ignore SIGPIPE and handle write errors manually.
    signal(SIGPIPE, SIG_IGN); // 忽略SIGPIPE信号，防止写入已关闭的FIFO时程序被终止

    // 1) Keep trying to open FIFO for writing until server is ready.
    //    Typical cases:
    //    - FIFO doesn't exist yet -> open fails with ENOENT
    //    - Server not opened read end yet -> open(O_WRONLY) may block on some systems
    //
    // We use open with O_WRONLY:
    //   - If FIFO doesn't exist -> fail and retry
    //   - If FIFO exists but no reader -> open may BLOCK (depends)
    //
    // If you want non-blocking open, use O_WRONLY | O_NONBLOCK, but then
    // open will fail with ENXIO if no reader. For beginners, keep it simple.
    while (1)
    {
        fd = open(FIFO_NAME, O_WRONLY); // 尝试以写模式打开FIFO文件，如果没有服务器打开读端，则可能会阻塞在这里
        if (fd != -1)
            break;

        printf("[client] Trying to connect to server... (%s)\n", strerror(errno));
        fflush(stdout);
        sleep(1);
    }

    printf("[client] Connected. Type text to send. Press Ctrl-D to quit.\n");
    fflush(stdout);

    // 2) Read from stdin char-by-char and write into FIFO.
    // getchar() returns int, must store into int to detect EOF (-1).
    while (1)
    {
        int c = getchar();
        if (c == EOF)
        {
            // Ctrl-D in terminal (EOF)
            printf("\n[client] EOF from keyboard. Closing FIFO write end...\n");
            fflush(stdout);
            break;
        }

        char ch = (char)c;

        // write one byte into FIFO
        ssize_t w = write(fd, &ch, 1);
        if (w == 1)
        {
            // ok
        }
        else if (w == -1 && errno == EINTR)
        {
            // interrupted, retry this character
            continue;
        }
        else
        {
            // If server closed read end, write may fail with EPIPE (and SIGPIPE if not ignored)
            fprintf(stderr, "[client] write error: %s\n", strerror(errno));
            break;
        }
    }

    close(fd);
    printf("[client] Done.\n");
    return 0;
}