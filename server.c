#include "server.h"

pthread_cond_t player_to_join;
pthread_mutex_t general_mutex;
client_info clients[MAX_CLIENTS] = {0};


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

bool emit(int client, char * message, int message_size) {
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

// void broadcast(wchar_t ** board, char * one_dimension_board, int player_one, int player_two) {

//   to_one_dimension_char(board, one_dimension_board);

//   printf("\tSending board to %d and %d size(%lu)\n", player_one, player_two, sizeof(one_dimension_board));
//   send(player_one, one_dimension_board, 64, 0);
//   char *inverted_board = invert_board(one_dimension_board);
//   send(player_two, inverted_board, 64, 0);
//   printf("\tSent board...\n");
// }


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
