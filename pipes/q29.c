#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/**
 * 	•	读端开着，但暂时没人读 → 可能阻塞（缓冲区满时）
 * 	•	读端全关了 → 不阻塞，直接出错 / SIGPIPE
 */
int main()
{
    /**
     * 如果一个 pipe 的所有读端都关闭了，再往写端写：write(fd[1], buf, n); 会发生：
     *  • 内核给进程发送 SIGPIPE
     *  • 默认行为是终止进程
     *  • 如果忽略/捕获了 SIGPIPE，那 write() 会返回 -1
     *  • 同时 errno = EPIPE
     */
    int fd[2];
    printf("Hello1\n");
    pipe(fd);
    close(fd[0]); // 关闭读端
    printf("Hello2\n");
    write(fd[1], "Hello2", 6); // 因为fd[0]关闭了，如果继续向没有读者的 pipe 写数据，那么这个write会触发SIGPIPE信号，导致程序被终止
    printf("Hello3\n");
    return 0;
}