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

#include "board.c"

#define PORT 8080;

#define MAX_LINE_LENGTH 256
#define MAX_USERNAME_LENGTH 50
#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024
#define ACCOUNT_FILE "account.txt"

// Waiting player conditional variable
pthread_cond_t player_to_join;
pthread_mutex_t general_mutex;
#define MAX_PLAYERS 20
bool is_diagonal(int, int);

typedef struct {
    int socket;
    struct sockaddr_in address;
    int index;
    char username[50];
    int elo;
    int is_waiting;
    int is_online;
    int played;
} client_info;

client_info clients[MAX_CLIENTS] = {0};

void update_or_add_user_elo(const char *filename, const char *username, int newElo) {
    // Temporary file for writing
    const char *tempFile = "temp.txt";

    // Open the original file for reading
    FILE *filePointer = fopen(filename, "r");
    if (filePointer == NULL) {
        printf("Error opening file for reading!\n");
        return;
    }

    // Open the temporary file for writing
    FILE *tempPointer = fopen(tempFile, "w");
    if (tempPointer == NULL) {
        printf("Error opening temporary file for writing!\n");
        fclose(filePointer);
        return;
    }

    char line[MAX_LINE_LENGTH];
    char storedUsername[MAX_USERNAME_LENGTH];
    int storedElo;
    int found = 0;

    // Process each line
    while (fgets(line, sizeof(line), filePointer)) {
        if (sscanf(line, "%s %d", storedUsername, &storedElo) == 2) {
            if (strcmp(storedUsername, username) == 0) {
                // Update Elo if username matches
                fprintf(tempPointer, "%s %d\n", storedUsername, newElo);
                found = 1;
            } else {
                // Write unchanged line
                fprintf(tempPointer, "%s", line);
            }
        } else {
            // Write lines that don't match the expected format
            fprintf(tempPointer, "%s", line);
        }
    }

    // Append the new user if not found
    if (!found) {
        fprintf(tempPointer, "%s %d\n", username, newElo);
    }

    // Close files
    fclose(filePointer);
    fclose(tempPointer);

    // Replace the original file with the updated file
    if (remove(filename) != 0 || rename(tempFile, filename) != 0) {
        printf("Error updating the file!\n");
        return;
    }

    printf("File updated successfully!\n");
}


int get_user_elo(const char *username) {
    FILE *file = fopen("elo_ratings.txt", "r");
    if (!file) {
        perror("Failed to open Elo ratings file");
        return 1000; // Default Elo if file is not accessible
    }

    char line[256];
    char stored_username[32];
    int stored_elo;

    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%s %d", stored_username, &stored_elo) == 2) {
            if (strcmp(username, stored_username) == 0) {
                printf("tìm thấy tên ở đây này\n");
                fclose(file);
                return stored_elo; // Return Elo if username matches
            }
        }
    }

    fclose(file);
    return 1000; // Default Elo if user not found
}

int initialize_elo(const char *username) {
    FILE *file = fopen("elo_ratings.txt", "a+");
    if (!file) return 0;

    char file_username[50];
    while (fscanf(file, "%s", file_username) != EOF) {
        if (strcmp(username, file_username) == 0) {
            fclose(file);
            return 0;
        }
    }

    fprintf(file, "%s %d\n", username, 1000);
    fclose(file);
    return 1;
}

