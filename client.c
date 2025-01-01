#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <string.h>

#include "board.c"

#define RED   "\x1B[31m"
#define RESET "\x1B[0m"
#define GREEN  "\x1B[32m"
#define YELLOW "\x1B[33m"
char input_buffer[1024]; 
bool isPrint = true;
bool isIngame = false;
char menu_buffer[1024];
char challenge_buffer = '#';
pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;
char challenge_response = '\0';
void *get_input(void *sockfd_ptr) {
    while (1) {
      pthread_mutex_lock(&input_mutex);
        fgets(input_buffer, sizeof(input_buffer), stdin);  // Non-blocking input

        // Remove newline character at the end of the input
        input_buffer[strcspn(input_buffer, "\n")] = '\0';
        if(input_buffer[0] <= '9' && input_buffer[0]>='0')
        strcpy(menu_buffer, input_buffer);
        if(input_buffer[0] == 'y' || input_buffer[0] == 'Y' || input_buffer[0] == 'n' || input_buffer[0] == 'N')
          challenge_buffer = input_buffer[0];
      pthread_mutex_unlock(&input_mutex);
        // Sleep for a brief moment to prevent busy-waiting
        usleep(100000);  // Sleep for 100 milliseconds
    }
}
void *send_moves(void *sockfd_ptr) {
    int socket = *(int *)sockfd_ptr; // Extract the socket file descriptor
    char move[64]; // Buffer to store the move to send to the server

    while (1) {
        // Only operate if the game is in progress
        if (isIngame) {
          pthread_mutex_lock(&input_mutex);
            // Check if there's input in the input_buffer
            if (strlen(input_buffer) > 0) {
                // Copy the move from input_buffer
                strncpy(move, input_buffer, sizeof(move) - 1);
                move[sizeof(move) - 1] = '\0'; // Ensure null-termination

                // Clear the input buffer
                memset(input_buffer, 0, sizeof(input_buffer));
          pthread_mutex_unlock(&input_mutex);
                // Send the move to the server
                if (send(socket, move, strlen(move), 0) < 0) {
                    perror("ERROR sending move to server");
                    exit(1);
                }

                printf("Move sent: %s\n", move);
            }
        }

        // Sleep briefly to avoid busy-waiting
        usleep(100000); // Sleep for 100 milliseconds
    }

    return NULL; // Thread cleanup
}
void * on_signal(void * sockfd) {
  char buffer[256];
  int n;
  int socket = *(int *)sockfd;
  int * player = (int *)malloc(sizeof(int *));
  while (1) {
    bzero(buffer, 256);
    n = read(socket, buffer, 256);

    if (n < 0) {
       perror("ERROR reading from socket");
       exit(1);
    }
     if (strstr(buffer, "Challenge from")) {
            printf("\n%s\n", buffer);


            // Wait for the user to respond to the challenge (Non-blocking)
            printf("\nDo you accept the challenge? (y/n): \n");
            while (challenge_response == '\0') {
                // Check the input_buffer for a response asynchronously
                if (challenge_buffer != '#') {
                    challenge_response = challenge_buffer; // Get the first character of input 
                }
                usleep(100000); // Sleep to prevent busy-waiting
            }
            printf("\n Challenge Response: %c \n", challenge_response);
            send(socket, &challenge_response, sizeof(challenge_response), 0);

            if (challenge_response == 'y' || challenge_response == 'Y') {
                printf("\nGame starting...\n");
                isIngame = true;
            } else {
                printf("\nYou declined the challenge.\n");
            }
            bzero(buffer, 256);
            memset(input_buffer, 0, sizeof(input_buffer));
            challenge_buffer = '#';
	    challenge_response == '\0';
            continue;
        }
    if (buffer[0] == 'i' || buffer[0] == 'e' || buffer[0] == '\0') {
      if (buffer[0] == 'i') {
        if (buffer[2] == 't') {
          isIngame = true;
          printf("\nMake your move: \n");
        }
        if (buffer[2] == 'n') {
          isIngame = true;
          printf("\nWaiting for opponent...\n");
        }
        if (buffer[2] == 'l') {
          system("clear");
          printf(RED "\nYou lose...\n" RESET);
          isIngame = false;
          isPrint = true;
          // if (!rematch(sockfd)) {
          //   printf("Rematch not accepted. Exiting.\n");
          //   close(sockfd);
          // }
        }
        if (buffer[2] == 'w'){
          system("clear");
          printf(GREEN "\nYou win...\n" RESET);
          isIngame = false;
          isPrint = true;
        }
        if (buffer[2] == 'd'){
          printf(YELLOW"\nDraw...\n" RESET);
          // if (!rematch(sockfd)) {
          //   printf("Rematch not accepted. Exiting.\n");
          //   close(sockfd);
          // }
        }
        if (buffer[2] == 'p') {
          *player = atoi(&buffer[3]);
          if (*player == 2) {
            printf("You're blacks (%c)\n", buffer[3]);
          } else {
            printf("You're whites (%c)\n", buffer[3]);
          }
        }
      }
      else if (buffer[0] == 'e') {
        // Syntax errors
        if (buffer[2] == '0') {
          switch (buffer[3]) {
            case '0':
              printf(RED "  ↑ ('-' missing)\n" RESET);
              break;
            case '1':
              printf(RED "↑ (should be letter)\n" RESET);
              break;
            case '2':
              printf(RED "   ↑ (should be letter)\n" RESET);
              break;
            case '3':
              printf(RED " ↑ (should be number)\n" RESET);
              break;
            case '4':
              printf(RED " ↑ (out of range)\n" RESET);
              break;
            case '5':
              printf(RED "   ↑ (should be number)\n" RESET);
              break;
            case '6':
              printf(RED "   ↑ (out of range)\n" RESET);
              break;
            case '7':
              printf(RED "(out of range)\n" RESET);
              break;
          }
        }
        printf("\nerror %s\n", buffer);
      }
      // Check if it's an informative or error message
    } else {
      // Print the board
      system("clear");
      if (*player == 1) {
        print_board_buff(buffer);
      } else {
        print_board_buff_inverted(buffer);
      }
    }

    bzero(buffer, 256);
  }
}

