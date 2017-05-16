#include "common.h"

//
// Created by maxx on 5/13/17.
//
void exit_handler(int signo);
void atexit_function();
void* server_thread_function(void* tmp_client);
int clients_count();
void unregister_client(struct client *pClient);
void get_args(int argc, char** argv) ;
void init_server();
void listen_function() ;
void send_opponent_to_player(struct game *game, struct client *client);

int port;
char *path;

int unix_socket;
int inet_socket;
struct sockaddr_un server_unix_address;
struct sockaddr_in server_inet_address;

struct client *clients = NULL;
struct game *games = NULL;

pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t history_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t send_to_opponent_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t waiting_player_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t waiting_cond = PTHREAD_COND_INITIALIZER;


int main(int argc, char **argv){
    atexit(atexit_function);
    get_args(argc, argv);

    CHECK(signal(SIGTSTP, exit_handler) != SIG_ERR);
    CHECK(signal(SIGINT, exit_handler) != SIG_ERR);

    init_server();
    listen_function();
    return 0;
}


void atexit_function() {
    //int i;
    /*for(i=0;i<client_counter;i++){
        shutdown(clients[i],SHUT_RDWR);
    }*/
    close(unix_socket);
    close(inet_socket);
    remove(path);
}

void get_args(int argc, char** argv) {
    if(argc == 3) {
        port = atoi(argv[1]);
        path = argv[2];
    } else{
        printf("Type port for network gaming: ");
        scanf("%d", &port);
        printf("Type path for local gaming: ");
        scanf("%s", path);
    }
}

void exit_handler(int signo) {
    if(signo == SIGTSTP)
        exit(EXIT_SUCCESS);
}

void init_server() {
    printf("\nCreating server socket for local communication");
    CHECK_RQ((unix_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) != -1);
    printf("\t\t\033[32m[ OK ]\033[0m\n");

    printf("Creating server socket for internet communication");
    CHECK_RQ((inet_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) != -1);
    printf("\t\033[32m[ OK ]\033[0m\n");

    memset(&server_unix_address, 0, sizeof(server_unix_address));
    memset(&server_inet_address, 0, sizeof(server_inet_address));

    // Ustawiamy adres lokalny
    server_unix_address.sun_family = AF_UNIX;
    strcpy(server_unix_address.sun_path, path);

    // Ustawiamy adres inet
    server_inet_address.sin_family = AF_INET;
    server_inet_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_inet_address.sin_port = htons(port);

    printf("Binding sockets");

    CHECK_RQ(bind(unix_socket, (struct sockaddr*) &server_unix_address, sizeof(struct sockaddr_un)) != -1);
    CHECK_RQ(bind(inet_socket, (struct sockaddr*) &server_inet_address, sizeof(struct sockaddr_in)) != -1);
    printf("\t\t\t\t\t\t\033[32m[ OK ]\033[0m\n");

    CHECK_RQ(listen(unix_socket, 10) != -1);
    CHECK_RQ(listen(inet_socket, 10) != -1);

    CHECK_RQ(mkdir(history,0777) == 0 || errno == EEXIST);
}

void listen_function() {
    printf("Waiting for clients\n");
    struct client tmp_client;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while(1) {
        pthread_mutex_lock(&client_list_mutex);
        tmp_client.socket = accept(unix_socket, NULL, NULL);
        if(tmp_client.socket == -1) {
            CHECK(tmp_client.socket != EAGAIN && tmp_client.socket != EWOULDBLOCK);
            pthread_mutex_unlock(&client_list_mutex);
        } else {
            CHECK(pthread_create(&tmp_client.thread, NULL, server_thread_function, &tmp_client) != -1);
        }

        pthread_mutex_lock(&client_list_mutex);
        tmp_client.socket = accept(inet_socket, NULL, NULL);
        if(tmp_client.socket == -1) {
            CHECK(tmp_client.socket != EAGAIN && tmp_client.socket != EWOULDBLOCK);
            pthread_mutex_unlock(&client_list_mutex);
        } else {
            CHECK(pthread_create(&tmp_client.thread, NULL, server_thread_function, &tmp_client) != -1);
        }
    }
#pragma clang diagnostic pop
}


