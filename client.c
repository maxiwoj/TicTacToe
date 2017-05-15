#include "common.h"
#include <curses.h>

int curr_x,curr_y;
int shouldWait=1;
int game_state=0;
int my_turn=0;
int opponent_socket;
int local_flag = 1;
int enemy_ready=0;
int server_port;
int client_port;
char server_path[CLIENT_NAME_LENGTH + 2];
char* ip;
char clientName[CLIENT_NAME_LENGTH];
int socket_fd;
pthread_t thread;
int thread_is_alive = 1;
short registered = 0;
char sign;
int movesCount = 0;
char field[FIELD_SIZE_ROWS][FIELD_SIZE_COLS];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t	waiting_cond = PTHREAD_COND_INITIALIZER;

void exit_handler(int signo) ;
void atexit_function();
void get_arguments(int argc, char **argv);
void init_client();
void registerClient();
void *menu();
void startGame();
gamestate_en makeMove();
void initField();
void sendNotificationClientWon();
void play();
short checkWon(int y, int x, char sign) ;

void sendNotificationClientMoved();

int main(int argc, char **argv){
    CHECK(atexit(atexit_function) == 0);

    CHECK_RQ(signal(SIGTSTP, exit_handler) != SIG_ERR);
    CHECK_RQ(signal(SIGINT, exit_handler) != SIG_ERR);

    get_arguments(argc, argv);

    init_client();

    CHECK_RQ(pthread_create(&thread, NULL, menu, NULL) != -1);

    registerClient();

    struct request response;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while(1){
        CHECK(recv(socket_fd, (void*) &response, sizeof(response), 0) != -1);
        switch(response.action){
            case CHECK_HISTORY:break;
            case GAME_STATE:
                if(response.gameState == LOSE){
//                    TODO:
                } else if(response.gameState == GAME_ON){ //opponent has been found
                    pthread_mutex_lock(&game_mutex);
                    opponent_socket = response.opponent_socket;
                    sign = response.sign;
                    my_turn = sign == 'X' ? 1 : 0;
                    mvprintw(FIELD_SIZE_ROWS, 0,"enemy found \033[32m[ OK ]\033[0m\n");
                    shouldWait=0;
                    pthread_cond_signal(&waiting_cond);
                    pthread_mutex_unlock(&game_mutex);
                } else if(response.gameState == DISCONN){
                    pthread_mutex_lock(&game_mutex);
                    mvprintw(FIELD_SIZE_COLS/2, FIELD_SIZE_ROWS/2 - 30,"\tPlayer %s disconected, which implies you winned by walkover\t");
                    game_state = 0;
                    getch();
                    pthread_mutex_unlock(&game_mutex);
                }
                break;
            case OPPONENT_MOVED:
                pthread_mutex_lock(&game_mutex);
                int y = response.fieldPoint.y;
                int x = response.fieldPoint.x;
                field[y][x] = response.sign;
                mvprintw(y,x,"%c",response.sign);
                my_turn = 1;
                pthread_cond_signal(&waiting_cond);
                pthread_mutex_unlock(&game_mutex);
                break;
            case HISTORY:break;
            case SERVER_INFO:break;
        }
    }
#pragma clang diagnostic pop
}

void registerClient() {
    struct request request;
    strcpy(request.name, clientName);
    request.action = REGISTER_USER;

    printf("Registering player on server...");
    pthread_mutex_lock(&mutex);
    if(send(socket_fd, (void*) &request, sizeof(request), 0) != -1);
    printf("\t\t\t\033[32m[ OK ]\033[0m\n");
    registered = 1;
    pthread_mutex_unlock(&mutex);
}

void get_arguments(int argc, char **argv) {
    printf("parsing arguments...");
    if(argc == 4 && strcmp(argv[2], "local") == 0){
        local_flag = 1;
        strcpy(clientName, argv[1]);
        strcpy(server_path, argv[3]);
        printf("\t\t\t\033[32m[ OK ]\033[0m\n");
    } else if(argc == 5 && strcmp(argv[2], "network") == 0) {
        strcpy(clientName, argv[1]);
        strcpy(ip,argv[3]);
        server_port = atoi(argv[4]);
        printf("\t\t\t\033[32m[ OK ]\033[0m\n");
    } else {
        printf("\t\t\t\033[31m[ WRONG ARGUMENTS ]\033[0m\n");
        printf("Select type of connection [1 = local, 0 = network]: ");
        scanf("%d", &local_flag);
        printf("Username: ");
        scanf("%s", clientName);
        if(local_flag){
            printf("Type server path: ");
            scanf("%s", server_path);
        } else {
            printf("Type server ip: ");
            scanf("%s", ip);
            printf("Type server port: ");
            scanf("%d", &server_port);
        }
    }
}

