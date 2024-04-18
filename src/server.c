#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define SERVER_PORT 8888
#define MAX_CLIENTS 10
#define CONNECTION_REQUEST "CONNECT"
#define MAX_MSG_SIZE 1024
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 24
#define ACK_MSG "ACK"

typedef struct
{
    int x;
    int y;
} Position;

typedef struct
{
    struct sockaddr_in addr;
    int                sockfd;
    int                port;
    bool               active;
} ClientInfo;

typedef struct
{
    int                clientSock;
    struct sockaddr_in clientAddr;
    pthread_mutex_t   *broadcastMutex;
    Position          *currentPosition;
    int               *broadcastFlag;
    pthread_cond_t    *condition;

} ClientThreadArgs;

int        clientCount;
int        messageID;
ClientInfo clientList[MAX_CLIENTS];    // Array to hold client information
int        clientSocket[MAX_CLIENTS];
clientCount = 0;
time_t lastBroadcastTime;
messageID   = 0;
clientCount = 0;

void  broadCastToClients(char *message);
void *clientHandler(void *arg);

void broadCastToClients(char *message)
{
    char buffer[MAX_MSG_SIZE];
    for(int i = 0; i < clientCount; i++)
    {
        printf("Broadcasting to client %d\n", clientList[i].sockfd);
        printf("Message: %s\n", message);
        sprintf(buffer, "%d %s", messageID, message);
        sendto(clientList[i].sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&clientList[i].addr, sizeof(clientList[i].addr));
    }
    messageID++;
};

void *clientHandler(void *arg)
{
    ClientThreadArgs  *threadArgs      = (ClientThreadArgs *)arg;
    int                clientSock      = threadArgs->clientSock;
    struct sockaddr_in clientAddr      = threadArgs->clientAddr;
    pthread_mutex_t   *broadcastMutex  = threadArgs->broadcastMutex;
    Position          *currentPosition = threadArgs->currentPosition;
    int               *broadcastFlag   = threadArgs->broadcastFlag;
    pthread_cond_t    *condition       = threadArgs->condition;

    printf("HELLO\n");

    char buffer[MAX_MSG_SIZE];

    struct sockaddr_in localAddr;
    socklen_t          addrLen = sizeof(localAddr);
    getsockname(clientSock, (struct sockaddr *)&localAddr, &addrLen);

    printf("Thread is listening on port: %d\n", ntohs(localAddr.sin_port));

    while(1)
    {
        memset(buffer, 0, MAX_MSG_SIZE);
        ssize_t bytesReceived = recv(clientSock, buffer, MAX_MSG_SIZE, 0);
        if(bytesReceived <= 0)
        {
            // Handle disconnection or error
            close(clientSock);
            pthread_exit(NULL);
        }

        printf("Thread: Client: %s\n", buffer);

        // Process the received command (assuming it's a string command)
        // For example, if the command is "BROADCAST", set the broadcast flag

        // pthread_mutex_lock(broadcastMutex);
        if(strcmp(buffer, "UP") == 0)
        {
            currentPosition->y = (currentPosition->y - 1 + SCREEN_HEIGHT) % SCREEN_HEIGHT;
        }
        else if(strcmp(buffer, "DOWN") == 0)
        {
            currentPosition->y = (currentPosition->y + 1) % SCREEN_HEIGHT;
        }
        else if(strcmp(buffer, "LEFT") == 0)
        {
            currentPosition->x = (currentPosition->x - 1 + SCREEN_WIDTH) % SCREEN_WIDTH;
        }
        else if(strcmp(buffer, "RIGHT") == 0)
        {
            currentPosition->x = (currentPosition->x + 1) % SCREEN_WIDTH;
        }
        else if(strstr(buffer, ACK_MSG) != NULL)
        {
            // Acknowledgement received
            int  id;
            char message[MAX_MSG_SIZE];
            sscanf(buffer, "%d %s", &id, message);
            printf("Acknowledgement received from client for message: %d\n", id);
        }
        // *broadcastFlag = 1;
        // pthread_cond_signal(condition);  // Signal the condition variable
        // pthread_mutex_unlock(broadcastMutex);

        sprintf(buffer, "%d %d X", currentPosition->x, currentPosition->y);
        broadCastToClients(buffer);
        lastBroadcastTime = time(NULL);
    }

    pthread_exit(NULL);
}

int main()
{
    int                serverSocket = 0;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t          addrLen = sizeof(clientAddr);
    char               buffer[1024];

    Position currentPosition;

    // Threading
    pthread_mutex_t broadcastMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  condition      = PTHREAD_COND_INITIALIZER;

    int broadcastFlag = 0;

    // Create main server socket
    serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if(serverSocket < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port        = htons(SERVER_PORT);

    // Bind server socket
    if(bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    printf("Server started, waiting for clients...\n");
    currentPosition.x = SCREEN_WIDTH / 2;
    currentPosition.y = SCREEN_HEIGHT / 2;

    while(1)
    {
        // Accept incoming connections
        int bytesReceived;
        bytesReceived = recvfrom(serverSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddr, &addrLen);
        if(bytesReceived < 0)
        {
            perror("Receive failed");
            exit(EXIT_FAILURE);
        }

        buffer[bytesReceived] = '\0';
        printf("Received message from client: %s\n", buffer);

        // Create a new socket for the client
        clientSocket[clientCount] = socket(AF_INET, SOCK_DGRAM, 0);
        if(clientSocket[clientCount] < 0)
        {
            perror("Client socket creation failed");
            exit(EXIT_FAILURE);
        }

        // Assign a unique port number for the client socket
        struct sockaddr_in clientSocketAddr;
        memset(&clientSocketAddr, 0, sizeof(clientSocketAddr));
        clientSocketAddr.sin_family      = AF_INET;
        clientSocketAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        clientSocketAddr.sin_port        = htons(SERVER_PORT + clientCount + 1);    // Example: Incrementing port numbers

        // Bind the client socket to the assigned port
        if(bind(clientSocket[clientCount], (struct sockaddr *)&clientSocketAddr, sizeof(clientSocketAddr)) < 0)
        {
            perror("Client socket binding failed");
            exit(EXIT_FAILURE);
        }

        printf("Client %d connected on port %d\n", clientCount + 1, SERVER_PORT + clientCount + 1);
        clientCount++;

        clientList[clientCount - 1].sockfd = clientSocket[clientCount - 1];
        clientList[clientCount - 1].port   = SERVER_PORT + clientCount;
        clientList[clientCount - 1].addr   = clientAddr;
        clientList[clientCount - 1].active = true;

        if(strcmp(buffer, CONNECTION_REQUEST) == 0)
        {
            // Send connection established message to client
            char portMessage[1024];
            int  newPort = SERVER_PORT + clientCount;
            sprintf(portMessage, "%d", newPort);
            sendto(serverSocket, portMessage, strlen(portMessage), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
        }

        ClientThreadArgs threadArgs;
        threadArgs.clientSock      = clientSocket[clientCount - 1];
        threadArgs.clientAddr      = clientAddr;
        threadArgs.broadcastMutex  = &broadcastMutex;
        threadArgs.broadcastFlag   = &broadcastFlag;
        threadArgs.currentPosition = &currentPosition;
        threadArgs.condition       = &condition;

        pthread_t clientThread;
        pthread_create(&clientThread, NULL, clientHandler, (void *)&threadArgs);
    }

    // Close sockets and perform cleanup
    for(int i = 0; i < clientCount; i++)
    {
        close(clientSocket[i]);
    }
    close(serverSocket);

    return 0;
}
