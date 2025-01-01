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
            update_client_status_in_file("client_status.log", clients[i].username, 1);
            printf("New client %s (socket=%d, elo=%d) added to online list at position %d.\n",
                   username, socket, elo, i);
            break; // Exit the loop after adding the client
        }
    }

    pthread_mutex_unlock(&general_mutex);
}


// void update_client_status_in_file(const char *filename, const char *username, int is_online) {
//     FILE *file = fopen(filename, "r+");

//     // If the file doesn't exist, create it
//     if (!file) {
//         file = fopen(filename, "w+");
//         if (!file) {
//             perror("ERROR opening or creating file");
//             return;
//         }
//     }

//     char line[256];
//     long pos;
//     int found = 0;

//     // Read the file to find the username and update the status
//     while (fgets(line, sizeof(line), file)) {
//         pos = ftell(file); // Save the current position
//         char name[50];
//         int status;

//         // Parse username and status
//         if (sscanf(line, "%s %d", name, &status) == 2) {
//             if (strcmp(name, username) == 0) {
//                 // Match found, update the status
//                 found = 1;
//                 fseek(file, pos - strlen(line), SEEK_SET); // Go back to the start of the line
//                 fprintf(file, "%s %d\n", name, is_online);
//                 fflush(file);
//                 break;
//             }
//         }
//     }

//     // If user not found, append them to the file
//     if (!found) {
//         fseek(file, 0, SEEK_END); // Move to the end of the file
//         fprintf(file, "%s %d\n", username, is_online);
//         fflush(file);
//     }

//     fclose(file);
// }



// Remove a player from the online list
void remove_online_player(int socket) {
    pthread_mutex_lock(&general_mutex);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].socket == socket) {
            update_client_status_in_file("client_status.log", clients[i].username, 0);
            clients[i].socket = 0;
            memset(clients[i].username, 0, sizeof(clients[i].username));
            clients[i].elo = 0;
            clients[i].is_waiting = 0;
            clients[i].address.sin_family = 0; // Reset the structure
            clients[i].index = -1;
            clients[i].ready = 0;
            printf("Client (socket=%d) removed from online list.\n", socket);
            break;
        }
    }
    pthread_mutex_unlock(&general_mutex);
}
