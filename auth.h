#ifndef AUTH_H
#define AUTH_H

#include "server.h"

int validate_login(const char *username, const char *password);
int register_user(const char *username, const char *password);
int get_user_elo(const char *username);
int initialize_elo(const char *username);
void update_or_add_user_elo(const char *filename, const char *username, int newElo);

#endif
