#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define BUFFERSIZE 256
#define MAXRULE 100
#define MAXQUERY 100
#define MAXREQ 100

typedef struct 
{
    char ipAddress[BUFFERSIZE];
    int portNum;
} Query;

typedef struct 
{
    struct in_addr beginIp;
    struct in_addr endIp;
    int beginPort;
    int endPort;
    Query queryList[MAXQUERY];
    int queryTally;
} NetworkRule;

bool parseConnectionDetails(const char *connectData, struct in_addr *ipStruct, int *port);
bool validateAgainstRules(struct in_addr ipAddr, int portNum, int cSocket);
void composeRuleDescription(char *buffer, NetworkRule *rule, const char *startIP, const char *endIP);

void displayQuery(int clientSocket, NetworkRule *rule, char *buffer);
void listRuleQuery(int clientSocket);
bool parseRule(const char *ruleStr, char *ipSegment, char *portSegment);

bool validateRule(const char *ipSegment, const char *portSegment, struct in_addr *startIp, struct in_addr *endIp, int *beginPort, int *finalPort);

bool removeRule(struct in_addr startIp, struct in_addr endIp, int start_port, int endPort, int cDescriptor);

void handleDelete(int cDescriptor, const char *args);
void handleVerify(int cDescriptor, const char *args);
void handleAddRule(int cDescriptor, const char *args);
void handleListRequests(int cDescriptor, const char *args);
void handleListRules(int cDescriptor, const char *args);
void handleInvalidRequest(int cDescriptor);

int reqTally = 0;
int ruleTally = 0;

pthread_mutex_t ruleLock = PTHREAD_MUTEX_INITIALIZER;

typedef struct 
{
    char cmdText[BUFFERSIZE];
} ClientRequest;


typedef struct {
    const char *commandPrefix;
    void (*handler)(int, const char*);
} CommandHandler;

CommandHandler commandHandlers[] = {
    {"D ", handleDelete},
    {"C ", handleVerify},
    {"A ", handleAddRule},
    {"R", handleListRequests},
    {"L", handleListRules}
};

NetworkRule ruleArr[MAXRULE];
ClientRequest reqArr[MAXREQ];

void trimNewline(char *str) {
    char *newline = strchr(str, '\n');
    if (newline) {
        *newline = '\0';
    }
}

void listReq(int clientSckt) 
{
    char terminal[BUFFERSIZE] = "List of requests:\n";
    int currentLen= strlen(terminal);

    for (int i = 0; i < reqTally && (currentLen < BUFFERSIZE); i++) 
    {
        int spaceLeft = BUFFERSIZE - currentLen - 1; 
        if (spaceLeft > 0) {
            strncat(terminal + currentLen, reqArr[i].cmdText, spaceLeft);
            currentLen += strlen(reqArr[i].cmdText);
        }
        if (spaceLeft > 1) {
            strncat(terminal + currentLen, "\n", spaceLeft - 1);
            currentLen += 1;
        }
        strcat(terminal, reqArr[i].cmdText);
        strcat(terminal, "\n");
    }
    write(clientSckt, terminal, currentLen);
}

bool convertIp(struct in_addr *ipStart, struct in_addr *ipFinish, const char *ipData) 
{
    char ipBuffer[BUFFERSIZE];
    strcpy(ipBuffer, ipData);

    char *firstPartIP = strtok(ipBuffer, "-");
    char *secondPartIp = strtok(NULL, "-");

    if (firstPartIP && secondPartIp) {
        return inet_pton(AF_INET, firstPartIP, ipStart) && inet_pton(AF_INET, secondPartIp, ipFinish);
    } else if (firstPartIP) {
        return inet_pton(AF_INET, firstPartIP, ipStart) && inet_pton(AF_INET, firstPartIP, ipFinish);
    } else {
        return false;
    }
}

