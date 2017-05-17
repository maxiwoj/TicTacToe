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
#define FIELD_SIZE_COLS 150
#define FIELD_SIZE_ROWS 50
#define NUMBER_OF_SIGNS_WINNING 3
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
    REGISTER_USER,	//wysylane przy probie polaczenia z serwerem
    START_GAME,
    CHECK_HISTORY,
    DISCONNECT,
    GAME_STATE, 	//GAME,WIN,LOSS,DISCONNECT
    OPPONENT_MOVED, 	//GAME READY
    HISTORY,
} action_en;

typedef enum {
    WIN,
    GAME_ON,
    LOSE,
    DISCONN
} gamestate_en;

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
    char name[CLIENT_NAME_LENGTH];
    int opponent_socket;  // GAME.SERVER_INFO.OPPONENT_SOCKET to w opponent_socket.socket przeciwnika
    action_en action;
    struct fieldPoint fieldPoint;
    gamestate_en gameState;
    char sign;
    char history[HISTORY_MSG_LENGTH];
};

struct game {
    struct client *player_1;
    struct client *player_2;
    struct game* next;
};

struct threadSpecificArgs{
    struct game game;
    struct client client;
};


#endif //TICTACTOE_COMMON_H
