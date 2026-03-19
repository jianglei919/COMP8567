#include <netinet/in.h> //structure for storing address information
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> //for socket APIs
#include <sys/types.h>
#include <string.h>

int main(int argc, char *argv[])
{ // E.g., 1, server
    int lis_sd, con_sd, portNumber;
    socklen_t len;
    struct sockaddr_in servAdd; // Socket address object to which IP address and port number are added

    if (argc != 2)
    {
        fprintf(stderr, "Call model: %s <Port#>\n", argv[0]); // Port number is argv[1]
        exit(0);
    }

    // socket() sytem call
    if ((lis_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Could not create socket\n");
        exit(1);
    }
    // Raw socket has been created with lis_sd as the socket descriptor

    // Add port number and IP address to servAdd before invoking the bind() system call
    servAdd.sin_family = AF_INET;
    servAdd.sin_addr.s_addr = htonl(INADDR_ANY); // Add the IP address of the machine
    // htonl: Host to Network Long : Converts host byte order to network byte order
    sscanf(argv[1], "%d", &portNumber);
    servAdd.sin_port = htons((uint16_t)portNumber); // Add the port number entered by the user
    // htons: Host to Network Short : Converts host byte order (of the port number) to network byte order

    // bind() system call
    bind(lis_sd, (struct sockaddr *)&servAdd, sizeof(servAdd));
    // listen

    listen(lis_sd, 5);

    while (1)
    {
        con_sd = accept(lis_sd, (struct sockaddr *)NULL, NULL); // accept()
        // process blocks if there are no connection requests

        char buff[50];
        printf("\nType your message to the client\n");
        scanf("%s", &buff);
        write(con_sd, buff, 50);

        printf("Message from the client\n");
        char buff1[50];
        read(con_sd, buff1, 50);
        printf("%s", buff1);
    }
    close(con_sd);
}