int authenticate(int socket) {
    char buffer[1024];
    char command[10], username[50], password[50];
    int choice;

    while (1) {
        printf("1. Register\n");
        printf("2. Login\n");
        printf("3. Exit\n");
        printf("Choose an option (1/2/3): ");
        scanf("%d", &choice);
        getchar();

        if (choice == 1) {
            strcpy(command, "REGISTER");
        } else if (choice == 2) {
            strcpy(command, "LOGIN");
        } else if (choice == 3) {
            printf("Exiting client.\n");
            return 0; // Exit the client application
        } else {
            printf("Invalid choice. Try again.\n");
            continue;
        }

        printf("Enter username: ");
        scanf("%s", username);
        printf("Enter password: ");
        scanf("%s", password);
        getchar();

        snprintf(buffer, sizeof(buffer), "%s %s %s", command, username, password);
        send(socket, buffer, strlen(buffer), 0);

        int bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            printf("Disconnected from server.\n");
            return 0;
        }

        buffer[bytes_received] = '\0';
        printf("%s", buffer);

        // Return 1 for successful authentication, 0 for logout or failure
        if (strstr(buffer, "successful")) {
            return 1;
        } else if (strstr(buffer, "logged out")) {
            return 0; // Logout case
        }
    }
}


int rematch(int socket){
  char buffer[1024];
  char command[20];
  int choice;
  printf("\n Send rematch request? (1 for YES, 0 for NO)");
  scanf("%d", &choice);
  if(choice == 0) strcpy(command, "REMATCH");
  else strcpy(command, "NOT_REMATCH");

  snprintf(buffer, sizeof(buffer), "%s %s %s", command);
  send(socket, buffer, strlen(buffer), 0);

  int bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0);
  if (bytes_received <= 0) {
      printf("Disconnected from server.\n");
      return 0;
  }

  buffer[bytes_received] = '\0';
  printf("%s", buffer);

  return strstr(buffer, "successful") != NULL;
}

