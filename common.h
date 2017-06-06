//
// Created by maxx on 5/13/17.
//

#ifndef TICTACTOE_COMMON_H
#define TICTACTOE_COMMON_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>


#define history_dir "history"
#define HISTORY_MSG_LENGTH 200
#define FIELD_SIZE_COLS 80
#define FIELD_SIZE_ROWS 24
#define NUMBER_OF_SIGNS_WINNING 5
#define CLIENT_NAME_LENGTH 50
#define MAX_CLIENTS 20

#define CHECK(x) \
    do { \
        if (!(x)) { \
            fprintf(stderr, "%s:%d: ", __func__, __LINE__); \
            perror(#x); \
        } \
    } while (0) \


#define CHECK_RQ(x) \
    do { \
        if (!(x)) { \
            fprintf(stderr, "%s:%d: ", __func__, __LINE__); \
            perror(#x); \
            exit(-1); \
        } \
    } while (0) \


typedef enum {
    REGISTER_USER,
    START_GAME,
    DISCONNECT,
    GAME_STATE,
    OPPONENT_MOVED,
    HISTORY
} requestType;

typedef enum {
    WIN,
    GAME_ON,
    LOSE,
    DISCONN // wysylany w momencie, jezeli uzytkownik rozlaczy sie podczas gry
} gameState;

struct client {
    char name[CLIENT_NAME_LENGTH];
    int socket;
    pthread_t thread;
    struct client* next;
};

struct fieldPoint{
    int y;
    int x;
};

struct request{
    char name[CLIENT_NAME_LENGTH];                  //player name
    char history[HISTORY_MSG_LENGTH];               //history
    int opponentSocket;
    requestType action;
    gameState gameState;
    char sign;
    struct fieldPoint fieldPoint;
};

struct game {
    struct client *player1;
    struct client *player2;
    struct game* next;
};

struct threadSpecificArgs{
    struct game *game;
    struct client *client;
};


#endif //TICTACTOE_COMMON_H
