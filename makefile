CC=gcc
TARGET1=server
TARGET2=client
CFLAGS=-Wall -Wpedantic -std=gnu99 -lncurses -lpthread
SERVER=server.c
CLIENT=client.c
COMMON=common.h

.PHONY: all
all: $(TARGET2) $(TARGET1)

.PHONY: run
run: $(TARGET2)
	resize -s 24 80
	./$(TARGET2)

$(TARGET1): $(SERVER)
	$(CC) $< -o $@ $(CFLAGS)

$(TARGET2): $(CLIENT)
	$(CC) $< -o $@ $(CFLAGS)

.PHONY: clean
clean:
	rm -f $(TARGET1) $(TARGET2) $(TEST)

.PHONY: cp
cp:
	cp ../TicTacToe/$(CLIENT) $(CLIENT)
	cp ../TicTacToe/$(SERVER) $(SERVER)
	cp ../TicTacToe/$(COMMON) $(COMMON)