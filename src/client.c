#include <arpa/inet.h>
#include <ncurses.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8888
#define MAX_MSG_SIZE 1024
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 24
#define ACK_MSG "ACK"
#define CONNECTED "CONNECTED"
#define BUFFER_SIZE 1024
#define ASCII_BACKSPACE 127
#define ASCII_DELETE 8

char *getInput(void);
void  sendAck(int sockfd, struct sockaddr_in *serverAddr);
void  updateLocalScreen(WINDOW *localWin, char *message, int sockfd, struct sockaddr_in serverAddr);
void *listenThread(void *arg);

struct ThreadArgs
{
    int                sockfd;
    struct sockaddr_in serverAddr;
};

typedef struct
{
    int                port;
    struct sockaddr_in serverAddr;
} ServerInfo;

void sendAck(int sockfd, struct sockaddr_in *serverAddr)
{
    sendto(sockfd, ACK_MSG, strlen(ACK_MSG), 0, (struct sockaddr *)serverAddr, sizeof(*serverAddr));
}

ServerInfo getServerInfo(void)
{
    int                sockfd;
    struct sockaddr_in serverAddr;
    ServerInfo         serverInfo;

    // get ip address from user
    printw("Enter the IP address of the server: ");
    refresh();
    char *ipAddress = getInput();
    printw("Enter the port number of the server: ");
    refresh();
    char *portStr = getInput();
    int   port    = atoi(portStr);
    free(portStr);
    serverInfo.port       = port;
    serverInfo.serverAddr = serverAddr;
    return serverInfo;
}

char *getInput(void)
{
    /**
     * Get input from the user
     * Return The input string
     */
    // nodelay(stdscr, TRUE);

    char *input;
    int   index = 0;

    input = (char *)malloc(BUFFER_SIZE * sizeof(char));

    if(input == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    while(1)
    {
        int ch;

        ch = getch();
        if(ch != ERR && ch != '\n')
        {
            if(ch == KEY_BACKSPACE || ch == ASCII_BACKSPACE || ch == ASCII_DELETE)
            {
                if(index > 0)
                {
                    index--;
                    printw("\b \b");
                    refresh();
                }
            }
            else if(index < BUFFER_SIZE - 1)
            {
                input[index++] = (char)ch;
                printw("%c", ch);
                refresh();
            }
        }
        else if(ch == '\n')
        {
            break;
        }
    }
    input[index] = '\0';
    return input;
}

void updateLocalScreen(WINDOW *localWin, char *message, int sockfd, struct sockaddr_in serverAddr)
{
    // Parse the message to extract coordinates and character
    int  x, y, id;
    char ch;
    char buffer[MAX_MSG_SIZE];
    sscanf(message, "%d %d %d %c", &id, &x, &y, &ch);

    // Update the local ncurses screen window
    clear();
    mvwaddch(localWin, y, x, ch);
    wrefresh(localWin);
    sprintf(buffer, "%d %s", id, ACK_MSG);
    // sendto(sockfd, buffer, strlen(buffer), 0,
    //        (struct sockaddr *)&serverAddr, sizeof(serverAddr));
}

void *listenThread(void *arg)
{
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)arg;
    int                sockfd     = threadArgs->sockfd;
    struct sockaddr_in serverAddr = threadArgs->serverAddr;
    char               buffer[MAX_MSG_SIZE];

    while(1)
    {
        int bytesReceived = recv(sockfd, buffer, MAX_MSG_SIZE, 0);
        if(bytesReceived < 0)
        {
            perror("Error receiving message");
            exit(EXIT_FAILURE);
        }
        else if(bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0';    // Null-terminate the received message
            printw("Received %s\n", buffer);
            // Update the local ncurses screen based on the received message
            updateLocalScreen(stdscr, buffer, sockfd, serverAddr);
            refresh();    // Refresh the screen after updating
        }
    }

    pthread_exit(NULL);
}

int main()
{
    int                sockfd;
    struct sockaddr_in serverAddr;
    char               buffer[MAX_MSG_SIZE];
    // Initialize ncurses
    initscr();
    clear();
    keypad(stdscr, TRUE);    // Enable arrow key input
    pthread_t tid;
    bool      connected = false;

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));

    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_port        = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");    // Use the server's IP address

    // send connection request to server
    sendto(sockfd, "CONNECT", strlen("CONNECT"), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

    // receive connection established info from server
    int bytesReceived = recv(sockfd, buffer, MAX_MSG_SIZE, 0);
    if(bytesReceived > 0)
    {
        buffer[bytesReceived] = '\0';    // Null-terminate the received message
        printw("Received: %s\n", buffer);
        uint16_t newPort    = atoi(buffer);    // Convert buffer to integer
        serverAddr.sin_port = htons(newPort);
    }

    struct ThreadArgs threadArgs;
    threadArgs.sockfd     = sockfd;
    threadArgs.serverAddr = serverAddr;

    if(pthread_create(&tid, NULL, listenThread, (void *)&threadArgs) != 0)
    {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }

    while(1)
    {
        int ch = getch();    // Wait for user input

        memset(buffer, 0, sizeof(buffer));

        switch(ch)
        {
            case KEY_DOWN:
                strcpy(buffer, "DOWN");
                break;
            case KEY_UP:
                strcpy(buffer, "UP");
                break;
            case KEY_LEFT:
                strcpy(buffer, "LEFT");
                break;
            case KEY_RIGHT:
                strcpy(buffer, "RIGHT");
                break;
            default:
                continue;
        }

        // printw("Sending: %s\n", buffer);

        printw("Sending, %s\n", buffer);
        sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

        // int bytesReceived = recv(sockfd, buffer, MAX_MSG_SIZE, 0);
        // if (bytesReceived < 0) {
        //     perror("Error receiving message");
        //     exit(EXIT_FAILURE);
        // }
        // printw("Received: %s\n", buffer);

        // buffer[bytesReceived] = '\0'; // Null-terminate the received message
        // printf("Received message: %s\n", buffer);
        // refresh();

        // Update the local ncurses screen based on the received message
        // updateLocalScreen(stdscr, buffer);
    }

    close(sockfd);
    endwin();
    return 0;
}