bool convertPort(int *start, int *end, const char *portData) 
{
    char portBuffer[20];
    int index = 0;
    bool foundHyph= false;

    while (*portData != '\0' && index < 19) 
    {
        if (*portData == '-') 
        {
            foundHyph = true;
            portBuffer[index] = '\0';
            *start = atoi(portBuffer);
            index = 0;
            portData++;
            continue;
        }
        portBuffer[index++] = *portData;
        portData++;
    }
    
    portBuffer[index] = '\0';
    
    if (foundHyph) {
        *end = atoi(portBuffer);
    } else {
        *start = *end = atoi(portBuffer);
    }

    return *start <= *end && *start >= 0 && *end <= 65535;
}

void addReq(const char *cmd) 
{
    if (reqTally >= MAXREQ) {
        printf("Request limit reached. We cannot add more requests.\n");
        return;
    }
    strncpy(reqArr[reqTally].cmdText, cmd, BUFFERSIZE - 1);
    reqArr[reqTally].cmdText[BUFFERSIZE - 1] = '\0';
    reqTally++;
}

void registerNewRule (int cDescriptor, const char *inputStr) 
{
    if (ruleTally >= MAXRULE) {
        write(cDescriptor, "Rule limit reached\n", 19);
        return;
    }

    NetworkRule rule;
    memset(&rule, 0, sizeof(NetworkRule));

    const char *space_pos = strchr(inputStr, ' ');
    if (!space_pos) {
        write(cDescriptor, "Invalid rule\n", 14);
        return;
    }

    char ipSegment[BUFFERSIZE] = {0};
    char portSegment[BUFFERSIZE] = {0};
    size_t ipSegmentLength = space_pos - inputStr;
    if (ipSegmentLength >= BUFFERSIZE) {
        ipSegmentLength = BUFFERSIZE - 1;
    }
    strncpy(ipSegment, inputStr, ipSegmentLength);

    size_t port_length = strlen(space_pos + 1);
    if (port_length >= BUFFERSIZE) port_length = BUFFERSIZE - 1;
    strncpy(portSegment, space_pos + 1, port_length);

    if (!convertIp(&rule.beginIp, &rule.endIp, ipSegment) ||
        !convertPort(&rule.beginPort, &rule.endPort, portSegment)) {
        write(cDescriptor, "Invalid rule\n", 14);
        return;
    }

    rule.queryTally = 0;

    pthread_mutex_lock(&ruleLock);
    ruleArr[ruleTally++] = rule;
    pthread_mutex_unlock(&ruleLock);

    write(cDescriptor, "Rule added\n", 11);
    fflush(stdout);  
}

bool ipValidator(struct in_addr ipFirst, struct in_addr ipCurr, struct in_addr ipFinal) 
{
    unsigned long ipNum = ntohl(ipCurr.s_addr);
    unsigned long startNum = ntohl(ipFirst.s_addr);
    unsigned long endNum = ntohl(ipFinal.s_addr);

    bool withinStart = ipNum >= startNum;
    bool withinEnd = ipNum <= endNum;

    bool withinRange = (ipNum - startNum) <= (endNum - startNum);

    return withinStart && withinEnd && withinRange;
}

bool verifyConn(int cDescriptor, const char *connectStr) 
{
    struct in_addr networkAddr;
    int portNum;

    if (!parseConnectionDetails(connectStr, &networkAddr, &portNum)) {
        const char *errMsg = "Illegal IP address or port specified\n";
        write(cDescriptor, errMsg, strlen(errMsg));
        return false;
    }

    return validateAgainstRules(networkAddr, portNum, cDescriptor);
}

