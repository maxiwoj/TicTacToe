#include <time.h>
#include "common.h"

//
// Created by maxx on 5/13/17.
//
void signalHandler(int signo);
void cleanUpFuntion();
void* clientServiceThread(void *tmpClient);
int getNumberOfClients();
void unregisterClient(struct client *pClient);
void getArgs(int argc, char **argv) ;
void initServer();
void newClientListener() ;
void notifyOpponent(struct game *game, struct client *client);
void saveClientHistory(struct request request, struct client *pClient);
void sendHistory(struct client *pClient);
void *waitForOpponentThread(void *waitingThreadSpecific);

int port;
char *socketPath;

int UnixSocket;
int inetSocket;
struct sockaddr_un unixServerAddress;
struct sockaddr_in inetServerAddress;

struct client *clients = NULL;
struct game *games = NULL;

pthread_mutex_t clientListMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t historyMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sendToClientMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gamesMutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t playerWaitingForGameCond = PTHREAD_COND_INITIALIZER;


int main(int argc, char **argv){
    atexit(cleanUpFuntion);
    getArgs(argc, argv);

    CHECK(signal(SIGTSTP, signalHandler) != SIG_ERR);
    CHECK(signal(SIGINT, signalHandler) != SIG_ERR);

    initServer();
    newClientListener();
    return 0;
}


void cleanUpFuntion() {
    close(UnixSocket);
    close(inetSocket);
    remove(socketPath);
}

void getArgs(int argc, char **argv) {
    if(argc == 3) {
        port = atoi(argv[1]);
        socketPath = argv[2];
    } else{
        printf("Type port for network gaming: ");
        scanf("%d", &port);
        socketPath = malloc(200 * sizeof(char));
        printf("Type path for local gaming: ");
        scanf("%s", socketPath);
    }
}

void signalHandler(int signo) {
    exit(EXIT_SUCCESS);
}

void initServer() {
    printf("\nCreating local socket\n");
    CHECK_RQ((UnixSocket = socket(AF_UNIX, SOCK_STREAM, 0)) != -1);
    printf("Creating inet socket\n");
    CHECK_RQ((inetSocket = socket(AF_INET, SOCK_STREAM, 0)) != -1);

    memset(&unixServerAddress, 0, sizeof(unixServerAddress));
    memset(&inetServerAddress, 0, sizeof(inetServerAddress));

    // set local address
    unixServerAddress.sun_family = AF_UNIX;
    strcpy(unixServerAddress.sun_path, socketPath);

    // set inet address
    inetServerAddress.sin_family = AF_INET;
    inetServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    inetServerAddress.sin_port = htons(port);

    printf("Binding sockets\n");
    CHECK_RQ(bind(UnixSocket, (struct sockaddr*) &unixServerAddress, sizeof(struct sockaddr_un)) != -1);
    CHECK_RQ(bind(inetSocket, (struct sockaddr*) &inetServerAddress, sizeof(struct sockaddr_in)) != -1);

    printf("starting listening for connections on socket\n");
    CHECK_RQ(listen(UnixSocket, 10) != -1);
    CHECK_RQ(listen(inetSocket, 10) != -1);

    CHECK_RQ(mkdir(history_dir,0777) == 0 || errno == EEXIST);
}

void newClientListener() {
    printf("Waiting for clients\n");
    int maxDesc = UnixSocket > inetSocket ? UnixSocket : inetSocket;
    struct client newClient;
    while(1) {
        fd_set fdSet;
        FD_ZERO(&fdSet);
        FD_SET(UnixSocket, &fdSet);
        FD_SET(inetSocket, &fdSet);
        int numberOfDescriptors = select(maxDesc + 1, &fdSet, NULL, NULL, NULL);
        if(numberOfDescriptors != -1) {
            if(FD_ISSET(UnixSocket,&fdSet)){
                pthread_mutex_lock(&clientListMutex);
                newClient.socket = accept(UnixSocket, NULL, NULL);
                CHECK(pthread_create(&newClient.thread, NULL, clientServiceThread, &newClient) != -1);
            }
            if(FD_ISSET(inetSocket, &fdSet)){
                pthread_mutex_lock(&clientListMutex);
                newClient.socket = accept(inetSocket, NULL, NULL);
                CHECK(pthread_create(&newClient.thread, NULL, clientServiceThread, &newClient) != -1);
            }
        }
    }
}

