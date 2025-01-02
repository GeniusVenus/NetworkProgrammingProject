#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <locale.h>

#include "board.h"
#include <sys/select.h>

#define RED "\x1B[31m"
#define RESET "\x1B[0m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"

int navigate_to_challenge_screen = 0;
pthread_mutex_t screen_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef enum
{
  EXIT,
  AUTH_SCREEN,
  OPTION_SCREEN,
  GAMEPLAY_SCREEN,
  CHALLENGE_SCREEN,
  CHALLENGE_REQUEST_SCREEN,
  CHALLENGE_WAITING_SCREEN,
  BLANK_SCREEN
} Screen;

Screen current_screen = AUTH_SCREEN;
int player;
int is_ingame = 0;
char challenger[256];

void navigate(Screen screen)
{
  current_screen = screen;
}

void *on_signal(void *sockfd)
{
  char buffer[256];
  int n;
  int socket = *(int *)sockfd;

  while (1)
  {
    bzero(buffer, 256);
    n = read(socket, buffer, 256);

    if (n < 0)
    {
      perror("ERROR reading from socket");
      exit(1);
    }
    printf("\n RECEIVE BUFFER: %s \n", buffer);
    if (buffer[0] == 'a') // Authentication messages
    {
      if (strstr(buffer, "login") != NULL)
      {
        if (buffer[strlen(buffer) - 1] == '1') // Successful login/register
        {
          printf(GREEN "Authentication successful\n" RESET);
          navigate(OPTION_SCREEN);
        }
        else
        {
          printf(RED "Authentication failed\n" RESET);
          navigate(AUTH_SCREEN);
        }
      }
      if (strstr(buffer, "register") != NULL)
      {
        if (buffer[strlen(buffer) - 1] == '1') // Successful login/register
        {
          printf(GREEN "Register successful\n" RESET);
          navigate(OPTION_SCREEN);
        }
        else
        {
          printf(RED "Register failed\n" RESET);
          navigate(AUTH_SCREEN);
        }
      }
      if (strstr(buffer, "logout") != NULL)
      {
        if (buffer[strlen(buffer) - 1] == '1') // Successful logout
        {
          printf(GREEN "Logout successfully\n" RESET);
          navigate(AUTH_SCREEN);
        }
      }
    }
    if (buffer[0] == 'i' || buffer[0] == 'e' || buffer[0] == '\0')
    {
      if (buffer[0] == 'i')
      {
        if (buffer[2] == 't')
        {
          printf("\nMake your move: \n");
        }
        if (buffer[2] == 'n')
        {
          printf("\nWaiting for opponent...\n");
        }
        if (buffer[2] == 'l')
        {
          is_ingame = 0;
          printf(RED "\nYou lose...\n" RESET);
          navigate(OPTION_SCREEN);
        }
        if (buffer[2] == 'w')
        {
          is_ingame = 0;
          printf(GREEN "\nYou win...\n" RESET);
          navigate(OPTION_SCREEN);
        }
        if (buffer[2] == 'd')
        {
          printf(YELLOW "\nDraw...\n" RESET);
          // if (!rematch(sockfd)) {
          //   printf("Rematch not accepted. Exiting.\n");
          //   close(sockfd);
          // }
        }
        if (buffer[2] == 'p')
        {
          memset(challenger, '\0', sizeof(challenger));
          player = atoi(&buffer[3]);
          is_ingame = 1;
          if (player == 2)
          {
            printf("You're blacks (%c)\n", buffer[3]);
          }
          else
          {
            printf("You're whites (%c)\n", buffer[3]);
          }
        }
      }
      else if (buffer[0] == 'e')
      {
        // Syntax errors
        if (buffer[2] == '0')
        {
          switch (buffer[3])
          {
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
    }
    else if (strstr(buffer, "Online") != NULL)
    {
      printf("%s \n", buffer);
      navigate(OPTION_SCREEN);
    }
    else if (strstr(buffer, "challenge") != NULL)
    {
      if (strstr(buffer, "request") != NULL)
      {
        if (strstr(buffer, "0") != NULL)
        {
          printf(YELLOW "NOT FOUND PLAYER\n" RESET);
          navigate(OPTION_SCREEN);
        }
        else
        {
          pthread_mutex_lock(&screen_mutex);
          char *challenger_start = strrchr(buffer, '-');
          strcpy(challenger, challenger_start + 1);
          navigate_to_challenge_screen = 1; // Set flag
          pthread_mutex_unlock(&screen_mutex);

          printf("Do you accept the challenge from %s? (y or Y for Yes, any other key for No):\n", challenger);

          navigate(CHALLENGE_REQUEST_SCREEN);
        }
      }
      else if (strstr(buffer, "response") != NULL)
      {
        if (buffer[strlen(buffer) - 1] == '1')
        {
          printf(YELLOW "CHALLENGE ACCEPTED\n" RESET);
          navigate(BLANK_SCREEN);
        }
        else
        {
          printf(RED "CHALLENGE DECLINED\n" RESET);
          navigate(OPTION_SCREEN);
        }
      }
    }
    else if (is_ingame)
    {
      // Print the board
      system("clear");
      if (player == 1)
        print_board_buff(buffer);
      else
        print_board_buff_inverted(buffer);
    }

    bzero(buffer, 256);
  }

  free(buffer);
  return NULL;
}

void authenticate_screen(int sockfd)
{
  while (current_screen == AUTH_SCREEN)
  {
    char buffer[1024];
    char command[10], username[50], password[50];
    int choice;
    printf("1. Register\n");
    printf("2. Login\n");
    printf("3. Exit\n");
    printf("Choose an option (1/2/3): ");
    scanf("%d", &choice);
    getchar();

    if (choice == 1)
      strcpy(command, "REGISTER");
    else if (choice == 2)
      strcpy(command, "LOGIN");
    else if (choice == 3)
    {
      printf("Exiting client.\n");
      exit(0);
    }
    else
    {
      printf("Invalid choice. Try again.\n");
      continue;
    }

    printf("Enter username: ");
    scanf("%s", username);
    printf("Enter password: ");
    scanf("%s", password);
    getchar();

    snprintf(buffer, sizeof(buffer), "%s %s %s", command, username, password);
    printf("\nSEND BUFFER: %s\n", buffer);
    send(sockfd, buffer, strlen(buffer), 0);

    sleep(1); // Wait for server response
  }
}

void option_screen(int sockfd)
{
  char buffer[1024];
  int choice = 0;
  struct timeval tv;
  fd_set fds;

  // Print menu once at the start
  printf("1. Matchmaking\n");
  printf("2. Online Players\n");
  printf("3. Send Challenge Request\n");
  printf("4. Logout\n");
  printf("Choose an option (1/2/3/4): ");
  fflush(stdout);

  while (current_screen == OPTION_SCREEN)
  {
    pthread_mutex_lock(&screen_mutex);
    if (navigate_to_challenge_screen)
    {
      navigate_to_challenge_screen = 0;
      pthread_mutex_unlock(&screen_mutex);
      navigate(CHALLENGE_REQUEST_SCREEN);
      return;
    }
    pthread_mutex_unlock(&screen_mutex);

    // Set up for select()
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    // Wait for input with timeout
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0)
    {
      scanf("%d", &choice);
      getchar(); // Consume newline

      if (choice == 1)
      {
        snprintf(buffer, sizeof(buffer), "MATCH_MAKING");
        printf("Enter matchmaking...\n");
        send(sockfd, buffer, strlen(buffer), 0);
        sleep(1);
        navigate(GAMEPLAY_SCREEN);
        break;
      }
      else if (choice == 2)
      {
        snprintf(buffer, sizeof(buffer), "LIST_PLAYER_ONLINE");
        printf("Online Players:\n%s\n", buffer);
        send(sockfd, buffer, strlen(buffer), 0);
        sleep(1);

        // Reprint menu after showing online players
        printf("\n1. Matchmaking\n");
        printf("2. Online Players\n");
        printf("3. Send Challenge Request\n");
        printf("4. Logout\n");
        printf("Choose an option (1/2/3/4): ");
        fflush(stdout);
      }
      else if (choice == 3)
      {
        navigate(CHALLENGE_SCREEN);
        break;
      }
      else if (choice == 4)
      {
        snprintf(buffer, sizeof(buffer), "LOG_OUT");
        printf("\nSEND BUFFER: %s\n", buffer);
        send(sockfd, buffer, strlen(buffer), 0);
        sleep(1);
      }
      else
      {
        printf("Invalid choice. Try again.\n");
        printf("Choose an option (1/2/3/4): ");
        fflush(stdout);
      }
    }
  }
}

void gameplay_screen(void *sockfd)
{
  char buffer[64];

  while (is_ingame)
  {
    bzero(buffer, 64);
    fgets(buffer, 64, stdin);
    int n = write(sockfd, buffer, strlen(buffer));
    if (n < 0)
    {
      perror("ERROR writing to socket");
      exit(1);
    }
  }
}

void challenge_screen(void *sockfd)
{
  char buffer[256];
  char target_username[50];
  printf("Enter the username of the player to challenge: ");
  fgets(target_username, sizeof(target_username), stdin);
  target_username[strcspn(target_username, "\n")] = '\0'; // Remove newline

  // Send choice and username to the server
  snprintf(buffer, sizeof(buffer), "CHALLENGE_PLAYER");
  send(sockfd, buffer, strlen(buffer), 0);
  send(sockfd, target_username, strlen(target_username) + 1, 0);
  navigate(CHALLENGE_WAITING_SCREEN);
  sleep(1);
}

void challenge_waiting_screen(void *sockfd)
{
}

void challenge_request_screen(void *sockfd)
{
  char answer[10];
  if (fgets(answer, sizeof(answer), stdin) != NULL)
  {
    // Remove trailing newline
    answer[strcspn(answer, "\n")] = '\0';

    if (strcmp(answer, "y") == 0 || strcmp(answer, "Y") == 0)
    {
      char command[256];
      snprintf(command, sizeof(command), "ACCEPT_CHALLENGE %s", challenger);

      if (send(sockfd, command, strlen(command), 0) < 0)
      {
        perror("ERROR sending ACCEPT_CHALLENGE command");
      }
      else
      {
        printf(GREEN "Challenge accepted.\n" RESET);
        navigate(GAMEPLAY_SCREEN);
      }
    }
    else
    {
      char decline_command[256];
      snprintf(decline_command, sizeof(decline_command), "DECLINE_CHALLENGE %s", challenger);

      if (send(sockfd, decline_command, strlen(decline_command), 0) < 0)
      {
        perror("ERROR sending DECLINE_CHALLENGE command");
      }
      else
      {
        printf(RED "Challenge declined.\n" RESET);
      }
    }
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
      ;
  }
  else
  {
    printf(RED "Invalid input.\n" RESET);
  }
  sleep(1);
}

void blank_screen(void *sockfd)
{
}

int main(int argc, char *argv[])
{
  int sockfd, portno;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  setlocale(LC_ALL, "en_US.UTF-8");
  if (argc < 2)
  {
    fprintf(stderr, "Usage: %s hostname [port]\n", argv[0]);
    exit(1);
  }

  portno = (argc >= 3) ? atoi(argv[2]) : 80;
  printf("Connecting to %s:%d\n", argv[1], portno);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    perror("ERROR opening socket");
    exit(1);
  }

  server = gethostbyname(argv[1]);
  if (server == NULL)
  {
    fprintf(stderr, "ERROR, no such host\n");
    exit(0);
  }

  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(portno);

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("ERROR connecting");
    exit(1);
  }

  printf("Connected to server.\n");

  pthread_t tid;
  pthread_create(&tid, NULL, &on_signal, &sockfd);

  while (current_screen != EXIT)
  {
    switch (current_screen)
    {
    case AUTH_SCREEN:
      authenticate_screen(sockfd);
      break;
    case OPTION_SCREEN:
      option_screen(sockfd);
      break;
    case GAMEPLAY_SCREEN:
      gameplay_screen(sockfd);
      break;
    case CHALLENGE_SCREEN:
      challenge_screen(sockfd);
      break;
    case CHALLENGE_REQUEST_SCREEN:
      challenge_request_screen(sockfd);
      break;
    case CHALLENGE_WAITING_SCREEN:
      challenge_waiting_screen(sockfd);
      break;
    case BLANK_SCREEN:
      blank_screen(sockfd);
      break;
    default:
      printf("Unknown state. Exiting.\n");
      current_screen = EXIT;
      break;
    }
  }

  close(sockfd);
  return 0;
}