bool parseConnectionDetails(const char *connectData, struct in_addr *ipStruct, int *port) 
{
    char dataBuffer[BUFFERSIZE];
    char ipAddr[BUFFERSIZE];

    strncpy(dataBuffer, connectData, BUFFERSIZE - 1);
    dataBuffer[BUFFERSIZE - 1] = '\0';

    char *tok = strtok(dataBuffer, " ");
    if (tok == NULL) {
        return false;
    }
    strncpy(ipAddr, tok, BUFFERSIZE);

    tok = strtok(NULL, " ");

    if (tok == NULL || sscanf(tok, "%d", port) != 1) {
        return false;
    }
    if (!inet_pton(AF_INET, ipAddr, ipStruct) || *port < 0 || *port > 65535) {
        return false;
    }
    return true;
}

bool validateAgainstRules(struct in_addr ipAddr, int portNum, int clntSckt) 
{
    pthread_mutex_lock(&ruleLock);

    bool accessGranted = false;

    for (int index = 0; index < ruleTally; index++) 
    {
        NetworkRule *currRule = &ruleArr[index];

        if (ntohl(ipAddr.s_addr) >= ntohl(currRule->beginIp.s_addr) &&
            ntohl(ipAddr.s_addr) <= ntohl(currRule->endIp.s_addr) &&
            portNum >= currRule->beginPort && 
            portNum <= currRule->endPort) {
            if (currRule->queryTally < MAXQUERY) {
                strncpy(currRule->queryList[currRule->queryTally].ipAddress, inet_ntoa(ipAddr), BUFFERSIZE - 1);
                currRule->queryList[currRule->queryTally].portNum = portNum;
                currRule->queryTally++;
            }
            accessGranted = true;
            break;
        }
    }

    pthread_mutex_unlock(&ruleLock);
    if (accessGranted) {
        write(clntSckt, "Connection accepted\n", 20);
        return true;
    } else {
        write(clntSckt, "Connection rejected\n", 20);
        return false;
    }
}

void listRuleQuery(int clientSocket) 
{
    char outputBuffer[BUFFERSIZE];
    memset(outputBuffer, 0, BUFFERSIZE);
    
    pthread_mutex_lock(&ruleLock);

    if (ruleTally == 0) 
    {
        const char *noRulesMsg = "No rules found.\n";
        send(clientSocket, noRulesMsg, strlen(noRulesMsg), 0);
        pthread_mutex_unlock(&ruleLock);
        return;
    }

    for (int index = 0; index < ruleTally; index++) 
    {
        NetworkRule *rule = &ruleArr[index];
        char startIP[INET_ADDRSTRLEN], endIP[INET_ADDRSTRLEN];
        
        inet_ntop(AF_INET, &(rule->beginIp), startIP, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(rule->endIp), endIP, INET_ADDRSTRLEN);

        composeRuleDescription(outputBuffer, rule, startIP, endIP);

        send(clientSocket, outputBuffer, strlen(outputBuffer), 0);

        displayQuery(clientSocket, rule, outputBuffer);
    }

    pthread_mutex_unlock(&ruleLock);
}

void composeRuleDescription(char *buffer, NetworkRule *rule, const char *startIP, const char *endIP) 
{
    if (memcmp(&rule->beginIp, &rule->endIp, sizeof(struct in_addr)) == 0) 
    {
        snprintf(buffer, BUFFERSIZE, "Rule: %s %d", startIP, rule->beginPort);
        if (rule->beginPort != rule->endPort) {
            snprintf(buffer + strlen(buffer), BUFFERSIZE - strlen(buffer), "-%d", rule->endPort);
        }
    } else {
        snprintf(buffer, BUFFERSIZE, "Rule: %s-%s %d", startIP, endIP, rule->beginPort);
        if (rule->beginPort != rule->endPort) {
            snprintf(buffer + strlen(buffer), BUFFERSIZE - strlen(buffer), "-%d", rule->endPort);
        }
    }
    strncat(buffer, "\n", BUFFERSIZE - strlen(buffer) - 1);
}

