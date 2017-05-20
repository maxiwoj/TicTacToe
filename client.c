#include "common.h"
#include <curses.h>

int currX = FIELD_SIZE_COLS/2;
int currY = FIELD_SIZE_ROWS/2;
int shouldWait=1;
int myTurn=1;
short registered = 0;
int inGameState=0;
char sign;
int gameThreadIsAlive = 1;

int opponentSocket;
int localGaming = 1;
int serverPort;
char serverPath[CLIENT_NAME_LENGTH + 2];
char* serverIP;
char clientName[CLIENT_NAME_LENGTH];
int serverSocket;
pthread_t thread;
char field[FIELD_SIZE_ROWS][FIELD_SIZE_COLS];
pthread_mutex_t sendMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gameMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t	waiting_cond = PTHREAD_COND_INITIALIZER;

void signalHandler(int signo) ;
void cleanUpFunction();
void getArgs(int argc, char **argv);
void initClient();
void registerClient();
void *menu();
void startGame();
gameState makeMove();
void initField();
void sendNotificationClientWon();
void sendNotificationClientMoved();
void play();
short checkAndMarkIfWon(int y, int x, char sign) ;
int kbhit(void);
int getUsrInput();
short belongsToTheField(int y, int x);
void reverseWinningFields(int y, int x, char sign, short way);
void communicateWithServer();
void askForHistory();
void endGameAndSendDisconnect();

int main(int argc, char **argv){
    CHECK(atexit(cleanUpFunction) == 0);

    CHECK_RQ(signal(SIGTSTP, signalHandler) != SIG_ERR);
    CHECK_RQ(signal(SIGINT, signalHandler) != SIG_ERR);

    getArgs(argc, argv);

    initClient();

    registerClient();

    CHECK_RQ(pthread_create(&thread, NULL, menu, NULL) != -1);

    communicateWithServer();
}

void communicateWithServer() {
    struct request response;
    while(1){
        recv(serverSocket, (void*) &response, sizeof(response), 0);
        switch(response.action){
            case GAME_STATE:
                if(response.gameState == LOSE){
                    pthread_mutex_lock(&gameMutex);
                    field[response.fieldPoint.y][response.fieldPoint.x] = response.sign;
                    mvprintw(response.fieldPoint.y, response.fieldPoint.x, "%c", response.sign);
                    checkAndMarkIfWon(response.fieldPoint.y, response.fieldPoint.x, response.sign);
                    mvprintw(FIELD_SIZE_ROWS - 1, FIELD_SIZE_COLS/2, "YOU LOSE");
                    refresh();
                    myTurn = 1;
                    shouldWait = 1;
                    inGameState = 0;
                    getUsrInput();
                    pthread_cond_signal(&waiting_cond);
                    pthread_mutex_unlock(&gameMutex);
                } else if(response.gameState == GAME_ON){ //opponent has been found
                    pthread_mutex_lock(&gameMutex);
                    opponentSocket = response.opponentSocket;
                    sign = response.sign;
                    myTurn = sign == 'X' ? 1 : 0;
                    shouldWait=0;
                    pthread_cond_signal(&waiting_cond);
                    pthread_mutex_unlock(&gameMutex);
                } else if(response.gameState == DISCONN){
                    pthread_mutex_lock(&gameMutex);
                    mvprintw(FIELD_SIZE_ROWS/2, FIELD_SIZE_COLS/2 - 30,"\tPlayer %s disconected, which implies you winned by walkover\t", response.name);
                    refresh();
                    myTurn = 1;
                    shouldWait = 1;
                    inGameState = 0;
                    getUsrInput();
                    pthread_cond_signal(&waiting_cond);
                    pthread_mutex_unlock(&gameMutex);
                }
                break;
            case OPPONENT_MOVED:
                pthread_mutex_lock(&gameMutex);
                int y = response.fieldPoint.y;
                int x = response.fieldPoint.x;
                field[y][x] = response.sign;
                mvprintw(y,x,"%c",response.sign);
                myTurn = 1;
                mvprintw(FIELD_SIZE_ROWS - 1, FIELD_SIZE_COLS/2 - 5, "YOUR TURN");
                getUsrInput();
                for(int i = FIELD_SIZE_COLS/2 - 5 ; i < FIELD_SIZE_COLS/2 + 5 ; i++){
                    mvprintw(FIELD_SIZE_ROWS - 1, i, "%c",field[FIELD_SIZE_ROWS -1][i] != 0 ? field[FIELD_SIZE_ROWS -1][i] : ' ');
                }
                move(currY,currX);
                pthread_cond_signal(&waiting_cond);
                pthread_mutex_unlock(&gameMutex);
                break;
            case HISTORY:
                printw(response.history);
                refresh();
                break;
        }
    }
}

