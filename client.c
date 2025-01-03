#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <locale.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>
#include <sys/select.h>
#include "board.h"

#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define RESET "\x1B[0m"

#define BUFFER_SIZE 256
#define USERNAME_SIZE 50
#define PASSWORD_SIZE 50

// Screen states
typedef enum
{
  EXIT,
  AUTH_SCREEN,
  OPTION_SCREEN,
  GAMEPLAY_SCREEN,
  LIST_PLAYER_SCREEN,
  CHALLENGE_SCREEN,
  CHALLENGE_REQUEST_SCREEN,
  CHALLENGE_WAITING_SCREEN,
  BLANK_SCREEN
} Screen;

// Global variables
Screen current_screen = AUTH_SCREEN;
int player = 0;
int is_ingame = 0;
char challenger[USERNAME_SIZE];
int navigate_to_challenge_screen = 0;
pthread_mutex_t screen_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function declarations
void navigate(Screen screen);
void *on_signal(void *sockfd);
void clear_stdin(void);
void set_nonblock_input(void);
void set_block_input(void);
int get_line(char *buffer, int size);
int get_integer(void);
void authenticate_screen(int sockfd);
void option_screen(int sockfd);
void gameplay_screen(void *sockfd);
void challenge_screen(void *sockfd);
void challenge_request_screen(void *sockfd);
void challenge_waiting_screen(void *sockfd);
void blank_screen(void *sockfd);

// Input handling utilities
void clear_stdin(void)
{
  int c;
  while ((c = getchar()) != '\n' && c != EOF)
    ;
}