int validate_login(const char *username, const char *password) {
    FILE *file = fopen(ACCOUNT_FILE, "r");
    if (!file) return 0;

    char file_username[50], file_password[50];
    while (fscanf(file, "%s %s", file_username, file_password) != EOF) {
        if (strcmp(username, file_username) == 0 && strcmp(password, file_password) == 0) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

int register_user(const char *username, const char *password) {
    FILE *file = fopen(ACCOUNT_FILE, "a+");
    if (!file) return 0;

    char file_username[50];
    while (fscanf(file, "%s", file_username) != EOF) {
        if (strcmp(username, file_username) == 0) {
            fclose(file);
            return 0;
        }
    }

    fprintf(file, "%s %s\n", username, password);
    fclose(file);
    return 1;
}



// int handle_client(client_info *client) {
//     char buffer[1024], response[1024];
//     int bytes_received;

//     while (1) {
//         bytes_received = recv(client->socket, buffer, sizeof(buffer), 0);
//         if (bytes_received <= 0) {
//             return -1; // Client disconnected
//         }

//         buffer[bytes_received] = '\0';
//         char *command = strtok(buffer, " ");
//         char *username = strtok(NULL, " ");
//         char *password = strtok(NULL, " ");

//         if (!command || !username || !password) {
//             snprintf(response, sizeof(response), "Invalid command. Use REGISTER <username> <password> or LOGIN <username> <password>.\n");
//             send(client->socket, response, strlen(response), 0);
//             continue;
//         }

//         if (strcmp(command, "REGISTER") == 0) {
//             if (register_user(username, password)) {
//                 snprintf(response, sizeof(response), "Registration successful.\n");
//                 strncpy(client->username, username, sizeof(client->username));
//                 send(client->socket, response, strlen(response), 0);
//                 return 0; // Authenticated
//             } else {
//                 snprintf(response, sizeof(response), "Registration failed. Username already exists.\n");
//             }
//         } else if (strcmp(command, "LOGIN") == 0) {
//             if (validate_login(username, password)) {
//                 snprintf(response, sizeof(response), "Login successful.\n");
//                 strncpy(client->username, username, sizeof(client->username));
//                 send(client->socket, response, strlen(response), 0);
//                 return 0; // Authenticated
//             } else {
//                 snprintf(response, sizeof(response), "Login failed. Invalid credentials.\n");
//             }
//         } else {
//             snprintf(response, sizeof(response), "Unknown command.\n");
//         }

//         send(client->socket, response, strlen(response), 0);
//     }
// }
// Waiting player conditional variable

// Match player
int challenging_player = 0;
int player_is_waiting = 0;

void move_piece(wchar_t ** board, int * move) {
  // Move piece in board from origin to dest
  board[move[2]][move[3]] = board[*move][move[1]];
  board[*move][move[1]] = 0;
}

bool emit(int client, char * message, int message_size) {
  return true;
}

void translate_to_move(int * move, char * buffer) {

  printf("buffer: %s\n", buffer);

  *(move) = 8-(*(buffer+1)-'0');
  move[1] = (*(buffer)-'0')-49;
  move[2] = 8-(*(buffer+4)-'0');
  move[3] = (*(buffer+3)-'0')-49;

  // printf("[%d, %d] to [%d, %d]\n", *(move), move[1], move[2], move[3]);
}

bool is_diagonal_clear(wchar_t ** board, int * move) {

  int * x_moves = (int *)malloc(sizeof(int));
  int * y_moves = (int *)malloc(sizeof(int));

  *(x_moves) = *(move) - move[2];
  *(y_moves) = move[1] - move[3];

  int * index = (int *)malloc(sizeof(int));
  *(index) =  abs(*x_moves) - 1;
  wchar_t * item_at_position = (wchar_t *)malloc(sizeof(wchar_t));

  // Iterate thru all items excepting initial posi
  while (*index > 0) {

    if (*x_moves > 0 && *y_moves > 0) { printf("%lc [%d,%d]\n", board[*move - *index][move[1]- *index],*move - *index,move[1]- *index); *item_at_position = board[*move - *index][move[1]- *index]; }
    if (*x_moves > 0 && *y_moves < 0) { printf("%lc [%d,%d]\n", board[*move - *index][move[1]+ *index],*move - *index,move[1]+ *index); *item_at_position = board[*move - *index][move[1]+ *index]; }
    if (*x_moves < 0 && *y_moves < 0 ) { printf("%lc [%d,%d]\n", board[*move + *index][move[1]+ *index],*move + *index,move[1]+ *index); *item_at_position = board[*move + *index][move[1]+ *index]; }
    if (*x_moves < 0 && *y_moves > 0 ) { printf("%lc [%d,%d]\n", board[*move + *index][move[1]- *index],*move + *index,move[1]- *index); *item_at_position = board[*move + *index][move[1]- *index]; }

    if (*item_at_position != 0) {
      free(index);
      free(x_moves);
      free(y_moves);
      free(item_at_position);
      return false;
    }

    (*index)--;
  }

  free(index);
  free(x_moves);
  free(y_moves);
  free(item_at_position);

  return true;
}

bool is_syntax_valid(int player, char * move) {
  // Look for -
  if (move[2] != '-') { send(player, "e-00", 4, 0); return false; }
  //First and 3th should be characters
  if (move[0]-'0' < 10) { send(player, "e-01", 4, 0); return false; }
  if (move[3]-'0' < 10) { send(player, "e-02", 4, 0); return false; }

  //Second and 5th character should be numbers
  if (move[1]-'0' > 10) { send(player, "e-03", 4, 0); return false; }
  if (move[1]-'0' > 8) { send(player, "e-04", 4, 0); return false; }
  if (move[4]-'0' > 10) { send(player, "e-05", 4, 0); return false; }
  if (move[4]-'0' > 8) { send(player, "e-06", 4, 0); return false; }
  // Move out of range
  if (move[0]-'0' > 56 || move[3]-'0' > 56) { send(player, "e-07", 4, 0); return false; }

  return true;
}

char *invert_board(char *board, int size) {
    // Allocate memory for the inverted board
    char *inverted_board = (char *)malloc(size * sizeof(char));
    
    // Invert the board by reversing the order of elements
    for (int i = 0; i < 64; i++) {
        inverted_board[i] = board[size - 1 - i];
    }

    return inverted_board;
}

void broadcast(wchar_t **board, char *one_dimension_board, int player_one, int player_two) {
    // Convert the 2D board to 1D representation for transmission
    to_one_dimension_char(board, one_dimension_board);
    
    // Send the board to Player One (no inversion needed)
    printf("\tSending board to Player One (%d)\n", player_one);
    send(player_one, one_dimension_board, 64, 0);

    // Invert the board for Player Two and send it
    printf("\tSending inverted board to Player Two (%d)\n", player_two);
    send(player_two, one_dimension_board, 64, 0);

    // Clean up the dynamically allocated memory for the inverted board
    printf("\tSent board to both players...\n");
}

// void broadcast(wchar_t ** board, char * one_dimension_board, int player_one, int player_two) {

//   to_one_dimension_char(board, one_dimension_board);

//   printf("\tSending board to %d and %d size(%lu)\n", player_one, player_two, sizeof(one_dimension_board));
//   send(player_one, one_dimension_board, 64, 0);
//   char *inverted_board = invert_board(one_dimension_board);
//   send(player_two, inverted_board, 64, 0);
//   printf("\tSent board...\n");
// }

int get_piece_team(wchar_t ** board, int x, int y) {

  switch (board[x][y]) {
    case white_king: return -1;
    case white_queen: return -1;
    case white_rook: return -1;
    case white_bishop: return -1;
    case white_knight: return -1;
    case white_pawn: return -1;
    case black_king: return 1;
    case black_queen: return 1;
    case black_rook: return 1;
    case black_bishop: return 1;
    case black_knight: return 1;
    case black_pawn: return 1;
  }

  return 0;

}

void promote_piece(wchar_t ** board, int destX, int destY, int team) {
  if (team == 1) {
    board[destX][destY] = black_queen;
  } else if (team == -1) {
    board[destX][destY] = white_queen;
  }
}

int get_piece_type(wchar_t piece) {

  switch (piece) {
    case white_king: return 0;
    case white_queen: return 1;
    case white_rook: return 2;
    case white_bishop: return 3;
    case white_knight: return 4;
    case white_pawn: return 5;
    case black_king: return 0;
    case black_queen: return 1;
    case black_rook: return 2;
    case black_bishop: return 3;
    case black_knight: return 4;
    case black_pawn: return 5;
  }
  return -1;

}

bool is_rect(int * move) {

  int * x_moves = (int *)malloc(sizeof(int));
  int * y_moves = (int *)malloc(sizeof(int));

  *x_moves = *(move) - move[2];
  *y_moves = move[1] - move[3];

  if ((*x_moves != 0 && *y_moves == 0) || (*y_moves != 0 && *x_moves == 0)) {
    free(x_moves);
    free(y_moves);
    return true;
  }

  free(x_moves);
  free(y_moves);
  return false;
}

int is_rect_clear(wchar_t ** board, int * move, int x_moves, int y_moves ) {

  // Is a side rect
  int * index = (int *)malloc(sizeof(int));

  if (abs(x_moves) > abs(y_moves)) {
    *index = abs(x_moves) - 1;
  } else {
    *index = abs(y_moves) - 1;
  }

  // Iterate thru all items excepting initial position
  while (*index > 0) {

    if (*(move) - move[2] > 0) { if (board[*move-(*index)][move[1]] != 0) { free(index); return false; } }
    if (*(move) - move[2] < 0) { if (board[*move+(*index)][move[1]] != 0) { free(index); return false; } }
    if (move[1] - move[3] > 0 ) { if (board[*move][move[1]-(*index)] != 0) { free(index); return false; } }
    if (move[1] - move[3] < 0 ) { if (board[*move][move[1]+(*index)] != 0) { free(index); return false; } }

    (*index)--;
  }

  free(index);
  return true;

}

bool is_diagonal(int x_moves, int y_moves) {

  if ((abs(x_moves)-abs(y_moves)) != 0) {
    return false;
  }

  return true;
}

int getManitud(int origin, int dest) {
  return (abs(origin-dest));
}

bool eat_piece(wchar_t ** board, int x, int y) {
  return (get_piece_team(board, x, y) != 0);
}

void freeAll(int * piece_team, int * x_moves, int * y_moves) {
  free(piece_team);
  free(x_moves);
  free(y_moves);
}

bool is_move_valid(wchar_t ** board, int player, int team, int * move) {

  int * piece_team = (int *)malloc(sizeof(int *));
  int * x_moves = (int *)malloc(sizeof(int *));
  int * y_moves = (int *)malloc(sizeof(int *));

  *piece_team = get_piece_team(board, *(move), move[1]);
  *x_moves = getManitud(*move, move[2]);
  *y_moves = getManitud(move[1], move[3]);

  // General errors
  if (board[*(move)][move[1]] == 0) {
    send(player, "e-08", 4, 0);
    return false;
  }  // If selected piece == 0 there's nothing selected
  if (*piece_team == get_piece_team(board, move[2], move[3])) {
    send(player, "e-09", 4, 0);
    return false;
  } // If the origin piece's team == dest piece's team is an invalid move

  // Check if user is moving his piece
  if (team != *piece_team) {
    send(player, "e-07", 4, 0);
    return false;
  }

  printf("Player %d(%d) [%d,%d] to [%d,%d]\n", player, *piece_team, move[0], move[1], move[2], move[3]);

  // XMOVES = getManitud(*move, move[2])
  // YMOVES = getManitud(move[1], move[3])
  printf("Moved piece -> %d \n", get_piece_type(board[*(move)][move[1]]));
  switch (get_piece_type(board[*(move)][move[1]])) {
    case 0: /* --- ♚ --- */
      if (*x_moves > 1 || getManitud(move[1], move[3]) > 1) {
        send(player, "e-10", 5, 0);
        freeAll(piece_team, x_moves, y_moves);
        return false;
      }
      break;
    case 1: /* --- ♛ --- */
       if (is_rect(move))
      {
        if (is_rect_clear(board, move, *x_moves, *y_moves))
        {
          if (eat_piece(board, move[2], move[3]))
          {
            send(player, "i-99", 4, 0);
            freeAll(piece_team, x_moves, y_moves);
            return true;
          }
        }
        else
        {
          send(player, "e-31", 4, 0);
          freeAll(piece_team, x_moves, y_moves);
          return false;
        }
      }
      else if (is_diagonal(*x_moves, getManitud(move[1], move[3])))
      {
        if (is_rect_clear(board, move, *x_moves, *y_moves))
        {
          if (eat_piece(board, move[2], move[3]))
          {
            send(player, "i-99", 4, 0);
            freeAll(piece_team, x_moves, y_moves);
            return true;
          }
        }
        else
        {
          if (!is_diagonal_clear(board, move))
          {
            send(player, "e-41", 4, 0);
            freeAll(piece_team, x_moves, y_moves);
            return false;
          }
        }
      }
      else
      {
        send(player, "e-30", 5, 0);
        freeAll(piece_team, x_moves, y_moves);
        return false;
      }
      break;
    case 2: /* --- ♜ --- */
      if (!is_rect(move)) {
        send(player, "e-30", 5, 0);
        freeAll(piece_team, x_moves, y_moves);
        return false;
      }
      if (!is_rect_clear(board, move, *x_moves, *y_moves)) {
        send(player, "e-31", 4, 0);
        freeAll(piece_team, x_moves, y_moves);
        return false;
      }
      if (eat_piece(board, move[2], move[3])) {
        send(player, "i-99", 4, 0);
        freeAll(piece_team, x_moves, y_moves);
        return true;
      }
      break;
    case 3: /* ––– ♝ ––– */
      if (!is_diagonal(*x_moves, getManitud(move[1], move[3]))) {
        send(player, "e-40", 4, 0);
        freeAll(piece_team, x_moves, y_moves);
        return false; // Check if it's a valid diagonal move
      }
      if (!is_diagonal_clear(board, move)) {
        send(player, "e-41", 4, 0);
        freeAll(piece_team, x_moves, y_moves);
        return false;
      }
      if (eat_piece(board, move[2], move[3])) {
        send(player, "i-99", 4, 0);
        freeAll(piece_team, x_moves, y_moves);
        return true;
      }
      break;
    case 4: /* --- ♞ --- */
      if ((abs(*x_moves) != 1 || abs(*y_moves) != 2) && (abs(*x_moves) != 2 || abs(*y_moves) != 1)) {
        send(player, "e-50", 4, 0);
        freeAll(piece_team, x_moves, y_moves);
        return false;
      }
      if (eat_piece(board, move[2], move[3])) {
        send(player, "i-99", 4, 0);
        freeAll(piece_team, x_moves, y_moves);
        return true;
      }
      break;
    case 5: /* --- ♟ --- */
      if (*piece_team == 1 && move[2] == 0) {
        printf("Promoting piece\n");
        promote_piece(board, move[2], move[3], *piece_team);
        send(player, "i-98", 4, 0);
        freeAll(piece_team, x_moves, y_moves);
        return true;
      }
      if (*piece_team == -1 && move[2] == 7) {
        printf("Promoting piece\n");
        promote_piece(board, move[2], move[3], *piece_team);
        send(player, "i-98", 4, 0);
        freeAll(piece_team, x_moves, y_moves);
        return true;
      }
      // Moving in Y axis
      if (getManitud(move[1], move[3]) != 0) {
        if (!is_diagonal(*x_moves, *y_moves) || (get_piece_team(board, move[2], move[3]) == 0)) {
          send(player, "e-60", 4, 0);
          freeAll(piece_team, x_moves, y_moves);
          return false; // Check if it's a diagonal move and it's not an empty location
        }
        if (eat_piece(board, move[2], move[3])) {
          send(player, "i-99", 4, 0);
          freeAll(piece_team, x_moves, y_moves);
          return true; // Check if there's something to eat
        }
      } else {
        // Check if it's the first move
        if (move[0] == 6 && *piece_team == 1 && *x_moves == 2 ) {
          printf("First move\n");
          return true;
        }
        if (move[0] == 1 && *piece_team == -1 && *x_moves == 2 ) {
          printf("First move\n");
          return true;
        }
        if (*x_moves > 1) {
          send(player, "e-62", 5, 0);
          freeAll(piece_team, x_moves, y_moves);
          return false;
        }
      }
      break;
    default:
      break;
  }

  freeAll(piece_team, x_moves, y_moves);
  return true;
}

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


void matchmaking(client_info *client) {
    if (client == NULL) {
        fprintf(stderr, "Error: client_info is NULL.\n");
        return;
    }

    printf("Client %s (Elo: %d) entering matchmaking...\n", client->username, client->elo);

    // Step 1: Try to find a match
    
    int opponent_socket;
    pthread_mutex_lock(&general_mutex);
    for (int j = 0; j < 5; ++ j) {
      opponent_socket = find_match(client->socket, client->elo);
      sleep(5);
    }
    pthread_mutex_unlock(&general_mutex);

    printf("%d\n", opponent_socket);
    if (opponent_socket != -1) {
        printf("Match found for Client %s with opponent socket %d.\n", client->username, opponent_socket);

        pthread_mutex_lock(&general_mutex);

        // Reset opponent's waiting status
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (clients[i].socket == opponent_socket) {
                clients[i].is_waiting = 0;
                break;
            }
        }

        pthread_mutex_unlock(&general_mutex);

        // Start the game room with matched players
        game_room(client->socket, opponent_socket);
        return;
    }

    // Step 2: No match found, fallback to waiting
    printf("No match found. Client %s will wait.\n", client->username);
    pthread_mutex_lock(&general_mutex);

    if (player_is_waiting == 0) {
        // Become Player One
        printf("Client %s is now Player One.\n", client->username);
        player_is_waiting = 1;
        challenging_player = client->socket;

        // Wait for another player
        pthread_cond_wait(&player_to_join, &general_mutex);
    } else {
        // Become Player Two
        printf("Client %s is now Player Two.\n", client->username);
        int player_one_socket = challenging_player;
        int player_two_socket = client->socket;

        // Reset waiting state
        player_is_waiting = 0;
        challenging_player = -1;

        // Signal Player One
        pthread_cond_signal(&player_to_join);
        pthread_mutex_unlock(&general_mutex);

        // Start the game room
        game_room(player_one_socket, player_two_socket);
        return;
    }

    pthread_mutex_unlock(&general_mutex);
}


void game_room(int player_one_socket, int player_two_socket) {
    printf("Starting game between Player One (%d) and Player Two (%d).\n", player_one_socket, player_two_socket);

    int *move = (int *)malloc(sizeof(int) * 4);
    char buffer[64];
    wchar_t **board = create_board();
    char *one_dimension_board = create_od_board();
    initialize_board(board);

    // Notify players of their roles
    // if (send(player_one_socket, "i-p1", 4, 0) < 0 || send(player_two_socket, "i-p2", 4, 0) < 0) {
    //     perror("ERROR sending player roles");
    //     close(player_one_socket);
    //     close(player_two_socket);
    //     free_board(board);
    //     free(one_dimension_board);
    //     free(move);
    //     return;
    // }

    if (send(player_one_socket, "i-p1", 4, 0) < 0) {
     perror("ERROR writing to socket");
     exit(1);
  }
  if (send(player_two_socket, "i-p2", 4, 0) < 0) {
     perror("ERROR writing to socket");
     exit(1);
  }

  sleep(1);

    // Broadcast the initial board
    broadcast(board, one_dimension_board, player_one_socket, player_two_socket);

    bool syntax_valid = false;
    bool move_valid = false;

    while (1) {
        // Player One's turn
        send(player_one_socket, "i-tm", 4, 0);
        send(player_two_socket, "i-nm", 4, 0);

        printf("Waiting for move from Player One (%d)...\n", player_one_socket);
        while (!syntax_valid || !move_valid) {
            bzero(buffer, 64);
            if (read(player_one_socket, buffer, sizeof(buffer)) <= 0) {
                perror("ERROR reading Player One move");
                goto cleanup;
            }
            printf("Player One move: %s\n", buffer);

            syntax_valid = is_syntax_valid(player_one_socket, buffer);
            translate_to_move(move, buffer);  // Convert buffer to move
            move_valid = is_move_valid(board, player_one_socket, 1, move); // Validate the move
        }
        syntax_valid = false;
        move_valid = false;

        move_piece(board, move);  // Apply Player One's move
        broadcast(board, one_dimension_board, player_one_socket, player_two_socket);

        // Player Two's turn
        send(player_one_socket, "i-nm", 4, 0);
        send(player_two_socket, "i-tm", 4, 0);

        printf("Waiting for move from Player Two (%d)...\n", player_two_socket);
        while (!syntax_valid || !move_valid) {
            bzero(buffer, 64);
            if (read(player_two_socket, buffer, sizeof(buffer)) <= 0) {
                perror("ERROR reading Player Two move");
                goto cleanup;
            }
            printf("Player Two move: %s\n", buffer);

            syntax_valid = is_syntax_valid(player_two_socket, buffer);
            translate_to_move(move, buffer);  // Convert buffer to move
            move_valid = is_move_valid(board, player_two_socket, -1, move); // Validate the move
        }
        syntax_valid = false;
        move_valid = false;

        move_piece(board, move);  // Apply Player Two's move
        broadcast(board, one_dimension_board, player_one_socket, player_two_socket);
        sleep(1);
    }

cleanup:
    free_board(board);
    free(one_dimension_board);
    free(move);
    close(player_one_socket);
    close(player_two_socket);
}

void *handle_client(void *arg) {
    client_info *client = (client_info *)arg;
    char buffer[1024], response[1024];
    int bytes_received;
    int elo = 1000; // Default Elo for new users

    // Authentication phase
    while (1) {
        bytes_received = recv(client->socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            printf("Client disconnected during authentication.\n");
            close(client->socket);
            return NULL;
        }

        buffer[bytes_received] = '\0';
        char *command = strtok(buffer, " ");
        char *username = strtok(NULL, " ");
        char *password = strtok(NULL, " ");

        if (strcmp(command, "REGISTER") == 0) {
            if (register_user(username, password)) {
                int x = initialize_elo(username);
                snprintf(response, sizeof(response), "Registration successful.\n");
                strncpy(client->username, username, sizeof(client->username));
                add_online_player(client->socket, username, elo, 1); // Add player with default Elo
                send(client->socket, response, strlen(response), 0);
                break;
            } else {
                snprintf(response, sizeof(response), "Registration failed. Username exists.\n");
            }
        } else if (strcmp(command, "LOGIN") == 0) {
            if (validate_login(username, password)) {
                elo = get_user_elo(username); // Fetch Elo from database or file
                int x = initialize_elo(username);
                snprintf(response, sizeof(response), "Login successful.\n");
                strncpy(client->username, username, sizeof(client->username));
                add_online_player(client->socket, username, elo, 1);
                send(client->socket, response, strlen(response), 0);
                break;
            } else {
                snprintf(response, sizeof(response), "Login failed. Invalid credentials.\n");
            }
        } else {
            snprintf(response, sizeof(response), "Invalid command. Use REGISTER or LOGIN.\n");
        }
        send(client->socket, response, strlen(response), 0);
    }

    printf("Client %s authenticated with Elo %d.\n", client->username, elo);

    // Enter matchmaking
    matchmaking(client);

    // Cleanup when the client disconnects
    remove_online_player(client->socket);
    close(client->socket);

    return NULL;
}



int is_duplicate_socket(int socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == socket) {
            return 1; // Found a duplicate
        }
    }
    return 0; // No duplicates
}



