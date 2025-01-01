#include "server.h"
#include <time.h>

pthread_mutex_t general_mutex;
client_info clients[MAX_CLIENTS] = {0};

client_info *matchmaking_list[MAX_PLAYERS];
int list_size = 0;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t list_cond = PTHREAD_COND_INITIALIZER;

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

void add_to_list(client_info *client) {
    // Ensure no duplicate entries
    for (int i = 0; i < list_size; i++) {
        if (matchmaking_list[i]->socket == client->socket) {
            printf("Client %s is already in matchmaking.\n", client->username);
            return;
        }
    }

    // Add to the list if space is available
    if (list_size < MAX_PLAYERS) {
        matchmaking_list[list_size++] = client;
        client->ready = 1;
        client->is_waiting = 1;
        printf("Client %s added to matchmaking list.\n", client->username);
    } else {
        printf("Matchmaking list is full! Client %s cannot join.\n", client->username);
    }
}

client_info *find_and_remove_match(client_info *client) {
    client_info *best_match = NULL;
    int closest_elo_diff = 10000000;

    for (int i = 0; i < list_size; i++) {
        client_info *potential_match = matchmaking_list[i];

        if (potential_match->socket != client->socket &&
            potential_match->is_waiting &&
            potential_match->ready) {

            int elo_diff = abs(client->elo - potential_match->elo);
            if (elo_diff < closest_elo_diff && elo_diff <= 100) {
                best_match = potential_match;
                closest_elo_diff = elo_diff;
            }
        }
    }

    if (best_match != NULL) {
        // Remove both the matched player and the client from the list
        for (int i = 0; i < list_size; i++) {
            if (matchmaking_list[i] == best_match || matchmaking_list[i] == client) {
                for (int j = i; j < list_size - 1; j++) {
                    matchmaking_list[j] = matchmaking_list[j + 1];
                }
                list_size--;
                i--;  // Adjust the index after removal
            }
        }

        best_match->is_waiting = 0;
        best_match->ready = 0;
    }

    return best_match;
}


void matchmaking(client_info *client) {
    if (client == NULL) {
        fprintf(stderr, "Error: client_info is NULL.\n");
        return;
    }

    printf("Client %s (Elo: %d) entering matchmaking...\n", client->username, client->elo);

    // Step 1: Add the client to the matchmaking list
    pthread_mutex_lock(&list_mutex);
    add_to_list(client);

    // Notify other threads that a player has joined
    pthread_cond_signal(&list_cond);
    pthread_mutex_unlock(&list_mutex);

    while (1) {
        pthread_mutex_lock(&list_mutex);

        // Wait until there are at least two players in the list
        while (list_size < 2) {
            printf("Waiting for more players...\n");
            pthread_cond_wait(&list_cond, &list_mutex);
        }

        pthread_mutex_unlock(&list_mutex);

        // Step 2: Try to find a match
        pthread_mutex_lock(&list_mutex);
        client_info *opponent = find_and_remove_match(client);
        pthread_mutex_unlock(&list_mutex);

        if (opponent != NULL) {
            // Step 3: Start the game if a match is found
            printf("Match found! %s (Elo: %d) will play against %s (Elo: %d).\n",
                   client->username, client->elo, opponent->username, opponent->elo);

            game_room(client->socket, opponent->socket);
            return;
        }

        // Step 4: No match found, retry after a brief delay
        printf("No suitable match found for %s. Retrying...\n", client->username);
        sleep(1);
    }
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