void* clientServiceThread(void *tmpClient){
    struct client *client = malloc(sizeof(struct client));
    client->socket = ((struct client*) tmpClient)->socket;
    printf("\nNew thread for client %s created\n", client->name);
    client->thread = pthread_self();

    if(getNumberOfClients() >= MAX_CLIENTS) {
        printf("Too many clients, cancelling thread\n");
        pthread_mutex_unlock(&clientListMutex);
        CHECK(pthread_cancel(client->thread) != -1);
    }

    //add client
    client->next = clients;
    clients = client;
    //client is not waiting for game yet
    pthread_t threadWaitingForGame = 0;

    struct request request;
    struct game *game = NULL;

    while(1){
        CHECK(recv(client->socket, &request, sizeof(request), 0) != -1);
        switch(request.action){
            case REGISTER_USER:
                printf("Registering client %s\n", request.name);
                strcpy(client->name, request.name);
                pthread_mutex_unlock(&clientListMutex);
                break;
            case DISCONNECT:
                printf("User %s disconected\n", client->name);
                fflush(stdout);
                pthread_mutex_lock(&clientListMutex);
                unregisterClient(client);
                if(pthread_cancel(threadWaitingForGame) != ESRCH){
                    pthread_mutex_lock(&gamesMutex);
                    game->player2 = game->player1; //make sure it is not null
                    pthread_mutex_unlock(&gamesMutex);
                }
                pthread_mutex_unlock(&clientListMutex);
                CHECK(pthread_cancel(pthread_self()) == 0);
                break;
            case HISTORY:
                printf("Checking history of: %s\n",client->name);
                pthread_mutex_lock(&historyMutex);
                pthread_mutex_lock(&sendToClientMutex);
                sendHistory(client);
                pthread_mutex_unlock(&sendToClientMutex);
                pthread_mutex_unlock(&historyMutex);
                break;
            case START_GAME:
                printf("Starting game for player: %s\n",client->name);
                pthread_mutex_lock(&gamesMutex);
                struct game* tmp  = games;
                printf("Looking for game\n");
                while(tmp != NULL){
                    if(tmp->player2 == NULL){
                        tmp->player2 = client;
                        game = tmp;
                        pthread_cond_broadcast(&playerWaitingForGameCond);
                        pthread_mutex_unlock(&gamesMutex);
                        break;
                    }
                    tmp = tmp->next;
                }
                pthread_mutex_unlock(&gamesMutex);
                if(tmp == NULL){ // There's no waiting players;
                    game = malloc(sizeof(struct game));
                    game->player1 = client;
                    game->player2 = NULL;
                    game->next = games;
                    pthread_mutex_lock(&gamesMutex);
                    games = game;
                    pthread_mutex_unlock(&gamesMutex);
                    struct threadSpecificArgs args;
                    args.client = client;
                    args.game = game;
                    CHECK(pthread_create(&threadWaitingForGame, NULL, waitForOpponentThread, &args) != -1);
                } else {
                    printf("Game has been found\n");
                    pthread_mutex_lock(&sendToClientMutex);
                    notifyOpponent(game, client);
                    pthread_mutex_unlock(&sendToClientMutex);

                }

                break;
            case OPPONENT_MOVED:
                printf("Player %s moved\n", request.name);
                pthread_mutex_lock(&sendToClientMutex);
                CHECK(send(request.opponentSocket, (void *) &request, sizeof(request), 0));
                pthread_mutex_unlock(&sendToClientMutex);
                break;
            case GAME_STATE:
                if(request.gameState == DISCONN){
                    pthread_mutex_lock(&sendToClientMutex);
                    printf("User %s surrendered!\n", request.name);
                    CHECK(send(request.opponentSocket, (void *) &request, sizeof(request), 0) != -1);
                    pthread_mutex_unlock(&sendToClientMutex);
                } else if(request.gameState == WIN){
                    pthread_mutex_lock(&sendToClientMutex);
                    request.gameState = LOSE;
                    printf("User %s won!\n", request.name);
                    CHECK(send(request.opponentSocket,&request, sizeof(request),0) != -1);
                    pthread_mutex_unlock(&sendToClientMutex);
                }
                printf("Saving history for player %s\n",game->player2->name);
                printf("Saving history for player %s\n",game->player1->name);
                pthread_mutex_lock(&historyMutex);
                saveClientHistory(request, game->player1);
                saveClientHistory(request, game->player2);
                pthread_mutex_unlock(&historyMutex);
                break;
        }
    }
}

