#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

int main()
{
    int fd;
    char buff1[64];
    char ch;
    off_t filesize;
    int i;

    /* Set umask so final permission becomes 0755 */
    umask(0022);

    /* Create and open lab3.txt with read/write access */
    fd = open("lab3.txt", O_RDWR | O_CREAT | O_TRUNC, 0755);
    if (fd < 0)
    {
        perror("open fail");
        return 1;
    }

    long int n;

    /* Write initial message to file */
    char msg1[] = "Welcome to COMP 8567, University of Windsor";
    n = write(fd, msg1, strlen(msg1));
    if (n < 0)
    {
        perror("write fail");
        close(fd);
        return 1;
    }

    /* Move file offset to start of "University of Windsor" */
    int len = strlen("Welcome to COMP 8567, ");
    lseek(fd, len, SEEK_SET);

    /* Read "University of Windsor" into buff1 */
    len = strlen("University of Windsor");
    read(fd, buff1, len);
    buff1[len] = '\0'; // Null-terminate the string

    /* Move back to the same position */
    len = strlen("Welcome to COMP 8567, ");
    lseek(fd, len, SEEK_SET);

    /* Write "School of Computer Science," */
    char msg2[] = "School of Computer Science, ";
    n = write(fd, msg2, strlen(msg2));
    if (n < 0)
    {
        perror("write fail");
        close(fd);
        return 1;
    }

    /* Write content of buff1 after it */
    n = write(fd, buff1, strlen(buff1));
    if (n < 0)
    {
        perror("write fail");
        close(fd);
        return 1;
    }

    /* Insert '-' between COMP and 8567 */
    len = strlen("Welcome to COMP");
    lseek(fd, len, SEEK_SET);
    n = write(fd, "-", 1);
    if (n < 0)
    {
        perror("write fail");
        close(fd);
        return 1;
    }

    /* Move 10 bytes past end of file */
    lseek(fd, 10, SEEK_END);

    /* Write "Winter 2026" */
    char msg3[] = "Winter 2026";
    n = write(fd, msg3, strlen(msg3));
    if (n < 0)
    {
        perror("write fail");
        close(fd);
        return 1;
    }

    /* Replace all NULL characters with space */
    filesize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    for (i = 0; i < filesize; i++)
    {
        read(fd, &ch, 1);
        if (ch == '\0')
        {                            // If NULL character found
            lseek(fd, -1, SEEK_CUR); // Move back one byte
            ch = ' ';                // Replace with space
            n = write(fd, &ch, 1);
            if (n < 0)
            {
                perror("write fail");
                close(fd);
                return 1;
            }
        }
    }

    /* Close the file */
    close(fd);

    return 0;
}
