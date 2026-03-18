#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main()
{
    fprintf(stderr, "isatty(stdin)=%d\n", isatty(0));

    int input_num;
    printf("Please enter a number: ");
    int ret = scanf("%d", &input_num);

    int fdx = open("filex.txt", O_RDONLY);
    int fdy = open("filey.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fdx < 0 || fdy < 0)
    {
        perror("open");
        exit(1);
    }

    int cp0 = dup(0);
    int cp1 = dup(1);
    fprintf(stderr, "cp0=%d cp1=%d\n", cp0, cp1);

    if (dup2(fdx, 0) < 0)
    {
        perror("dup2 stdin");
        exit(1);
    }
    if (dup2(fdy, 1) < 0)
    {
        perror("dup2 stdout");
        exit(1);
    }

    close(fdx);
    close(fdy);

    int a, b;
    int r1 = scanf("%d %d", &a, &b);
    fprintf(stderr, "scanf from filex returned %d (a=%d b=%d)\n", r1, a, b); // 这里是fprintf还是输出到屏幕。 操！！！

    printf("sum=%d\n", a + b);
    fflush(stdout);

    // restore
    int rv0 = dup2(cp0, 0);
    int rv1 = dup2(cp1, 1);
    fprintf(stderr, "restore rv0=%d rv1=%d\n", rv0, rv1);

    close(cp0);
    close(cp1);

    printf("Back to standard input, enter num:\n");
    fflush(stdout);

    int num;
    fprintf(stderr, "About to scanf num...\n");
    int r2 = scanf("%d", &num);

    fprintf(stderr, "after restore isatty(stdin)=%d\n", isatty(0));

    int c = getchar();
    fprintf(stderr, "getchar returned %d\n", c); // EOF is -1
    if (c != EOF)
        ungetc(c, stdin); // 放回去，不影响后续 scanf

    fprintf(stderr, "scanf num returned %d, num=%d\n", r2, num);

    return 0;
}