int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "en_US.UTF-8");

    int sockfd, client_socket, port_number, client_length;
    struct sockaddr_in server_address, client;
    
    
    pthread_t client_threads[MAX_CLIENTS];
    pthread_cond_init(&player_to_join, NULL);
    pthread_mutex_init(&general_mutex, NULL);

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    // Initialize socket structure
    bzero((char *)&server_address, sizeof(server_address));
    port_number = PORT;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port_number);

    // Bind the socket
    if (bind(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    // Listen for incoming connections
    listen(sockfd, 20);
    printf("Server listening on port %d\n", port_number);

    while (1) {
        client_length = sizeof(client);

        // Accept a client connection
        client_socket = accept(sockfd, (struct sockaddr *)&client, (unsigned int *)&client_length);
        if (client_socket < 0) {
            perror("ERROR on accept");
            continue;
        }

        printf("Connection accepted from %d.%d.%d.%d:%d\n",
               client.sin_addr.s_addr & 0xFF,
               (client.sin_addr.s_addr & 0xFF00) >> 8,
               (client.sin_addr.s_addr & 0xFF0000) >> 16,
               (client.sin_addr.s_addr & 0xFF000000) >> 24,
               ntohs(client.sin_port));
        

        // Create a thread to handle authentication
        pthread_mutex_lock(&general_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == 0) {  // Ensure the client slot is correctly 
                clients[i].socket = client_socket;
                clients[i].address = client;
                clients[i].index = i;
                pthread_create(&client_threads[i], NULL, handle_client, &clients[i]);
                break;
            }
        }
        pthread_mutex_unlock(&general_mutex);
    }
    close(sockfd);
    return 0;
}