void registerClient() {
    struct request request;
    strcpy(request.name, clientName);
    request.action = REGISTER_USER;

    printf("Registering player on server...\n");
    pthread_mutex_lock(&sendMutex);
    CHECK_RQ(send(serverSocket, (void*) &request, sizeof(request), 0) != -1);
    registered = 1;
    pthread_mutex_unlock(&sendMutex);
}

void getArgs(int argc, char **argv) {
    printf("parsing arguments...");
    if(argc == 4 && strcmp(argv[2], "local") == 0){
        localGaming = 1;
        strcpy(clientName, argv[1]);
        strcpy(serverPath, argv[3]);
        printf("\t\t\033[32m[ ARGUMENTS OK ]\033[0m\n");
    } else if(argc == 5 && strcmp(argv[2], "network") == 0) {
        localGaming = 0;
        strcpy(clientName, argv[1]);
        serverIP = argv[3];
        serverPort = atoi(argv[4]);
        printf("\t\t\033[32m[ ARGUMENTS OK ]\033[0m\n");
    } else {
        printf("\t\t\033[31m[ WRONG ARGUMENTS ]\033[0m\n");
        printf("Select type of connection [1 = local, 0 = network]: ");
        scanf("%d", &localGaming);
        printf("Username: ");
        scanf("%s", clientName);
        if(localGaming){
            printf("Type server path: ");
            scanf("%s", serverPath);
        } else {
            printf("Type server ip: ");
            serverIP = calloc(20, sizeof(char));
            scanf("%s", serverIP);
            printf("Type server port: ");
            scanf("%d", &serverPort);
        }
    }
}

void signalHandler(int signo) {
    exit(0);
}

void cleanUpFunction(){
    struct request request;
    if(inGameState==1){
        strcpy(request.name,clientName);
        request.action = GAME_STATE;
        request.gameState=DISCONN;
        request.opponentSocket=opponentSocket;

        pthread_mutex_lock(&sendMutex);
        CHECK(send(serverSocket, (void*) &request, sizeof(request), 0) != -1);
        pthread_mutex_unlock(&sendMutex);
    }
    if(registered){
        request.action = DISCONNECT;
        pthread_mutex_lock(&sendMutex);
        CHECK(send(serverSocket, (void*) &request, sizeof(request), 0) != -1);
        pthread_mutex_unlock(&sendMutex);
    }
    gameThreadIsAlive = 0;
    close(serverSocket);
    endwin();
    printf("Exiting...");
}

void initClient(){
    printf("Creating socket\n");
    if(localGaming) CHECK_RQ((serverSocket = socket(AF_UNIX, SOCK_STREAM, 0)) != -1);
    else CHECK_RQ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) != -1);

    struct sockaddr_un	server_unix_address;
    struct sockaddr_in	server_inet_address;
    struct sockaddr* server_address;

    socklen_t address_size;
    memset(&server_unix_address, 0, sizeof(server_unix_address));
    memset(&server_inet_address, 0, sizeof(server_inet_address));

    if(localGaming) {
        // Set address for local connection
        printf("Setting up server adress\n");
        server_unix_address.sun_family = AF_UNIX;
        strcpy(server_unix_address.sun_path, serverPath);
        server_address = (struct sockaddr*) &server_unix_address;

        address_size = sizeof(struct sockaddr_un);
    } else {
        // Set address for network connection
        printf("Setting up server adress\n");
        server_inet_address.sin_family = AF_INET;
        inet_pton(AF_INET, serverIP, &(server_inet_address.sin_addr.s_addr));
        server_inet_address.sin_port = htons(serverPort);
        server_address = (struct sockaddr*) &server_inet_address;

        address_size = sizeof(struct sockaddr_in);
    }

    printf("Connecting to server");
    CHECK_RQ(connect(serverSocket, server_address, address_size) != -1);
    printf("\t\t\033[32m[ Connected ]\033[0m\n");
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
        user_action = getUsrInput();
        clear();
        //react to user_action
        if( user_action == 259 && which != min_choice ) which--;
        else if( user_action == 258 && which != max_choice ) which++;
        if( user_action == 10 ){
            switch( which ){
                case 1:
                    clear();
                    mvprintw(1,(FIELD_SIZE_COLS/2)-3,"Gramy!");
                    mvprintw(FIELD_SIZE_ROWS-4,0,"pamietaj, wyjscie z gry bedzie oznaczalo poddanie sie!\n");
                    printw("Wcisnij dowolny przycisk aby rozpoczac...\n");
                    printw("Esc - powrot do menu");
                    move(FIELD_SIZE_ROWS-1,0);
                    user_action=getUsrInput();
                    if(user_action!=27){
                        startGame();
                    }
                    clear();
                    break;
                case 2:
                    clear();
                    move(0,0);
                    askForHistory();
                    while(user_action != 27) {
                        user_action=getUsrInput();
                        if(user_action == 259) scrl(2);
                        else if(user_action == 258) scrl(-2);
                    }
                    clear();
                    break;
                case 3:
                    clear();
                    gameThreadIsAlive = 0;
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
    } while( gameThreadIsAlive);			//to end the loop you need to hit enter on "exit"
    endwin();
    kill(getpid(), SIGINT);
    return NULL;
}

