#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAXBUFFERLEN 256

void errHandle(const char *errMsg) 
{
    perror(errMsg);
    exit(EXIT_FAILURE);
}

int init_sckt() 
{
    int sckt = socket(AF_INET, SOCK_STREAM, 0);
    if (sckt < 0) {
        errHandle("Error creating socket");
    }
    return sckt;
}

void configAddr(struct sockaddr_in *serverAddr, const char *hostAddr, int portNum) 
{
    memset(serverAddr, 0, sizeof(*serverAddr));
    serverAddr->sin_family = AF_INET;
    serverAddr->sin_port = htons(portNum);

    if (inet_pton(AF_INET, hostAddr, &serverAddr->sin_addr) <= 0) {
        fprintf(stderr, "Invalid or unsupported address\n");
        exit(EXIT_FAILURE);
    }
}

void buildCommand(char *cmdBuffer, int argCount, char *argVal[]) 
{
    snprintf(cmdBuffer, MAXBUFFERLEN - 1, "%s", argVal[3]);
    for (int index = 4; index < argCount; index++) 
    {
        strncat(cmdBuffer, " ", MAXBUFFERLEN - strlen(cmdBuffer) - 1);
        strncat(cmdBuffer, argVal[index], MAXBUFFERLEN - strlen(cmdBuffer) - 1);
    }
    strncat(cmdBuffer, "\n", MAXBUFFERLEN - strlen(cmdBuffer) - 1);
}

void verifyConn(int scktFd, struct sockaddr_in *serverAddr) 
{
    if (connect(scktFd, (struct sockaddr *)serverAddr, sizeof(*serverAddr)) < 0) {
        errHandle("Connection failed");
    }
}

void sendCommand(int scktFd, const char *cmdBuffer) 
{
    if (send(scktFd, cmdBuffer, strlen(cmdBuffer), 0) < 0) {
        errHandle("Transmission error");
    }
}

void fetch(int scktFd) 
{
    char buffer[MAXBUFFERLEN] = {0};
    int bytes = read(scktFd, buffer, MAXBUFFERLEN - 1);

    if (bytes < 0) {
        errHandle("Error receiving data");
    }
    buffer[bytes] = '\0';
    printf("%s", buffer);
}

int main(int argTally, char *argVal[]) 
{
    if (argTally < 4) {
        fprintf(stderr, "Usage: %s <host_address> <port_number> <command> [parameters]\n", argVal[0]);
        return EXIT_FAILURE;
    }

    const char *hostAddr = (strcmp(argVal[1], "localhost") == 0) ? "127.0.0.1" : argVal[1];
    int portNum = atoi(argVal[2]);

    char cmdBuffer[MAXBUFFERLEN] = {0};
    buildCommand(cmdBuffer, argTally, argVal);

    int scktFd = init_sckt();

    struct sockaddr_in serverAddr;
    configAddr(&serverAddr, hostAddr, portNum);

    verifyConn(scktFd, &serverAddr);

    sendCommand(scktFd, cmdBuffer);
    fetch(scktFd);

    close(scktFd);
    return 0;
}