#include "server.h"
#include <time.h>

pthread_cond_t player_to_join;
pthread_mutex_t general_mutex;
client_info clients[MAX_CLIENTS] = {0};

// Function to create the log file name with full date
void create_log_filename(char *filename, size_t size, const char *username1, const char *username2) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    // Format the filename as YYYYMMDD-username1-username2.log
    snprintf(filename, size, "logs/%04d%02d%02d-%02d%02d%02d--%s-%s.log",
            tm.tm_year + 1900,  // Year
            tm.tm_mon + 1,      // Month (tm_mon is 0-based)
            tm.tm_mday,         // Day
            tm.tm_hour,         // Hour (24-hour format)
            tm.tm_min,          // Minute
            tm.tm_sec,          // Second
            username1, username2);
}


const char *get_username_by_socket(int socket, client_info *clients) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == socket) {
            return clients[i].username;
        }
    }
    return "unknown"; // Fallback if username not found
}


// Match player
int challenging_player = 0;
int player_is_waiting = 0;

void wait_for_game_start(int socket) {
    char buffer[64];
    while (1) {
        bzero(buffer, sizeof(buffer)); // Clear buffer
        ssize_t bytes_read = recv(socket, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
        printf("ByteRead is: %zd\n", bytes_read);
        printf("HERE is buffer: %s\n", buffer);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Null-terminate the buffer
            printf("HERE is buffer: %s\n", buffer);
            if (strcmp(buffer, "game_ready") == 0) {
                printf("Client %d received game ready signal. Joining the game.\n", socket);
                return;
            }
        }
        sleep(1); // Polling delay
    }
}
void matchmaking(client_info *client) {
    if (client == NULL) {
        fprintf(stderr, "Error: client_info is NULL.\n");
        return;
    }

    printf("Client %s (Elo: %d) entering matchmaking...\n", client->username, client->elo);
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (clients[i].socket == client->socket) {
                clients[i].ready = 1;
            }
    }
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
             printf("Client %s is now Player Two.\n", client->username);
            int player_one_socket = challenging_player;
            int player_two_socket = client->socket;

            // Reset waiting state
            player_is_waiting = 0;
            challenging_player = -1;

            // Signal Player One
            pthread_cond_signal(&player_to_join);
            pthread_mutex_unlock(&general_mutex);

            // Start the game room with Player 1 and Player 2
            game_room(client->socket, opponent_socket);
            return;
        } else {
            // Opposite assignment when user2_elo > user1_elo
            // printf("Client %s is Player Two.\n", client->username);
            // printf("Opponent %d is Player One.\n", opponent_socket);

            // // Start the game room with Player 1 and Player 2
            // game_room(opponent_socket, client->socket);

            printf("Client %s is now Player Two.\n", client->username);
            player_is_waiting = 1;
            challenging_player = client->socket;

            // Wait for another player
            pthread_cond_wait(&player_to_join, &general_mutex);
        }
        pthread_mutex_unlock(&general_mutex);
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
    // if (send(player_two_socket, "game_ready", strlen("game_ready") + 1, 0) < 0) {
    //     perror("ERROR sending game ready signal");
    // }

    time_t start_time = time(NULL);
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

    const char *username1 = get_username_by_socket(player_one_socket, clients);
    const char *username2 = get_username_by_socket(player_two_socket, clients);

    printf("Make the log file start now!\n");

    // Create log file with full date format
    char log_filename[128];
    create_log_filename(log_filename, sizeof(log_filename), username1, username2);

    FILE *log_file = fopen(log_filename, "w");
    if (!log_file) {
        perror("ERROR opening log file");
        return;
    }

    fprintf(log_file, "Game started between %s and %s.\n", username1, username2);

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

        // Calcute start_time - end_time
        time_t end_time = time(NULL);
        double total_time = difftime(end_time, start_time);
        int total_seconds = (int)total_time;
        int hours = total_seconds / 3600;
        int minutes = (total_seconds % 3600) / 60;
        int seconds = total_seconds % 60;
        printf("Total match time: %02d:%02d:%02d\n", hours, minutes, seconds);
        printf("Total match time: %.0f seconds\n", total_time);

        while (!syntax_valid || !move_valid) {
            // Check win/draw before reading new move
            if (check_win_game(board, player_one_socket, player_two_socket, total_seconds)) {
                goto cleanup;
            }
            sleep(1);

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
            fprintf(log_file, "Player One move: %s\n", buffer);
            fflush(log_file);
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
            time_t end_time = time(NULL);
            // Calculate total duration
            double total_time = difftime(end_time, start_time);
            int total_seconds = (int)total_time;
            int hours = total_seconds / 3600;
            int minutes = (total_seconds % 3600) / 60;
            int seconds = total_seconds % 60;
            printf("Total match time: %02d:%02d:%02d\n", hours, minutes, seconds);
            printf("Total match time: %.0f seconds\n", total_time);

            // Check win/draw before reading new move
            if (check_win_game(board, player_one_socket, player_two_socket, total_seconds)) {
                goto cleanup;
            }
            sleep(1);

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
            fprintf(log_file, "Player Two move: %s\n", buffer);
            fflush(log_file);
            syntax_valid = is_syntax_valid(player_two_socket, buffer);

            translate_to_move(move, buffer);
            move_valid = is_move_valid(board, player_two_socket, -1, move);
        }


        syntax_valid = false;
        move_valid = false;

        move_piece(board, move);
        broadcast(board, one_dimension_board, player_one_socket, player_two_socket);
        print_board(board);
    }