void askForHistory() {
    struct request request;
    request.action = HISTORY;
    strcpy(request.name, clientName);
    CHECK(send(serverSocket, &request, sizeof(request), 0) != -1);
}

void startGame() {
    struct request request;
    strcpy(request.name, clientName);
    request.action = START_GAME;
    CHECK_RQ(send(serverSocket, (void*) &request, sizeof(request), 0) != -1);
    initField();

    pthread_mutex_lock(&gameMutex);
    clear();
    mvprintw(FIELD_SIZE_ROWS/2,(FIELD_SIZE_COLS/2) - 14, "Oczekiwanie na przeciwnika...");
    refresh();
    while(shouldWait)
        pthread_cond_wait(&waiting_cond, &gameMutex);
    inGameState = 1;
    pthread_mutex_unlock(&gameMutex);
    play();
}

void play() {
    clear();
    move(FIELD_SIZE_ROWS/2, FIELD_SIZE_COLS/2);
    while(inGameState && gameThreadIsAlive){
        pthread_mutex_lock(&gameMutex);
        while(!myTurn) pthread_cond_wait(&waiting_cond, &gameMutex);
        if(!inGameState || !gameThreadIsAlive) {
            pthread_mutex_unlock(&gameMutex);
            break;
        }
        if(makeMove() == WIN){
            sendNotificationClientWon();
            mvprintw(FIELD_SIZE_ROWS-1, FIELD_SIZE_COLS/2 - 5, "WINNER!!!");
            refresh();
            getUsrInput();
            clear();
            inGameState = 0;
            myTurn = 1;
            shouldWait = 1;
        } else if(inGameState && gameThreadIsAlive) sendNotificationClientMoved();
        pthread_mutex_unlock(&gameMutex);
    }
}

gameState makeMove() {
    getyx(stdscr, currY, currX);
    int user_action = 0;
    pthread_mutex_unlock(&gameMutex);
    while(1){
        user_action = getUsrInput();
        switch(user_action){
            case 260:
                currX--;
                move(currY,currX);
                break;
            case 259:
                currY--;
                move(currY,currX);
                break;
            case 261:
                currX++;
                move(currY,currX);
                break;
            case 258:
                currY++;
                move(currY,currX);
                break;
            case 27:
                endGameAndSendDisconnect();
                return DISCONN;
            default:
                pthread_mutex_lock(&gameMutex);
                if(field[currY][currX] == 0 && (user_action == 32 || user_action == 10)) {
                    field[currY][currX] = sign;
                    mvprintw(currY,currX,"%c", sign);
                    refresh();
                    if(checkAndMarkIfWon(currY, currX, sign)) return WIN;
                    else return GAME_ON;
                }
                pthread_mutex_unlock(&gameMutex);
                break;
        }
    }
}

void endGameAndSendDisconnect() {
    struct request request;
    strcpy(request.name, clientName);
    request.gameState = DISCONN;
    request.action = GAME_STATE;
    request.opponentSocket = opponentSocket;
    pthread_mutex_lock(&sendMutex);
    CHECK(send(serverSocket, (void*) &request, sizeof(request), 0) != -1);
    pthread_mutex_lock(&sendMutex);
    pthread_mutex_lock(&gameMutex);
    inGameState = 0;
    pthread_mutex_lock(&gameMutex);

}

void sendNotificationClientMoved() {
    struct request request;
    request.action = OPPONENT_MOVED;
    request.opponentSocket = opponentSocket;
    strcpy(request.name, clientName);
    request.sign = sign;
    struct fieldPoint fieldPoint;
    fieldPoint.y = currY;
    fieldPoint.x = currX;
    request.fieldPoint = fieldPoint;
    CHECK(send(serverSocket, &request, sizeof(request), 0) != -1);
    myTurn = 0;
}