void* server_thread_function(void* tmp_client){
    printf("\nNew thread created\n");
    struct client *client = malloc(sizeof(struct client));
    client->socket = ((struct client*) tmp_client)->socket;
    client->thread = pthread_self();

    if(clients_count() >= MAX_CLIENTS) {
        printf("Too many clients, cancelling thread\n");
        pthread_mutex_unlock(&client_list_mutex);
        CHECK(pthread_cancel(client->thread) != -1);
    }

    client->next = clients;
    clients = client;

    struct request request;
    struct game *game = NULL;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while(1){
        CHECK(recv(client->socket, &request, sizeof(request), 0) != -1);
        switch(request.action){
            case REGISTER_USER:
                printf("Registering client %s", request.name);
                strcpy(client->name, request.name);
                printf("\t\t\t\t\t\033[32m[ OK ]\033[0m\n");
                pthread_mutex_unlock(&client_list_mutex);
                break;
            case DISCONNECT:
                printf("User %s disconected", client->name);
                fflush(stdout);
                pthread_mutex_lock(&client_list_mutex);
                unregister_client(client);
                pthread_mutex_unlock(&client_list_mutex);
                CHECK(pthread_cancel(client->thread) != -1);
                printf("\t\t\t\t\t\033[32m[ OK ]\033[0m\n");
                break;
            case CHECK_HISTORY:
                printf("Checking history of: %s\n",client->name);
                pthread_mutex_lock(&history_mutex);
                pthread_mutex_lock(&client_list_mutex);
                pthread_mutex_lock(&send_to_opponent_mutex);
//                TODO: check_and_send_history(client);
                pthread_mutex_unlock(&send_to_opponent_mutex);
                pthread_mutex_unlock(&client_list_mutex);
                pthread_mutex_unlock(&history_mutex);
                printf("\t\t\t\t\t\033[32m[ OK ]\033[0m\n");
                break;
            case START_GAME:
                printf("Starting game for player: %s\n",client->name);
                pthread_mutex_lock(&waiting_player_mutex);
                struct game* tmp  = games;
                printf("Looking for game\n");
                while(tmp != NULL){
                    if(tmp->player_2 == NULL){
                        tmp->player_2 = client;
                        game = tmp;
                        pthread_cond_signal(&waiting_cond);
                        pthread_mutex_unlock(&waiting_player_mutex);
                        break;
                    }
                    tmp = tmp->next;
                }
                if(tmp == NULL){ // There's no waiting players;
                    if(game != NULL) game->player_2 = NULL;
                    else {
                        game = malloc(sizeof(struct game));
                        game->player_1 = client;
                        game->next = games;
                        games = game;
                    }
                    while(game->player_2 == NULL) pthread_cond_wait(&waiting_cond, &waiting_player_mutex);
                    pthread_mutex_unlock(&waiting_player_mutex);
                }

                printf("Game has been found\n");
                pthread_mutex_lock(&send_to_opponent_mutex);
                send_opponent_to_player(game,client);
                pthread_mutex_unlock(&send_to_opponent_mutex);

                printf("\t\t\t\t\t\033[32m[ OK ]\033[0m\n");
                break;
            case OPPONENT_MOVED:
                printf("Player %s moved\n", request.name);
                pthread_mutex_lock(&send_to_opponent_mutex);
                CHECK(send(request.opponent_socket, (void *) &request, sizeof(request), 0));
                pthread_mutex_unlock(&send_to_opponent_mutex);
                break;
            case GAME_STATE:
                if(request.gameState == DISCONN){
                    pthread_mutex_lock(&send_to_opponent_mutex);
                    pthread_mutex_lock(&waiting_player_mutex);
//                    TODO: Save in history
                    printf("User %s surrendered!\n", request.name);
                    CHECK(send(request.opponent_socket, (void *) &request, sizeof(request), 0));

                    pthread_mutex_unlock(&waiting_player_mutex);
                    pthread_mutex_unlock(&send_to_opponent_mutex);
                }
                break;
            default:
                printf("Request received but not recognized: %d\n", request.action);
        }
    }
#pragma clang diagnostic pop
}

void send_opponent_to_player(struct game *game, struct client *client) {
    struct request response;
    response.action=GAME_STATE;
    response.gameState = GAME_ON;
    if (game->player_1->socket == client->socket) {
        response.opponent_socket = game->player_2->socket;
        response.sign = 'X'; //client sign
    } else {
        response.opponent_socket = game->player_1->socket;
        response.sign = 'O'; //client sign
    }
    CHECK(send(client->socket,(void*) &response, sizeof(response),0)!=-1);
}

void unregister_client(struct client *pClient) {
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

int clients_count() {
    int result = 0;
    struct client *tmp = clients;
    while(tmp != NULL) {
        tmp = tmp->next;
        result++;
    }
    return result;
}