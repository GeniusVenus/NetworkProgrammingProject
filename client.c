#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <netdb.h>
#include <netinet/in.h>

#include <string.h>

#include "board.c"

#define RED   "\x1B[31m"
#define RESET "\x1B[0m"
#define GREEN  "\x1B[32m"

void * on_signal(void * sockfd) {
  char buffer[64];
  int n;
  int socket = *(int *)sockfd;
  int * player = (int *)malloc(sizeof(int *));

  while (1) {
    bzero(buffer, 64);
    n = read(socket, buffer, 64);

    if (n < 0) {
       perror("ERROR reading from socket");
       exit(1);
    }

    if (buffer[0] == 'i' || buffer[0] == 'e' || buffer[0] == '\0') {
      if (buffer[0] == 'i') {
        if (buffer[2] == 't') {
          printf("\nMake your move: \n");
        }
        if (buffer[2] == 'n') {
          printf("\nWaiting for opponent...\n");
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
        printf("%s: %d", "Here:", *player);
        print_board_buff_inverted(buffer);
      }
    }

    bzero(buffer, 64);
  }
}

// int authenticate(int socket) {
//     char buffer[1024];
//     char command[10], username[50], password[50];
//     int choice;

//     printf("1. Register\n");
//     printf("2. Login\n");
//     printf("Choose an option (1/2): ");
//     scanf("%d", &choice);
//     getchar();

//     if (choice == 1) {
//         strcpy(command, "REGISTER");
//     } else if (choice == 2) {
//         strcpy(command, "LOGIN");
//     } else {
//         printf("Invalid choice. Exiting.\n");
//         return 0;
//     }

//     printf("Enter username: ");
//     scanf("%s", username);
//     printf("Enter password: ");
//     scanf("%s", password);
//     getchar();

//     snprintf(buffer, sizeof(buffer), "%s %s %s", command, username, password);
//     send(socket, buffer, strlen(buffer), 0);

//     int bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0);
//     if (bytes_received <= 0) {
//         printf("Disconnected from server.\n");
//         return 0;
//     }

//     buffer[bytes_received] = '\0';
//     printf("%s", buffer);

//     return strstr(buffer, "successful") != NULL;
// }

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
        while (1) {
          printf("1. Matchmaking\n");
          printf("2. Logout\n");
          printf("Choose an option (1/2): ");
          char input[10];
          int choice;

          fgets(input, sizeof(input), stdin);
          choice = atoi(input);

          if (choice == 1) {
              snprintf(buffer, sizeof(buffer), "1");
              send(sockfd, buffer, strlen(buffer), 0);
              printf("Entering matchmaking...\n");
              pthread_t tid[1];

              pthread_create(&tid[0], NULL, &on_signal, &sockfd);

              while (1) {
                  bzero(buffer, 64);
                  fgets(buffer, 64, stdin);
                  int n = write(sockfd, buffer, strlen(buffer));
                  if (n < 0) {
                      perror("ERROR writing to socket");
                      exit(1);
                  }
              }

              return 0;
          } else if (choice == 2) {
              snprintf(buffer, sizeof(buffer), "2");
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

    close(sockfd);
    return 0;
}
