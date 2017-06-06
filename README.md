TicTacToe
=============
Online TicTacToe game

## Overview
This project is a simple online TicTacToe game for 2 users. The 
purpose of the project was usage of low-level mechanisms provided 
in Unix-based systems. Whole project is implemented in C, so as to 
use all the mechanisms directly.

Package contains 2 programs: server and client. Server may handle 
multiple clients (it is limited by a constant in the package), 
even though the game is designed for 2 players (it is possible 
for the server to handle more than 1 game simultaneously). 

## Requirements

- Unix based system
- installed gcc compiler (if other - make sure to change the makefile)
- installed ncurses library

if you do not have the library you can obtain for ex. this way:
```
    $ sudo apt-get install libncurses5-dev
```
## Installation

To install first clone the repo:
```
    $ git clone https://github.com/maxiwoj/TicTacToe.git
```
Then in the project folder compile the project:
```
    $ cd TicTacToe
    $ make
```

## Usage

To run the client server application is required. 
### Easy way
##### To run server:
```
    $ ./server
```
##### To run client:
```
    $ ./client
```

Both programs will ask you for all the needed parameters

### Fast way
##### To run server:
```
    $ ./server [port] [UnixSocketFilePath]
```
where:  
- port is a valid port for network connection
- UnixSocketFilePath is a path to nonexistent file, which will be 
used by server for inner-unix connection

Once you have a server running multiple users can connect to it.

##### To run client:
###### if the server is running on the same machine:
```
    $ ./client [playerName] local [UnixSocketFilePath]
```
where 
- UnixSocketFilePath is the file path given to server

###### if you want to play remotly:
```
    $ ./client [playerName] network [serverIpAddress] [serverport]
```
where:
- serverIpAddress is the Ip address of the machine where server is 
running
- serverPort is the port given for the server
