# Makefile for C Chess

CC = gcc
CFLAGS = -pthread -Wall -Wextra -O2
SERVER_SRC = server.c board.c
SERVER_OBJ = $(SERVER_SRC:.c=.o)
CLIENT_SRC = client.c
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
SERVER_BIN = server
CLIENT_BIN = client

.PHONY: all clean run_server run_client

all: $(SERVER_BIN) $(CLIENT_BIN)

# Build server
$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC)

# Build client
$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC)

# Clean build files
clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN) $(SERVER_OBJ) $(CLIENT_OBJ)

# Run the server
run_server: $(SERVER_BIN)
	./$(SERVER_BIN)

# Run the client
run_client: $(CLIENT_BIN)
	./$(CLIENT_BIN) localhost 8080