cleanup:
    free_board(board);
    free(one_dimension_board);
    free(move);
    if (log_file) {
        fclose(log_file);  // Close the file properly
    }
}

void update_client_status_in_file(const char *filename, const char *username, int is_online) {
    FILE *file = fopen(filename, "r+");

    // If the file doesn't exist, create it
    if (!file) {
        file = fopen(filename, "w+");
        if (!file) {
            perror("ERROR opening or creating file");
            return;
        }
    }

    char line[256];
    long pos;
    int found = 0;

    // Read the file to find the username and update the status
    while (fgets(line, sizeof(line), file)) {
        pos = ftell(file); // Save the current position
        char name[50];
        int status;

        // Parse username and status
        if (sscanf(line, "%s %d", name, &status) == 2) {
            if (strcmp(name, username) == 0) {
                // Match found, update the status
                found = 1;
                fseek(file, pos - strlen(line), SEEK_SET); // Go back to the start of the line
                fprintf(file, "%s %d\n", name, is_online);
                fflush(file);
                break;
            }
        }
    }

    fclose(file);
}


void *handle_client(void *arg) {
    client_info *client = (client_info *)arg;
    char buffer[1024], response[1024];
    int bytes_received;
    int elo = 1000; // Default Elo for new users

    while (1) {
        bytes_received = recv(client->socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            printf("HEEHEHEHHEHE: %s\n", client->username);
            remove_online_player(client->socket);
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
                add_online_player(client->socket, username, elo, 1);
                // update_client_status_in_file("client_status.log", username, 1);
                send(client->socket, response, strlen(response), 0);
                break;
            } else {
                snprintf(response, sizeof(response), "Registration failed. Username exists.\n");
            }
        } else if (strcmp(command, "LOGIN") == 0) {

            if (validate_login(username, password)) {
                elo = get_user_elo(username);
                int x = initialize_elo(username);
                snprintf(response, sizeof(response), "Login successful.\n");
                strncpy(client->username, username, sizeof(client->username));
                // update_client_status_in_file("client_status.log", username, 1);
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

    while (1) {
        bytes_received = recv(client->socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            update_client_status_in_file("client_status.log", client->username, 0);
            printf("Client %d with name is %s disconnected.\n", client->socket, client->username);
            close(client->socket);
            return NULL;
        }

        buffer[bytes_received] = '\0';
        int choice = atoi(buffer);

        if (choice == 1) {
            // Enter matchmaking
            matchmaking(client);
        } else if (choice == 2) {
            snprintf(response, sizeof(response), "Logging out...\n");
            send(client->socket, response, strlen(response), 0);
            printf("Client %s logged out.\n", client->username);
            remove_online_player(client->socket);
            ///update_client_status_in_file("client_status.log", client->username, 0);
            return NULL; // Break out of the loop and close connection
        } else {
            printf("Invalid choice received: %s\n", buffer); // Debugging line
            snprintf(response, sizeof(response), "Invalid choice. Try again.\n");
            send(client->socket, response, strlen(response), 0);
        }
    }
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
                clients[i].is_online = 1;
                pthread_create(&client_threads[i], NULL, handle_client, &clients[i]);
                break;
            }
        }
        pthread_mutex_unlock(&general_mutex);
    }
    close(sockfd);
    return 0;
}
