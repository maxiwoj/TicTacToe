TicTacToe
=========

## Goal
The goal was to implement simple TicTacToe game using low-level 
mechanisms provided by Unix-based platforms in C such as sockets,
threads etc. Project was not supposed to use any libraries, except 
those concerning user interface. Users should be provided with 
history management, to check their history of game results.

## Usage process
Practical usage have been described in the README file, but the process 
of running the game is following:

1. Run the server and set it's attributes
2. while(serverIsRunning):
    1. if(existsPlayerThatWantsToJoinTheGame):
        1. Run client providing informations about server and type of 
connection (local or network)
        2. Chose menu item:
            1. play - if another player is registered on server and
            wants choosed to play - the player joins an existing game.
            Otherwise new game is created and player waits for opponent
            2. history - check own history of games with results
            3. exit - obvious

## Source Files

### common.h
In this file all the common structures are stored and macros 
defined. 
 
- Enum requestType - every request is processed depending on this enum.
- Enum gameState - when requestType specifies change of a game state
this num contains the actual state. Note, that DISCONN is different 
than regular DISCONNECT from requestType - if user disconnects while 
in the game, his opponent needs to be notified, that the user surrendered.
- struct Request:
    - name - the name of the client
    - history - used by server when sending history to user
    - opponentSocket - client provides server with opponentSocket
    so that server does not need to search for the opponent with 
    every move to notify him
    - action - requestType
    - gameState
    - sign - sign that user has in the game
    - fieldPoint - used in the game to specify move
    
    
### server.c
This file is the file for the server process. Only the most important 
functions will be described:
- initServer - here happens all the magic connected to the 
configuration of the server sockets 
- newClientListener - simple function accepting client requests for 
registration - when a client tries to connect, new socket is created 
(using accept()) and new thread for handling all the requests for 
this client is created. Every of those threads run until 
client disconnects.


### client.c
This file is the file for all gamers, that want to play TicTacToe. 
Here most of the logic of the game is implemented. Client is also 
multi-threaded so as to devise separate network communication 
from user interface and game logic. Due to multi-threading 
user actions are not left unnoticed and interface is always responsive 
with the preservation of unceasing network communication.
#### Main functions:
- initClient - here lies all the network magic with sockets and 
server connection
- communicateWithServer - in this function all the responses and communicates 
from server are handled. Depending on requestType different functions 
are invoked to handle event properly.
- menu - this is a thread function - simple menu. Depending on user action 
different functions are called.
- startGame - requests a game on server and freezes user screen 
while waiting for an opponent
- play - the main game function.