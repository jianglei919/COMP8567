#include <netinet/in.h> //structure for storing address information
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> //for socket APIs
#include <sys/types.h>
#include <string.h>

int main(int argc, char *argv[])
{ // E.g., 1, client
    char message[100];
    int client_sd, portNumber;
    socklen_t len;
    struct sockaddr_in servAdd;

    if (argc != 3)
    {
        printf("Call model:%s <IP> <Port#>\n", argv[0]);
        exit(0);
    }

    if ((client_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    { // socket()
        fprintf(stderr, "Cannot create socket\n");
        exit(1);
    }

    // ADD the server's PORT NUMBER AND IP ADDRESS TO THE sockaddr_in object
    servAdd.sin_family = AF_INET; // Internet
    sscanf(argv[2], "%d", &portNumber);
    servAdd.sin_port = htons((uint16_t)portNumber); // Host to network short (htons)
    // htons is used to convert host byte order into network byte order

    // inet_pton() is used to convert the IP address in text into binary
    // internet presentation to numeric
    if (inet_pton(AF_INET, argv[1], &servAdd.sin_addr) < 0)
    {
        fprintf(stderr, " inet_pton() has failed\n");
        exit(2);
    }

    if (connect(client_sd, (struct sockaddr *)&servAdd, sizeof(servAdd)) < 0)
    { // Connect()
        fprintf(stderr, "connect() failed, exiting\n");
        exit(3);
    }

    if (read(client_sd, message, 100) < 0)
    { // read()
        fprintf(stderr, "read() error\n");
        exit(3);
    }

    fprintf(stdout, "%s\n", message);

    char buff[50];
    printf("\nEnter the message to be sent to the server\n");
    scanf("%s", &buff);

    write(client_sd, buff, 100);

    exit(0);
}
