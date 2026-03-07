#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main()
{
    char msg1[] = "COMP 8517\n";
    char inbuf[20];
    int p[2];

    int fd = pipe(p); // 成功返回0，失败返回-1
    printf("pipe() returned: %d\n", fd);
    if (fd == -1)
    {
        perror("pipe");
        exit(1);
    }
    printf("pipe created: p[0]=%d, p[1]=%d\n", p[0], p[1]);

    int len = strlen(msg1);
    printf("Writing len= %d ,msg1: %s\n", len, msg1);

    ssize_t n = write(p[1], msg1, len);
    if (n != len)
    {
        perror("write");
        exit(1);
    }

    close(p[1]); // IMPORTANT: allow reader to see EOF after data is consumed. 不关闭写端，读端会一直阻塞等待数据。

    ssize_t m;

    m = read(p[0], inbuf, 10); // leave space for null terminator
    printf("Read %zd bytes: %s\n", m, inbuf);

    m = read(p[0], inbuf, 10); // read the remaining data
    printf("Read again %zd\n", m);

    close(p[0]);

    // while ((m = read(p[0], inbuf, 10)) > 0)
    // {
    //     inbuf[m] = '\0'; // null-terminate the string
    //     printf("Read %zd bytes: %s", m, inbuf);
    // }
    // if (m < 0)
    // {
    //     perror("read");
    //     close(p[0]);
    // }

    return 0;
}