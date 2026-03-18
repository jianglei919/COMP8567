#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main()
{
    int number1, number2, sum;

    int fdx = open("filex.txt", O_RDONLY); // scanf reads from this file
    int fdy = open("filey.txt", O_RDWR);   // printf writes into this file

    // 如果文件不存在，则按默认Standard I/O处理
    int ret1 = dup2(fdx, STDIN_FILENO);            // redirect stdin to fdx, 会关闭原来的Standard Input（STDIN_FILENO）
    printf("dup2 for stdin returned: %d\n", ret1); // 返回值是新的文件描述符，即STDIN_FILENO的值：0
    if (ret1 == -1)
    {
        perror("dup2 for stdin");
        exit(1);
    }

    int ret2 = dup2(fdy, STDOUT_FILENO);            // redirect stdout to fdy, 会关闭原来的Standard Output（STDOUT_FILENO）
    printf("dup2 for stdout returned: %d\n", ret2); // 返回值是新的文件描述符，即STDOUT_FILENO的值：1
    if (ret2 < 0)
    {
        perror("dup2 for stdout");
        exit(1);
    }

    // fdx and fdy are no longer needed after dup2
    // close(fdx);
    // close(fdy);

    scanf("%d %d", &number1, &number2);              // reads number 1 and number2 from filex.txt and not the standard input
    sum = number1 + number2;                         // compute their sum
    printf("The sum of two numbers is\n");           // writes into filex.txt and not the std output
    printf("%d + %d = %d\n", number1, number2, sum); // writes into filex.txt

    return 0;
}