void initField() {
    for(int i = 0 ; i < FIELD_SIZE_ROWS ; i++){
        for(int j = 0 ; j < FIELD_SIZE_COLS ; j++){
            field[i][j] = 0;
        }
    }
}

void sendNotificationClientWon() {
    struct request request;
    strcpy(request.name, clientName);
    request.action = GAME_STATE;
    request.gameState = WIN;
    request.sign = sign;
    request.opponentSocket = opponentSocket;
    struct fieldPoint fieldPoint;
    fieldPoint.y = currY;
    fieldPoint.x = currX;
    request.fieldPoint = fieldPoint;
    CHECK(send(serverSocket,&request, sizeof(request), 0) != -1);
}

short checkAndMarkIfWon(int y, int x, char sign) {
    int numberOfSignsNear = 0;
    short checkingWay = 1;
    for(int i = -5 ; i < 6 ; i++){
        if(belongsToTheField(y,x)){
            if(field[y+i][x+i] == sign) numberOfSignsNear++;
            else numberOfSignsNear = 0;
            if(numberOfSignsNear == NUMBER_OF_SIGNS_WINNING) {
                reverseWinningFields(y + i, x + i, sign, checkingWay);
                return 1;
            }
        }
    }
    numberOfSignsNear = 0;
    checkingWay = 2;
    for(int i = -5 ; i < 6 ; i++){
        if(belongsToTheField(y,x)){
            if(field[y-i][x+i] == sign) numberOfSignsNear++;
            else numberOfSignsNear = 0;
            if(numberOfSignsNear == NUMBER_OF_SIGNS_WINNING) {
                reverseWinningFields(y - i, x + i, sign, checkingWay);
                return 1;
            }
        }
    }
    numberOfSignsNear = 0;
    checkingWay = 3;
    for(int i = -5 ; i < 6 ; i++){
        if(belongsToTheField(y,x)){
            if(field[y][x+i] == sign) numberOfSignsNear++;
            else numberOfSignsNear = 0;
            if(numberOfSignsNear == NUMBER_OF_SIGNS_WINNING) {
                reverseWinningFields(y, x + i, sign, checkingWay);
                return 1;
            }
        }
    }
    numberOfSignsNear = 0;
    checkingWay = 4;
    for(int i = -5 ; i < 6 ; i++){
        if(belongsToTheField(y,x)){
            if(field[y+i][x] == sign) numberOfSignsNear++;
            else numberOfSignsNear = 0;
            if(numberOfSignsNear == NUMBER_OF_SIGNS_WINNING) {
                reverseWinningFields(y + i, x, sign, checkingWay);
                return 1;
            }
        }
    }
    return 0;
}

void reverseWinningFields(int y, int x, char sign, short way) {
    attron( A_REVERSE );
    switch(way){
        case 1:
            for(int i = 0 ; i < NUMBER_OF_SIGNS_WINNING ; i++) mvprintw(y-i, x-i, "%c", sign);
            break;
        case 2:
            for(int i = 0 ; i < NUMBER_OF_SIGNS_WINNING ; i++) mvprintw(y+i, x-i, "%c", sign);
            break;
        case 3:
            for(int i = 0 ; i < NUMBER_OF_SIGNS_WINNING ; i++) mvprintw(y, x-i, "%c", sign);
            break;
        case 4:
            for(int i = 0 ; i < NUMBER_OF_SIGNS_WINNING; i++) mvprintw(y-i, x, "%c", sign);
            break;
    }
    attroff( A_REVERSE );
}

short belongsToTheField(int y, int x) {
    return y >= 0 && y < FIELD_SIZE_ROWS && x >= 0 && x < FIELD_SIZE_COLS;
}

int kbhit(void){
    struct timeval        timeout;
    fd_set                readfds;
    int                how;

    /* look only at stdin (fd = 0) */
    FD_ZERO(&readfds);
    FD_SET(0, &readfds);

    /* poll: return immediately */
    timeout.tv_sec = 0L;
    timeout.tv_usec = 0L;

    how = select(1, &readfds, (fd_set *)NULL, (fd_set *)NULL, &timeout);
    /* Change "&timeout" above to "(struct timeval *)0"       ^^^^^^^^
     * if you want to wait until a key is hit
     */

    if ((how > 0) && FD_ISSET(0, &readfds))
        return 1;
    else
        return 0;
}
int getUsrInput(){
    while(kbhit()) getch();
    return getch();
}
