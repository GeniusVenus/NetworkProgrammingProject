#ifndef PLAYER_MANAGEMENT_H
#define PLAYER_MANAGEMENT_H

#include "server.h"

void add_online_player(int socket, const char *username, int elo, int is_waiting);
void remove_online_player(int socket);
int find_match(int socket, int player_elo);

#endif
