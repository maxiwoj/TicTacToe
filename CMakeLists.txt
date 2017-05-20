cmake_minimum_required(VERSION 3.0)
project(TicTacToe)

set(CMAKE_C_STANDARD 99)

add_compile_options(-Wall -pthread)


set(SOURCE_FILES
        client.c
#        server.c
        common.h
        )

add_executable(TicTacToe ${SOURCE_FILES})
target_link_libraries(TicTacToe -pthread)

#find_package( Curses REQUIRED )
#include_directories( ${CURSES_INCLUDE_DIRS} )
#target_link_libraries( TicTacToe ${CURSES_LIBRARIES} )

