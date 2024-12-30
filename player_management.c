#include "player_management.h"

void add_online_player(int socket, const char *username, int elo, int is_waiting) {
    pthread_mutex_lock(&general_mutex);

    // Find an empty slot in the `clients` array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != 0 && clients[i].socket == socket) { // Look for an empty slot
            clients[i].socket = socket;
            strncpy(clients[i].username, username, sizeof(clients[i].username) - 1); // Copy the username
            clients[i].elo = elo; // Set the Elo rating
            clients[i].is_waiting = is_waiting; // Set whether the player is waiting for a match
            clients[i].index = i; // Store the index in the array
            printf("New client %s (socket=%d, elo=%d) added to online list at position %d.\n",
                   username, socket, elo, i);
            break; // Exit the loop after adding the client
        }
    }

    pthread_mutex_unlock(&general_mutex);
}


// Remove a player from the online list
void remove_online_player(int socket) {
    pthread_mutex_lock(&general_mutex);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].socket == socket) {
            clients[i].socket = 0;
            memset(clients[i].username, 0, sizeof(clients[i].username));
            clients[i].elo = 0;
            clients[i].is_waiting = 0;
            clients[i].address.sin_family = 0; // Reset the structure
            clients[i].index = -1;
            printf("Client (socket=%d) removed from online list.\n", socket);
            break;
        }
    }
    pthread_mutex_unlock(&general_mutex);
}

int find_match(int socket, int player_elo) {
  int closest_socket = -1;
  int closest_elo_diff = 10000000;
  printf("1\n");
  // pthread_mutex_lock(&general_mutex);

  // Debugging: log client information to ensure correctness
  // pthread_mutex_lock(&general_mutex);
  for (int i = 0; i < 10; i++) {
      if (clients[i].socket > 0) {
          printf("Checking client[%d]: socket=%d, is_waiting=%d, elo=%d\n",
                i, clients[i].socket, clients[i].is_waiting, clients[i].elo);
      }
  }
  pthread_mutex_unlock(&general_mutex);

  printf("2\n");

  for (int i = 0; i < 15; i++) {
      if (clients[i].socket != 0 && clients[i].is_waiting && clients[i].socket != socket) {
          int elo_diff = abs(clients[i].elo - player_elo);
          if (elo_diff < closest_elo_diff && elo_diff <= 50) {
              closest_socket = clients[i].socket;
              closest_elo_diff = elo_diff;
          }
      }
  }
  printf("3\n");

  // pthread_mutex_unlock(&general_mutex);

  return closest_socket;
}