void waitingForPlayerCleanUp(void *args){
    pthread_mutex_unlock(&gamesMutex);
    pthread_mutex_unlock(&sendToClientMutex);
}
void *waitForOpponentThread(void *waitingThreadSpecific){
    pthread_cleanup_push(waitingForPlayerCleanUp,NULL);
        struct threadSpecificArgs *threadSpecificArgs = waitingThreadSpecific;

        pthread_mutex_lock(&gamesMutex);
        while(threadSpecificArgs->game->player2 == NULL) pthread_cond_wait(&playerWaitingForGameCond, &gamesMutex);
        pthread_mutex_unlock(&gamesMutex);

        printf("Opponent for %s has been found\n", threadSpecificArgs->client->name);
        pthread_mutex_lock(&sendToClientMutex);
            notifyOpponent(threadSpecificArgs->game, threadSpecificArgs->client);
        pthread_mutex_unlock(&sendToClientMutex);

    pthread_cleanup_pop(1);
    return NULL;
}

void sendHistory(struct client *pClient) {
    char historyFilename[100];
    char msg[HISTORY_MSG_LENGTH], *result;

    FILE *plik;
    struct request response;
    response.action = HISTORY;
    strcpy(historyFilename,history_dir);
    strcat(historyFilename,"/");
    strcat(historyFilename,pClient->name);
    plik = fopen(historyFilename,"r");
    if(plik == NULL){
        strcpy(response.history, "No history yet, try to change that by playing the game!\n");
        CHECK(send(pClient->socket,(void*) &response, sizeof(response),0) != -1);
    }else{
        while(1){
            result = fgets(msg,HISTORY_MSG_LENGTH,plik);
            if(result!= NULL){
                strcpy(response.history,msg);
                CHECK(send(pClient->socket,(void*) &response, sizeof(response),0) != -1);
                if(feof(plik) != 0 ) break;
            }else {
                break;
            }
        }

        fclose(plik);
    }
}

void saveClientHistory(struct request request, struct client *pClient) {
    FILE * pFILE;
    time_t timer;
    char buffer[26];
    char filePath[CLIENT_NAME_LENGTH + strlen(history_dir) + 2];

    strcpy(filePath,history_dir);
    strcat(filePath,"/");
    strcat(filePath,pClient->name);
    pFILE= fopen(filePath,"a");
    CHECK(pFILE != NULL);

    time(&timer);
    struct tm *tm_info = localtime(&timer);
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S: ", tm_info);

    fprintf(pFILE,"%s",buffer);
    if(request.opponentSocket == pClient->socket) {
        if(request.gameState == LOSE) fprintf(pFILE, "LOSE");
        else fprintf(pFILE, "Win by walkover");
    } else {
        if(request.gameState == LOSE) fprintf(pFILE, "WIN");
        else fprintf(pFILE, "Surrender");
    }
    fprintf(pFILE,"\n");
    fclose(pFILE);
}

void notifyOpponent(struct game *game, struct client *client) {
    struct request response;
    response.action=GAME_STATE;
    response.gameState = GAME_ON;
    if (game->player1->socket == client->socket) {
        response.opponentSocket = game->player2->socket;
        response.sign = 'X'; //client sign
    } else {
        response.opponentSocket = game->player1->socket;
        response.sign = 'O'; //client sign
    }
    CHECK(send(client->socket,(void*) &response, sizeof(response),0)!=-1);
}

void unregisterClient(struct client *pClient) {
    if(clients == pClient){
        clients = pClient->next;
        free(pClient);
        return;
    }
    if(clients != NULL){
        struct client *tmp  = clients->next;
        struct client *prev = clients;
        while(tmp != NULL && tmp != pClient){
            tmp = tmp->next;
            prev = prev->next;

        }
        if(tmp != NULL){
            prev->next = tmp->next;
            free(tmp);
        }
    }
}

int getNumberOfClients() {
    int result = 0;
    struct client *tmp = clients;
    while(tmp != NULL) {
        tmp = tmp->next;
        result++;
    }
    return result;
}