void displayQuery(int clientSocket, NetworkRule *rule, char *buffer) 
{
    for (int i = 0; i < rule->queryTally; i++) 
    {
        memset(buffer, 0, BUFFERSIZE);
        
        int maxIpLength = BUFFERSIZE - 50; 
        int actualIpLength = strlen(rule->queryList[i].ipAddress);

        if (actualIpLength > maxIpLength) {
            actualIpLength = maxIpLength; 
        }
        
        snprintf(buffer, BUFFERSIZE, "Query: %.*s %d\n",
                 actualIpLength, rule->queryList[i].ipAddress, rule->queryList[i].portNum);
        send(clientSocket, buffer, strlen(buffer), 0);
    }
}

bool ruleDeletion(int cDescriptor, const char *ruleStr) 
{
    char ipSegment[BUFFERSIZE], portSegment[BUFFERSIZE];
    struct in_addr beginIp, endIp;
    int beginPort, endPort;

    if (!parseRule(ruleStr, ipSegment, portSegment) ||
        !validateRule(ipSegment, portSegment, &beginIp, &endIp, &beginPort, &endPort)) {
        write(cDescriptor, "Rule invalid\n", strlen("Rule invalid\n"));
        return false;
    }

    if (!removeRule(beginIp, endIp, beginPort, endPort, cDescriptor)) {
        write(cDescriptor, "Rule not found\n", strlen("Rule not found\n"));
        return false;
    }

    write(cDescriptor, "Rule deleted\n", strlen("Rule deleted\n"));
    return true;
}


bool parseRule(const char *ruleStr, char *ipSegment, char *portSegment) 
{
    return sscanf(ruleStr, "%s %s", ipSegment, portSegment) == 2;
}

bool validateRule(const char *ipSegment, const char *portSegment, 
struct in_addr *beginIp, struct in_addr *endIp, int *beginPort, int *endPort) 
{
    return (inet_pton(AF_INET, ipSegment, beginIp) && inet_pton(AF_INET, ipSegment, endIp) &&
            sscanf(portSegment, "%d-%d", beginPort, endPort) == 2 && *beginPort <= *endPort);
}

bool removeRule(struct in_addr beginIp, struct in_addr endIp, int beginPort, int endPort, int cDescriptor) 
{
    pthread_mutex_lock(&ruleLock);

    for (int i = 0; i < ruleTally; ++i) 
    {
        if (memcmp(&ruleArr[i].beginIp, &beginIp, sizeof(struct in_addr)) == 0 &&
            memcmp(&ruleArr[i].endIp, &endIp, sizeof(struct in_addr)) == 0 &&
            ruleArr[i].beginPort == beginPort && ruleArr[i].endPort == endPort) {
            
            memmove(&ruleArr[i], &ruleArr[i + 1], (ruleTally - i - 1) * sizeof(NetworkRule));
            ruleTally--;

            pthread_mutex_unlock(&ruleLock);
            return true;
        }
    }

    pthread_mutex_unlock(&ruleLock);
    return false;
}

void handleDelete(int cDescriptor, const char *args) {
    ruleDeletion(cDescriptor, args);
}

void handleVerify(int cDescriptor, const char *args) {
    verifyConn(cDescriptor, args);
}

void handleAddRule(int cDescriptor, const char *args) {
    registerNewRule(cDescriptor, args);
}

void handleListRequests(int cDescriptor, const char *args) {
    listReq(cDescriptor);
}

void handleListRules(int cDescriptor, const char *args) {
    listRuleQuery(cDescriptor);
}

void handleInvalidRequest(int cDescriptor) 
{
    const char *illMsg = "Illegal request\n";
    write(cDescriptor, illMsg, strlen(illMsg));
}

void *clntOrg(void *scktPtr) 
{
    int cDescriptor = *(int *)scktPtr;

    free(scktPtr);

    char buffer[BUFFERSIZE] = {0};
    read(cDescriptor, buffer, BUFFERSIZE - 1);
    trimNewline(buffer);

    addReq(buffer); 

    size_t numHandlers = sizeof(commandHandlers) / sizeof(commandHandlers[0]);
    for (size_t i = 0; i < numHandlers; ++i) 
    {
        if (strncmp(buffer, commandHandlers[i].commandPrefix, strlen(commandHandlers[i].commandPrefix)) == 0) 
        {
            commandHandlers[i].handler(cDescriptor, buffer + strlen(commandHandlers[i].commandPrefix));
            close(cDescriptor);
            return NULL;
        }
    }
    handleInvalidRequest(cDescriptor);
    close(cDescriptor);
    return NULL;
}

