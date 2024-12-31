#include "server.h"

pthread_cond_t player_to_join;
pthread_mutex_t general_mutex;
client_info clients[MAX_CLIENTS] = {0};


// Match player
int challenging_player = 0;
int player_is_waiting = 0;

bool emit(int client, char * message, int message_size) {
  return true;
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
    for (int j = 0; j < 5; ++j) {
        opponent_socket = find_match(client->socket, client->elo);
        printf("Opponent Socket: %d\n", opponent_socket);
        if (opponent_socket != -1) {
            printf("Match found! %s will play against opponent with socket %d.\n", client->username, opponent_socket);
            break;
        }
        sleep(5);
    }
    pthread_mutex_unlock(&general_mutex);

    if (opponent_socket != -1) {
        int user1_elo = client->elo;
        int user2_elo;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (clients[i].socket == opponent_socket) {
                user2_elo = get_user_elo(clients[i].username);
                break;
            }
        }
         if (user1_elo <= user2_elo) {
            printf("Client %s is Player One.\n", client->username);
            printf("Opponent %d is Player Two.\n", opponent_socket);

            // Start the game room with Player 1 and Player 2
            game_room(client->socket, opponent_socket);
        } else {
            // Opposite assignment when user2_elo > user1_elo
            printf("Client %s is Player Two.\n", client->username);
            printf("Opponent %d is Player One.\n", opponent_socket);

            // Start the game room with Player 1 and Player 2
            game_room(opponent_socket, client->socket);
        }
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
    print_board(board);

    while (1) {
        // Player One's turn
        send(player_one_socket, "i-tm", 4, 0);
        send(player_two_socket, "i-nm", 4, 0);
        sleep(1);

        printf("Waiting for move from Player One (%d)...\n", player_one_socket);
        while (!syntax_valid || !move_valid) {
            // Check win/draw before reading new move
            if (check_win_game(board, player_one_socket, player_two_socket)) {
                goto cleanup;
            }
            // if (check_draw_game(board, player_one_socket, player_two_socket)) {
            //     goto cleanup;
            // }
            bzero(buffer, 64);

            ssize_t bytes_read = read(player_one_socket, buffer, sizeof(buffer));
            if (bytes_read <= 0) {
                perror("ERROR reading Player One move");
                goto cleanup;
            }

            printf("Player One move: %s\n", buffer);
            syntax_valid = is_syntax_valid(player_one_socket, buffer);
            translate_to_move(move, buffer);
            move_valid = is_move_valid(board, player_one_socket, 1, move);
        }

        syntax_valid = false;
        move_valid = false;
        move_piece(board, move);
        broadcast(board, one_dimension_board, player_one_socket, player_two_socket);
        print_board(board);

        // Player Two's turn
        send(player_one_socket, "i-nm", 4, 0);
        send(player_two_socket, "i-tm", 4, 0);
        sleep(1);

        printf("Waiting for move from Player Two (%d)...\n", player_two_socket);
        while (!syntax_valid || !move_valid) {
            // Check win/draw before reading new move
            if (check_win_game(board, player_one_socket, player_two_socket)) {
                goto cleanup;
            }
            // if (check_draw_game(board, player_two_socket, player_two_socket)) {
            //     goto cleanup;
            // }

            bzero(buffer, 64);

            ssize_t bytes_read = read(player_two_socket, buffer, sizeof(buffer));
            if (bytes_read <= 0) {
                perror("ERROR reading Player Two move");
                goto cleanup;
            }

            printf("Player Two move: %s\n", buffer);
            syntax_valid = is_syntax_valid(player_two_socket, buffer);

            translate_to_move(move, buffer);
            move_valid = is_move_valid(board, player_two_socket, -1, move);
        }

        syntax_valid = false;
        move_valid = false;

        move_piece(board, move);
        broadcast(board, one_dimension_board, player_one_socket, player_two_socket);
        print_board(board);
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