void set_nonblock_input(void)
{
  int flags = fcntl(STDIN_FILENO, F_GETFL);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void set_block_input(void)
{
  int flags = fcntl(STDIN_FILENO, F_GETFL);
  fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}

int get_line(char *buffer, int size)
{
  if (fgets(buffer, size, stdin) != NULL)
  {
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
    {
      buffer[len - 1] = '\0';
      return 1;
    }
    clear_stdin();
    return 1;
  }
  return 0;
}

int get_integer(void)
{
  char buffer[32];
  if (get_line(buffer, sizeof(buffer)))
  {
    return atoi(buffer);
  }
  return -1;
}

// Navigation
void navigate(Screen screen)
{
  current_screen = screen;
}

// Signal handler
void *on_signal(void *sockfd)
{
  char buffer[BUFFER_SIZE];
  int n;
  int socket = *(int *)sockfd;
  while (1)
  {
    bzero(buffer, BUFFER_SIZE);
    n = read(socket, buffer, BUFFER_SIZE);
    // printf("Socket descriptor: %d\n", socket);
    if (n < 0)
    {
      perror("ERROR reading from socket");
      exit(1);
    }

    // printf("\nRECEIVE BUFFER: %s\n", buffer);

    // Authentication messages
    if (buffer[0] == 'a')
    {
      if (strstr(buffer, "login") != NULL)
      {
        if (buffer[strlen(buffer) - 1] == '1')
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
        if (buffer[strlen(buffer) - 1] == '1')
        {
          printf(GREEN "Register successful\n" RESET);
          navigate(AUTH_SCREEN);
        }
        else
        {
          printf(RED "Register failed\n" RESET);
          navigate(AUTH_SCREEN);
        }
      }
      if (strstr(buffer, "logout") != NULL)
      {
        if (buffer[strlen(buffer) - 1] == '1')
        {
          printf(GREEN "Logout successfully\n" RESET);
          navigate(AUTH_SCREEN);
        }
      }
    }

    // Game state messages
    if (buffer[0] == 'i' || buffer[0] == 'e' || buffer[0] == '\0')
    {
      if (buffer[0] == 'i')
      {
        if (buffer[2] == 't')
        {
          printf("\nMake your move: \n");
          fflush(stdout);
        }
        if (buffer[2] == 'n')
        {
          printf("\nWaiting for opponent...\n");
          fflush(stdout);
        }
        if (buffer[2] == 'l')
        {
          is_ingame = 0;
          printf(RED "\nYou lose...\n" RESET);
          navigate(OPTION_SCREEN);
          fflush(stdout);
        }
        if (buffer[2] == 'w')
        {
          is_ingame = 0;
          printf(GREEN "\nYou win...\n" RESET);
          navigate(OPTION_SCREEN);
          fflush(stdout);
        }
        if (buffer[2] == 'd')
        {
          printf(YELLOW "\nDraw...\n" RESET);
        }
        if (buffer[2] == 'p')
        {
          memset(challenger, '\0', sizeof(challenger));
          player = atoi(&buffer[3]);
          is_ingame = 1;
          printf("You're %s (%c)\n",
                 player == 2 ? "blacks" : "whites",
                 buffer[3]);
          navigate(GAMEPLAY_SCREEN);
        }
      }
      // Error messages
      else if (buffer[0] == 'e')
      {
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
    }
    // Online players list
    else if (strstr(buffer, "Online") != NULL)
    {
      printf("%s\n", buffer);
      navigate(OPTION_SCREEN);
    }

    // Challenge related messages
    else if (strstr(buffer, "challenge") != NULL)
    {
      if (strstr(buffer, "request") != NULL)
      {
        if (strstr(buffer, "0") != NULL)
        {
          printf(YELLOW "PLAYER NOT FOUND\n" RESET);
          navigate(OPTION_SCREEN);
        }
        else
        {
          pthread_mutex_lock(&screen_mutex);
          char *challenger_start = strrchr(buffer, '-');
          strcpy(challenger, challenger_start + 1);
          navigate_to_challenge_screen = 1;
          pthread_mutex_unlock(&screen_mutex);
          printf("Do you accept the challenge from %s? (y/Y for Yes, any other key for No):\n", challenger);
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
    // Game board update
    else if (is_ingame)
    {
      system("clear");
      if (player == 1)
        print_board_buff(buffer);
      else
        print_board_buff_inverted(buffer);
    }

    bzero(buffer, BUFFER_SIZE);
  }

  return NULL;
}

// Screen handlers
void authenticate_screen(int sockfd)
{
  while (current_screen == AUTH_SCREEN)
  {
    char buffer[BUFFER_SIZE];
    char username[USERNAME_SIZE], password[PASSWORD_SIZE];

    printf("\n=== Authentication ===\n");
    printf("1. Register\n");
    printf("2. Login\n");
    printf("3. Exit\n");
    printf("Choose an option (1/2/3): ");

    set_block_input();
    int choice = get_integer();

    if (choice < 1 || choice > 3)
    {
      printf("Invalid choice. Try again.\n");
      continue;
    }

    if (choice == 3)
    {
      printf("Exiting client.\n");
      exit(0);
    }

    printf("Enter username: ");
    if (!get_line(username, sizeof(username)))
      continue;

    printf("Enter password: ");
    if (!get_line(password, sizeof(password)))
      continue;

    const char *command = (choice == 1) ? "REGISTER" : "LOGIN";
    snprintf(buffer, sizeof(buffer), "%s %s %s", command, username, password);
    send(sockfd, buffer, strlen(buffer), 0);

    sleep(1);
  }
}

void option_screen(int sockfd)
{
  char buffer[BUFFER_SIZE];
  set_nonblock_input();

  // Print the menu once before entering the loop
  printf("\n=== Main Menu ===\n");
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

    char input[10];
    if (get_line(input, sizeof(input)))
    {
      int choice = atoi(input);

      switch (choice)
      {
      case 1:
        snprintf(buffer, sizeof(buffer), "MATCH_MAKING");
        send(sockfd, buffer, strlen(buffer), 0);
        printf("Enter matchmaking...\n");
        navigate(GAMEPLAY_SCREEN);
        return;

      case 2:
        snprintf(buffer, sizeof(buffer), "LIST_PLAYER_ONLINE");
        send(sockfd, buffer, strlen(buffer), 0);
        // printf("HEY I TRY TO SEND BUFFER HERE: %s\n", buffer);
        sleep(1);
        return;

      case 3:
        navigate(CHALLENGE_SCREEN);
        return;

      case 4:
        snprintf(buffer, sizeof(buffer), "LOG_OUT");
        send(sockfd, buffer, strlen(buffer), 0);
        sleep(1);
        return;

      default:
        printf("Invalid choice. Try again.\n");
        printf("Choose an option (1/2/3/4): ");
        fflush(stdout);
      }
    }

    usleep(100000);
  }
}

void gameplay_screen(void *sockfd)
{
  char buffer[64];
  set_block_input();

  while (is_ingame)
  {
    if (get_line(buffer, sizeof(buffer)))
    {
      int n = write(sockfd, buffer, strlen(buffer));
      if (n < 0)
      {
        perror("ERROR writing to socket");
        exit(1);
      }
    }
  }

  set_nonblock_input();
}

void challenge_screen(void *sockfd)
{
  char buffer[BUFFER_SIZE];
  char target_username[USERNAME_SIZE];

  set_block_input();
  printf("\n=== Send Challenge ===\n");
  printf("Enter the username of the player to challenge: ");

  if (get_line(target_username, sizeof(target_username)))
  {
    snprintf(buffer, sizeof(buffer), "CHALLENGE_PLAYER");
    send(sockfd, buffer, strlen(buffer), 0);
    send(sockfd, target_username, strlen(target_username), 0);
    navigate(CHALLENGE_WAITING_SCREEN);
  }

  // printf("Waiting for opponent to accept challenge...\n");
  sleep(1);
}

void challenge_request_screen(void *sockfd)
{
  char answer[10];
  set_block_input();

  printf("\n=== Challenge Request ===\n");
  printf("Do you accept the challenge from %s? (y/Y for Yes, any other key for No): ", challenger);

  if (get_line(answer, sizeof(answer)))
  {
    if (strcmp(answer, "y") == 0 || strcmp(answer, "Y") == 0)
    {
      char command[BUFFER_SIZE];
      snprintf(command, sizeof(command), "ACCEPT_CHALLENGE %s", challenger);
      send(sockfd, command, strlen(command), 0);
      printf(GREEN "Challenge accepted.\n" RESET);
      navigate(GAMEPLAY_SCREEN);
    }
    else
    {
      char decline_command[BUFFER_SIZE];
      snprintf(decline_command, sizeof(decline_command), "DECLINE_CHALLENGE %s", challenger);
      send(sockfd, decline_command, strlen(decline_command), 0);
      printf(RED "Challenge declined.\n" RESET);
      navigate(OPTION_SCREEN);
    }
  }
}

void challenge_waiting_screen(void *sockfd)
{
  printf("\n=== Waiting for Response ===\n");
  printf("Waiting for opponent to accept challenge...\n");

  int timeout_counter = 0;
  const int timeout_limit = 30; // Timeout limit in seconds

  while (current_screen == CHALLENGE_WAITING_SCREEN)
  {
    sleep(1); // Wait for 1 second
    timeout_counter++;

    // Exit waiting screen after timeout limit
    if (timeout_counter >= timeout_limit)
    {
      printf(RED "Challenge request timed out.\n" RESET);
      navigate(OPTION_SCREEN);
      break;
    }

    // Check if the screen has transitioned elsewhere
    pthread_mutex_lock(&screen_mutex);
    if (current_screen != CHALLENGE_WAITING_SCREEN)
    {
      pthread_mutex_unlock(&screen_mutex);
      break;
    }
    pthread_mutex_unlock(&screen_mutex);
  }
}

void blank_screen(void *sockfd)
{
  // Empty implementation - placeholder for transitional state
  sleep(1);
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
