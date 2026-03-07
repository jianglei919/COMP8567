// ex5server.c
// FIFO server: reads from named pipe "server" and prints to screen.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   // read(), close(), unlink()
#include <fcntl.h>    // open()
#include <sys/stat.h> // mkfifo(), chmod()
#include <errno.h>    // errno
#include <string.h>   // strerror()

#define FIFO_NAME "server"

static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(void)
{
    int fd;
    char ch;

    // 1) Remove old FIFO file if it exists.
    //    If it does not exist, unlink() returns -1 and errno == ENOENT (ignore it).
    if (unlink(FIFO_NAME) == -1 && errno != ENOENT)
    {
        die("unlink");
    }

    // 2) Create FIFO in filesystem.
    //    Note: Actual permissions can be affected by process umask.
    //    We'll chmod() after to be safe (same idea as your slide).
    if (mkfifo(FIFO_NAME, 0777) == -1) // 创建一个FIFO文件，权限0777
    {
        die("mkfifo");
    }

    // 3) Force permissions (in case umask reduced them)
    if (chmod(FIFO_NAME, 0777) == -1) // 设置FIFO文件的权限为0777
    {
        die("chmod");
    }

    printf("[server] FIFO '%s' created. Waiting for a client...\n", FIFO_NAME);
    fflush(stdout);

    // 4) Open FIFO for reading.
    //    IMPORTANT: open(O_RDONLY) will BLOCK until some other process opens FIFO for writing.
    fd = open(FIFO_NAME, O_RDONLY); // 打开FIFO文件的读端，如果没有其他进程打开写端，则会阻塞在这里
    if (fd == -1)
    {
        die("open(O_RDONLY)");
    }

    printf("[server] Client connected. Start receiving data (Ctrl-D in client to end)...\n");
    fflush(stdout);

    // 5) Read one byte at a time and print it.
    //    read() blocks when FIFO is empty but writer is still open.
    //    read() returns 0 (EOF) when ALL writers have closed the FIFO.
    while (1)
    {
        // 循环读取FIFO中的数据，直到所有写端都关闭（read返回0）或发生错误（read返回-1）

        ssize_t n = read(fd, &ch, 1);

        if (n == 1)
        {
            // Print one character
            putchar(ch);

            // Make output visible immediately (avoid "no output until buffer flush" confusion)
            fflush(stdout);
        }
        else if (n == 0)
        {
            // EOF: all write ends are closed
            printf("\n[server] EOF: client closed write end. Server exits.\n");
            break;
        }
        else
        {
            // n == -1: error
            // If interrupted by signal, try again
            if (errno == EINTR)
                continue;

            fprintf(stderr, "[server] read error: %s\n", strerror(errno));
            break;
        }
    }

    close(fd);

    // Optional: keep FIFO file or delete it.
    // If you want to auto-clean:
    // unlink(FIFO_NAME);

    return 0;
}