// int main( int argc, char *argv[] ) {
//   pthread_t tid[1];
//   setlocale(LC_ALL, "en_US.UTF-8");

//   int sockfd, client_socket, port_number, client_length;
//   char buffer[64];
//   struct sockaddr_in server_address, client;
//   int  n;

//   // Conditional variable
//   pthread_cond_init(&player_to_join, NULL);
//   pthread_mutex_init(&general_mutex, NULL);

//   /* First call to socket() function */
//   sockfd = socket(AF_INET, SOCK_STREAM, 0);

//   if (sockfd < 0) {
//     perror("ERROR opening socket");
//     exit(1);
//   }

//   /* Initialize socket structure */
//   bzero((char *) &server_address, sizeof(server_address));
//   port_number = PORT;

//   server_address.sin_family = AF_INET;
//   server_address.sin_addr.s_addr = INADDR_ANY;
//   server_address.sin_port = htons(port_number);

//   /* Now bind the host address using bind() call.*/
//   if (bind(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
//       perror("ERROR on binding");
//       exit(1);
//    }

//           /* MAX_QUEUE */
//   listen(sockfd, 20);
//   printf("Server listening on port %d\n", port_number);
//   pthread_t threads[MAX_CLIENTS];
//    while(1) {
//      client_length = sizeof(client);
//      // CHECK IF WE'VE A WAITING USER

