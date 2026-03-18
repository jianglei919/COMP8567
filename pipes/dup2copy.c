#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main()
{
    printf("isatty(stdin) = %d\n", isatty(0));
    int number1, number2, sum;
    int fdx = open("filex.txt", O_RDONLY); // scanf reads from this file
    int fdy = open("filey.txt", O_RDWR);   // printf writes into

    int cp0 = dup(0); // cp0 additionally refers to STD INPUT
    int cp1 = dup(1); // cp1 additionally refers to STD OUTPUT
    printf("copy stdin & stdout, cp0 = %d, cp1 = %d\n\n", cp0, cp1);

    int ret1 = dup2(fdx, 0);                       // redirect stdin to fdx, 会关闭原来的Standard Input（STDIN_FILENO）
    printf("dup2 for stdin returned: %d\n", ret1); // 返回值是新的文件描述符，即STDIN_FILENO的值：0。输出在屏幕上
    if (ret1 == -1)
    {
        perror("dup2 for stdin");
        exit(1);
    }
    int ret2 = dup2(fdy, 1);                        // redirect stdout to fdy, 会关闭原来的Standard Output（STDOUT_FILENO）
    printf("dup2 for stdout returned: %d\n", ret2); // 返回值是新的文件描述符，即STDOUT_FILENO的值：1。输出到文件中
    if (ret2 < 0)
    {
        perror("dup2 for stdout");
        exit(1);
    }

    scanf("%d %d", &number1, &number2);              // reads number 1 and number2 from filex.txt and not the standard input
    sum = number1 + number2;                         // compute their sum
    printf("The sum of two numbers is\n");           // writes into filex.txt and not the std output
    printf("%d + %d = %d\n", number1, number2, sum); // writes into filex.txt
    fflush(stdout);

    //----------dup2 reversal----------

    // Reversal of Redirection
    int retval1 = dup2(cp0, 0); // restore original STD INPUT
    // int retval1 = dup2(cp0, fdx);
    printf("dup2 for restoring stdin returned: %d\n", retval1); // 需要注意这行代码的位置，不同地方放置会有不同的输出结果（这时候还是输出到文件中）

    int retval2 = dup2(cp1, 1); // restore original STD OUTPUT
    // int retval2 = dup2(cp1, fdy);
    // printf("dup2 for restoring stdin returned: %d\n", retval1); // 放在这里就输出到屏幕
    printf("dup2 for restoring stdout returned: %d\n", retval2);

    printf("\nBack to standard input, enter the value of num\n");
    fflush(stdout);
    int num;
    scanf("%d", &num); //??? 程序直接结束了，没有等待终端输入

    return 0;
}