void exit_handler(int signo) {
    exit(0);
}

void atexit_function(){
    struct request request;
    if(game_state==1){
        strcpy(request.name,clientName);
        request.action = GAME_STATE;
        request.gameState=DISCONN;
        request.opponent_socket=opponent_socket;

        pthread_mutex_lock(&mutex);
        CHECK(send(socket_fd, (void*) &request, sizeof(request), 0) != -1);
        pthread_mutex_unlock(&mutex);
    }
    if(registered){
        request.action = DISCONNECT;
        pthread_mutex_lock(&mutex);
        CHECK(send(socket_fd, (void*) &request, sizeof(request), 0) != -1);
        pthread_mutex_unlock(&mutex);
    }
    thread_is_alive = 0;
    //if(thread != 0)
    //	pthread_join(thread, NULL);
    close(socket_fd);
    endwin();
    printf("Exiting...");
}

void init_client(){
    printf("Creating socket");
    if(local_flag) CHECK_RQ((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) != -1);
    else CHECK_RQ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) != -1);
    printf("\t\t\t\t\t\033[32m[ OK ]\033[0m\n");

    struct sockaddr_un	server_unix_address;
    struct sockaddr_in	server_inet_address;
    struct sockaddr* server_address;

    socklen_t unix_address_size = sizeof(struct sockaddr_un);
    socklen_t inet_address_size = sizeof(struct sockaddr_in);
    socklen_t address_size;

    memset(&server_unix_address, 0, sizeof(server_unix_address));
    memset(&server_inet_address, 0, sizeof(server_inet_address));

    if(local_flag) {
        // Ustawiamy adresy dla połączenia lokalnego
        printf("Setting up server adress");
        server_unix_address.sun_family = AF_UNIX;
        strcpy(server_unix_address.sun_path, server_path);
        server_address = (struct sockaddr*) &server_unix_address;
        printf("\t\t\t\033[32m[ OK ]\033[0m\n");

        address_size = unix_address_size;
    } else {
        // Ustawiamy adresy dla połączenia zdalnego
        printf("Setting up server adress");
        server_inet_address.sin_family = AF_INET;
        inet_pton(AF_INET, ip, &(server_inet_address.sin_addr.s_addr));
        server_inet_address.sin_port = htons((uint16_t) server_port);
        server_address = (struct sockaddr*) &server_inet_address;
        printf("\t\t\t\033[32m[ OK ]\033[0m\n");

        address_size = inet_address_size;
    }

    printf("Connecting to server");
    CHECK_RQ(connect(socket_fd, server_address, address_size) != -1);
    printf("\t\t\t\t\033[32m[ OK ]\033[0m\n");
}

void *menu(){
    initscr();
    mvprintw(FIELD_SIZE_ROWS-1,0,"Przycisnij przycisk aby rozpoczac...\n");

    noecho();
    keypad( stdscr, TRUE );
    //start displaying menu
    const char field1[] = "Graj!";
    const char field2[] = "Historia";
    const char field3[] = "Wyjscie";
    int which = 1;
    int user_action;
    const short int min_choice = 1;
    const short int max_choice = 3;
    do{
        //get user_action
        user_action = getch();
        clear();
        //react to user_action
        if( user_action == 259 && which != min_choice ) which--;
        else if( user_action == 258 && which != max_choice ) which++;
        if( user_action == 10 ){
            switch( which ){
                case 1:
                    clear();
                    mvprintw(1,(FIELD_SIZE_COLS/2)-3,"Gramy!");
                    mvprintw(FIELD_SIZE_ROWS-4,0,"pamietaj, ze jezeli wyjdziesz z gry poprzez nacisniecie przycisku, twoja gra zostanie zapisana!\n");
                    printw("Wcisnij dowolny przycisk aby rozpoczac...\n");
                    printw("Esc - powrot do menu");
                    move(FIELD_SIZE_ROWS-1,0);
                    user_action=getch();
                    if(user_action!=27){
                        pthread_mutex_lock(&game_mutex);
                        game_state = 1;
                        pthread_mutex_unlock(&game_mutex);

                        startGame();
                    }
                    clear();
                    break;
                case 2:
                    clear();
//                    TODO: historia()
                    while(user_action != 27) user_action=getch();
                    clear();
                    break;
                case 3:
                    clear();
                    thread_is_alive = 0;
            }

        }
        //display menu after user_action
        switch( which ){			//if the arrow up/down has been hit - the highlightened field changed, so print menu again, but change the field that is highlightened
            case 1:
                mvprintw( 6, 5, field2 );
                mvprintw( 7, 5, field3 );
                attron( A_REVERSE );
                mvprintw( 5, 5, field1 );
                break;

            case 2:
                mvprintw( 5, 5, field1 );
                mvprintw( 7, 5, field3 );
                attron( A_REVERSE );
                mvprintw( 6, 5, field2 );
                break;

            case 3:
                mvprintw( 5, 5, field1 );
                mvprintw( 6, 5, field2 );
                attron( A_REVERSE );
                mvprintw( 7, 5, field3 );
                break;
        }
        attroff( A_REVERSE );
        mvprintw(FIELD_SIZE_ROWS-1,0,"Game created and produced by Maksymilian Wojczuk");
    } while( thread_is_alive);			//to end the loop you need to hit enter on "exit"
//    getch();
    endwin();
    kill(getpid(), SIGINT);
    return NULL;
}

