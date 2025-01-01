#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <wchar.h>
#include <locale.h>
#include <signal.h>

#include "board.h"
#include "auth.h"
#include "game_play.h"
#include "player_management.h"

#define PORT 8080
#define MAX_LINE_LENGTH 256
#define MAX_USERNAME_LENGTH 50
#define MAX_CLIENTS 20
#define BUFFER_SIZE 1024
#define ACCOUNT_FILE "account.txt"
#define MAX_PLAYERS 20

extern pthread_cond_t player_to_join;
extern pthread_mutex_t general_mutex;

// Client information structure
typedef struct {
    int socket;
    struct sockaddr_in address;
    int index;
    char username[50];
    int elo;
    int is_waiting;
    int is_online;
    int played;
    int ready;
} client_info;

extern client_info clients[MAX_CLIENTS];

// Function declarations
void *handle_client(void *arg);
void game_room(int player_one_socket, int player_two_socket);
void matchmaking(client_info *client);

#endif