int main(int argc, char *argv[]) {
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    setlocale(LC_ALL, "en_US.UTF-8");
    char buffer[1024];

    if (argv[2] == NULL) {
        portno = 80;
    } else {
        portno = atoi(argv[2]);
    }

    printf("Connecting to %s:%d\n", argv[1], portno);

    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }

    printf("Connected to server.\n");
    while (1) {
        // Authenticate the user
        if (!authenticate(sockfd)) {
            printf("Exiting client...\n");
            close(sockfd);
            return 0; // Exit the application if authentication fails or user logs out
        }

        printf("Authentication successful.\n");
        char input[10];
        int choice;
        pthread_t tid[1];
        pthread_create(&tid[0], NULL, &on_signal, &sockfd);
        pthread_t input_thread;
        pthread_create(&input_thread, NULL, &get_input, &sockfd);
        pthread_t send_moves_thread;
        pthread_create(&send_moves_thread, NULL, &send_moves, &sockfd);
        while (1) {
          if(isPrint)
          {
            printf("1. Matchmaking\n");
            printf("2. Online Players\n");
            printf("3. Send Challenge Request\n");
            printf("4. Logout\n");
            printf("Choose an option (1/2/3/4): \n");
            isPrint = false;
          }
          char input[10];

          if (strlen(menu_buffer) > 0 && !isIngame) {
            int choice = atoi(menu_buffer);
            memset(input_buffer, 0, sizeof(input_buffer));
            memset(menu_buffer, 0, sizeof(menu_buffer));
            if (choice == 1) {
                snprintf(buffer, sizeof(buffer), "1");
                send(sockfd, buffer, strlen(buffer), 0);
                printf("Entering matchmaking...\n");

            }  else if (choice == 2) {
              snprintf(buffer, sizeof(buffer), "2");
              send(sockfd, buffer, strlen(buffer), 0);

              // Receive and print the list of online players
              int bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
              if (bytes_received <= 0) {
                  printf("Disconnected from server.\n");
                  close(sockfd);
                  return 0;
              }

              buffer[bytes_received] = '\0';
              printf("Online Players:\n%s\n", buffer);
              isPrint = true;
          }
          else if (choice == 3) {
              char target_username[50];
              printf("Enter the username of the player to challenge: ");
              fgets(target_username, sizeof(target_username), stdin);
              target_username[strcspn(target_username, "\n")] = '\0'; // Remove newline

              // Send choice and username to the server
              snprintf(buffer, sizeof(buffer), "3");
              send(sockfd, buffer, strlen(buffer), 0);
              send(sockfd, target_username, strlen(target_username) + 1, 0);

              // Receive response from the server
              int bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
              if (bytes_received <= 0) {
                  printf("Disconnected from server.\n");
                  close(sockfd);
                  return 0;
              }

              buffer[bytes_received] = '\0';
              printf("%s\n", buffer);
              if(strstr(buffer, "Game start")){
                isIngame = true;
              }
              else {
                isPrint = true;
              }
          }
          else if (choice == 4) {
                snprintf(buffer, sizeof(buffer), "4");
                send(sockfd, buffer, strlen(buffer), 0);

                int bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
                if (bytes_received <= 0) {
                    printf("Disconnected from server.\n");
                    close(sockfd);
                    return 0;
                }

                buffer[bytes_received] = '\0';
                printf("%s\n", buffer);

                if (strstr(buffer, "Logging out")) {
                    printf("Logged out successfully.\n");
                    return 0;
                }
            } else {
                printf("Invalid choice. Try again.\n");
            }
          }
      }

    }

    close(sockfd);
    return 0;
}