void startGame() {
    struct request request;
    strcpy(request.name, clientName);
    request.action = START_GAME;
    CHECK_RQ(send(socket_fd, (void*) &request, sizeof(request), 0) != -1);
    initField();

    pthread_mutex_lock(&game_mutex);
    clear();
    mvprintw(FIELD_SIZE_ROWS/2,(FIELD_SIZE_COLS/2) - 14, "Oczekiwanie na przeciwnika...");
    while(shouldWait)
        pthread_cond_wait(&waiting_cond, &game_mutex);
    pthread_mutex_unlock(&game_mutex);

    play();
}

void play() {
    clear();
    move(FIELD_SIZE_ROWS/2, FIELD_SIZE_COLS/2);
    while(game_state==1 && thread_is_alive==1){
        pthread_mutex_lock(&game_mutex);
        while(!my_turn) pthread_cond_wait(&waiting_cond, &game_mutex);
        if(makeMove() == WIN){
            sendNotificationClientMoved();
            sendNotificationClientWon();
            clear();
            mvprintw(FIELD_SIZE_ROWS/2, FIELD_SIZE_COLS/2 - 5, "WINNER!!!");
            getch();
        } else if(game_state && thread_is_alive) sendNotificationClientMoved();
        pthread_mutex_unlock(&game_mutex);
    }
}

void sendNotificationClientMoved() {
    struct request request;
    request.action = OPPONENT_MOVED;
    request.opponent_socket = opponent_socket;
    strcpy(request.name, clientName);
    request.sign = sign;
    struct fieldPoint fieldPoint;
    fieldPoint.y = curr_y;
    fieldPoint.x = curr_x;
    request.fieldPoint = fieldPoint;
    CHECK(send(socket_fd, &request, sizeof(request), 0) != -1);
    my_turn = 0;
}

void initField() {
    for(int i = 0 ; i < FIELD_SIZE_ROWS ; i++){
        for(int j = 0 ; j < FIELD_SIZE_COLS ; j++){
            field[i][j] = 0;
        }
    }
}

gamestate_en makeMove() {
    getyx(stdscr, curr_y, curr_x);
    int user_action = 0;
    pthread_mutex_unlock(&game_mutex);
    while(user_action != 10){
        user_action = getch();
        switch(user_action){
            case 260:
                curr_x--;
                move(curr_y,curr_x);
                break;
            case 259:
                curr_y--;
                move(curr_y,curr_x);
                break;
            case 261:
                curr_x++;
                move(curr_y,curr_x);
                break;
            case 258:
                curr_y++;
                move(curr_y,curr_x);
                break;
            case 27:
//                TODO: End the game and send GAME disconnect
//                return;
            default:
                pthread_mutex_lock(&game_mutex);
                if(field[curr_y][curr_x] == 0 && (user_action == 32 || user_action == 10)) {
                    field[curr_y][curr_x] = sign;
                    mvprintw(curr_y,curr_x,"%c", sign);
                    movesCount++;
                    if(checkWon(curr_y,curr_x,sign)) return WIN;
                    else return GAME_ON;
                }
                pthread_mutex_unlock(&game_mutex);
                break;
        }
    }
}

void sendNotificationClientWon() {
//TODO: notify, send sign!
}

short checkWon(int y, int x, char sign) {
//    TODO: logic of winning
//    TODO: Reverse winning fields
    return 0;
}