void initServerNet(int portNum) 
{
    int netSckt;
    struct sockaddr_in netAddr;
    socklen_t netAddrSize = sizeof(netAddr);
    int opt = 1;

    netSckt = socket(AF_INET, SOCK_STREAM, 0);
    if (netSckt == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(netSckt, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    netAddr.sin_family = AF_INET;
    netAddr.sin_addr.s_addr = INADDR_ANY;
    netAddr.sin_port = htons(portNum);

    if (bind(netSckt, (struct sockaddr *)&netAddr, netAddrSize) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(netSckt, 3) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // printf("Server is now listening on port %d\n", portNum);

    while (true) 
    {
        int *clntScktPtr = malloc(sizeof(int));

        if (!clntScktPtr) {
            perror("Failed to allocate memory for client socket");
            continue;
        }

        *clntScktPtr = accept(netSckt, (struct sockaddr *)&netAddr, &netAddrSize);
        if (*clntScktPtr == -1) {
            perror("Accept failed");
            free(clntScktPtr);
            continue;
        }

        pthread_t threadId;
        if (pthread_create(&threadId, NULL, clntOrg, clntScktPtr) != 0) {
            perror("Failed to create thread");
            close(*clntScktPtr);
            free(clntScktPtr);
            continue;
        }
        
        pthread_detach(threadId);
    }
    close(netSckt);
}

void interactive() 
{
    char tempBuffer[BUFFERSIZE];
    bool requestProcessed;

    while (1) {
        requestProcessed = false;
        fflush(stdout);

        if (fgets(tempBuffer, BUFFERSIZE, stdin) == NULL) {
            break;
        }

        tempBuffer[strcspn(tempBuffer, "\n")] = '\0'; 
        
        if (reqTally < MAXREQ) {
            strncpy(reqArr[reqTally].cmdText, tempBuffer, BUFFERSIZE - 1);
            reqArr[reqTally].cmdText[BUFFERSIZE - 1] = '\0';
            reqTally++;
        } else {
            printf("Request limit reached. No more requests can be added.\n");
            continue;
        }

        if (strcmp(tempBuffer, "exit") == 0) {
            break;
        }

        for (int i = 0; i < sizeof(commandHandlers) / sizeof(commandHandlers[0]); i++) {
            if (strncmp(tempBuffer, commandHandlers[i].commandPrefix, 
                strlen(commandHandlers[i].commandPrefix)) == 0) {
                commandHandlers[i].handler(STDOUT_FILENO, tempBuffer + strlen(commandHandlers[i].commandPrefix));
                requestProcessed = true;
                break;
            }
        }

        if (!requestProcessed) {
            handleInvalidRequest(STDOUT_FILENO);
        }

        fflush(stdout);
    }
}

int validatePort(const char *str) 
{
    int portNum = atoi(str);
    if (portNum <= 0) {
        fprintf(stderr, "Invalid port number.\n");
        return -1;
    }
    return portNum;
}

int main(int argc, char **argv) 
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s -i (for interactive mode) or %s <port> (for network mode)\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    void (*modeFn)(int) = NULL;
    int portNum;

    if (strcmp(argv[1], "-i") == 0) {
        modeFn = (void (*)(int))interactive;
        portNum = 0;  
    } else {
        portNum = validatePort(argv[1]);
        if (portNum == -1) {
            return EXIT_FAILURE;
        }
        modeFn = initServerNet;
    }

    modeFn(portNum);
    return EXIT_SUCCESS;
}