//      /* Accept actual connection from the client */
//      client_socket = accept(sockfd, (struct sockaddr *)&client, (unsigned int *)&client_length);

//      if (client_socket < 0) {
//         perror("ERROR on accept");
//         exit(1);
//      }

//      pthread_mutex_lock(&general_mutex);
//         int i;
//         for (i = 0; i < MAX_CLIENTS; i++) {
//             if (clients[i].socket == 0) {
//                 clients[i].socket = client_socket;
//                 clients[i].address = client;
//                 clients[i].index = i;
//                 pthread_create(&threads[i], NULL, handle_client, &clients[i]);
//                 break;
//             }
//         }
//         pthread_mutex_unlock(&general_mutex);
//       printf("– Connection accepted from %d at %d.%d.%d.%d:%d –\n", client_socket, client.sin_addr.s_addr&0xFF, (client.sin_addr.s_addr&0xFF00)>>8, (client.sin_addr.s_addr&0xFF0000)>>16, (client.sin_addr.s_addr&0xFF000000)>>24, client.sin_port);

//      pthread_mutex_lock(&general_mutex); // Unecesary?
//      // Create thread if we have no user waiting
//      if (player_is_waiting == 0) {
//        printf("Connected player, creating new game room...\n");
//        pthread_create(&tid[0], NULL, &game_room, &client_socket);
//        pthread_mutex_unlock(&general_mutex); // Unecesary?
//      }
//      // If we've a user waiting join that room
//      else {
//        // Send user two signal
//        printf("Connected player, joining game room... %d\n", client_socket);
//        challenging_player = client_socket;
//        pthread_mutex_unlock(&general_mutex); // Unecesary?
//        pthread_cond_signal(&player_to_join);
//      }

//    }

//    close(sockfd);

//    return 0;
// }
