#include "game_play.h"

void print_board(wchar_t **board){
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++)
      printf("%d ", get_piece_team(board, i, j));
    printf("\n");
  }
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

bool is_move_valid(wchar_t ** board, int player, int team, int * move) {

  int * piece_team = (int *)malloc(sizeof(int *));
  int * x_moves = (int *)malloc(sizeof(int *));
  int * y_moves = (int *)malloc(sizeof(int *));

 *piece_team = get_piece_team(board, *(move), move[1]);
  *x_moves = getManitud(*move, move[2]);
  *y_moves = getManitud(move[1], move[3]);

  printf("BEGIN_MOVEMENT:  \n");
  printf("%d %d %d %d\n", *piece_team , team , *x_moves, *y_moves);
  printf("%d %d %d %d\n", move[0], move[1], move[2], move[3]);
  printf("END_MOVEMENT  \n");

  // General errors

  // If selected piece == 0 there's nothing selected
  if (board[*(move)][move[1]] == 0) {
    send(player, "e-08", 4, 0);
    return false;
  }

  // If the origin piece's team == dest piece's team is an invalid move
  if (*piece_team == get_piece_team(board, move[2], move[3])) {
    send(player, "e-09", 4, 0);
    return false;
  }

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
        if (move[0] == 1 && *piece_team == 1 && *x_moves == 2 ) {
          printf("First move\n");
          return true;
        }
        if (move[0] == 6 && *piece_team == -1 && *x_moves == 2 ) {
          printf("First move\n");
          return true;
        }

        if (move[0] - move[2] < 0 && *piece_team == -1){
          send(player, "e-62", 5, 0);
          freeAll(piece_team, x_moves, y_moves);
          return false;
        }

        if(move[0] - move[2] > 0 && *piece_team == 1){
          send(player, "e-62", 5, 0);
          freeAll(piece_team, x_moves, y_moves);
          return false;
        }

        if (*x_moves > 1) {
          send(player, "e-62", 5, 0);
          freeAll(piece_team, x_moves, y_moves);
          return false;
        }

        printf("Eat ? %d \n", eat_piece(board, move[2], move[3]));
        if(eat_piece(board, move[2], move[3])){
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

void move_piece(wchar_t ** board, int * move) {
  // Move piece in board from origin to dest
  board[move[2]][move[3]] = board[*move][move[1]];
  board[*move][move[1]] = 0;
}


void translate_to_move(int * move, char * buffer) {

  printf("buffer: %s\n", buffer);

  *(move) = (*(buffer+1)-'0')-1;
  move[1] = (*(buffer)-'0')-49;
  move[2] = (*(buffer+4)-'0')-1;
  move[3] = (*(buffer+3)-'0')-49;

  // printf("[%d, %d] to [%d, %d]\n", *(move), move[1], move[2], move[3]);
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

bool is_diagonal(int x_moves, int y_moves) {

  if ((abs(x_moves)-abs(y_moves)) != 0) {
    return false;
  }

  return true;
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

int get_piece_team(wchar_t ** board, int x, int y) {

  switch (board[x][y]) {
    case white_king: return 1;
    case white_queen: return 1;
    case white_rook: return 1;
    case white_bishop: return 1;
    case white_knight: return 1;
    case white_pawn: return 1;
    case black_king: return -1;
    case black_queen: return -1;
    case black_rook: return -1;
    case black_bishop: return -1;
    case black_knight: return -1;
    case black_pawn: return -1;
  }

  return 0;

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

void promote_piece(wchar_t ** board, int destX, int destY, int team) {
  if (team == 1) {
    board[destX][destY] = black_queen;
  } else if (team == -1) {
    board[destX][destY] = white_queen;
  }
}

bool is_piece_on_board(wchar_t **board, wchar_t piece) {
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (board[i][j] == piece)
                return true;
        }
    }
    return false;
}

bool is_king_in_check(wchar_t **board, int king_x, int king_y, int king_team) {
    // Check for attacks from all possible directions

    // Check horizontal and vertical (rook/queen)
    int directions[4][2] = {{0,1}, {0,-1}, {1,0}, {-1,0}};
    for (int d = 0; d < 4; d++) {
        int x = king_x + directions[d][0];
        int y = king_y + directions[d][1];
        while (x >= 0 && x < 8 && y >= 0 && y < 8) {
            if (board[x][y] != 0) {
                if (get_piece_team(board, x, y) == -king_team) {
                    int piece_type = get_piece_type(board[x][y]);
                    if (piece_type == 1 || piece_type == 2) { // Queen or Rook
                        return true;
                    }
                }
                break;
            }
            x += directions[d][0];
            y += directions[d][1];
        }
    }

    // Check diagonals (bishop/queen)
    int diagonals[4][2] = {{1,1}, {1,-1}, {-1,1}, {-1,-1}};
    for (int d = 0; d < 4; d++) {
        int x = king_x + diagonals[d][0];
        int y = king_y + diagonals[d][1];
        while (x >= 0 && x < 8 && y >= 0 && y < 8) {
            if (board[x][y] != 0) {
                if (get_piece_team(board, x, y) == -king_team) {
                    int piece_type = get_piece_type(board[x][y]);
                    if (piece_type == 1 || piece_type == 3) { // Queen or Bishop
                        return true;
                    }
                }
                break;
            }
            x += diagonals[d][0];
            y += diagonals[d][1];
        }
    }

    // Check knight moves
    int knight_moves[8][2] = {
        {-2,-1}, {-2,1}, {-1,-2}, {-1,2},
        {1,-2}, {1,2}, {2,-1}, {2,1}
    };
    for (int i = 0; i < 8; i++) {
        int x = king_x + knight_moves[i][0];
        int y = king_y + knight_moves[i][1];
        if (x >= 0 && x < 8 && y >= 0 && y < 8) {
            if (board[x][y] != 0) {
                if (get_piece_team(board, x, y) == -king_team &&
                    get_piece_type(board[x][y]) == 4) { // Knight
                    return true;
                }
            }
        }
    }

    // Check pawn attacks
    int pawn_direction = (king_team == 1) ? 1 : -1;
    if (king_x + pawn_direction >= 0 && king_x + pawn_direction < 8) {
        if (king_y - 1 >= 0) {
            if (board[king_x + pawn_direction][king_y - 1] != 0 &&
                get_piece_team(board, king_x + pawn_direction, king_y - 1) == -king_team &&
                get_piece_type(board[king_x + pawn_direction][king_y - 1]) == 5) {
                return true;
            }
        }
        if (king_y + 1 < 8) {
            if (board[king_x + pawn_direction][king_y + 1] != 0 &&
                get_piece_team(board, king_x + pawn_direction, king_y + 1) == -king_team &&
                get_piece_type(board[king_x + pawn_direction][king_y + 1]) == 5) {
                return true;
            }
        }
    }

    return false;
}

bool can_king_escape(wchar_t **board, int king_x, int king_y, int king_team) {
    int moves[8][2] = {
        {-1,-1}, {-1,0}, {-1,1},
        {0,-1},          {0,1},
        {1,-1},  {1,0},  {1,1}
    };

    // Try all possible king moves
    for (int i = 0; i < 8; i++) {
        int new_x = king_x + moves[i][0];
        int new_y = king_y + moves[i][1];

        // Check if move is within board bounds
        if (new_x >= 0 && new_x < 8 && new_y >= 0 && new_y < 8) {
            // Check if square is empty or contains enemy piece
            if (board[new_x][new_y] == 0 ||
                get_piece_team(board, new_x, new_y) == -king_team) {

                // Make temporary move
                wchar_t temp = board[new_x][new_y];
                board[new_x][new_y] = board[king_x][king_y];
                board[king_x][king_y] = 0;

                // Check if new position is safe
                bool in_check = is_king_in_check(board, new_x, new_y, king_team);

                // Undo temporary move
                board[king_x][king_y] = board[new_x][new_y];
                board[new_x][new_y] = temp;

                if (!in_check) return true;
            }
        }
    }
    printf("KING GOT CHECKMATED \n");
    printf("King position: %d %d && King_team: %d \n", king_x, king_y, king_team);
    return false;
}

bool checkmate(wchar_t **board, wchar_t piece) {
    // Find the king's position
    int king_x = -1, king_y = -1;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (board[i][j] == piece) {
                king_x = i;
                king_y = j;
                break;
            }
        }
        if (king_x != -1) break;
    }

    // If king not found, return false
    if (king_x == -1) return false;

    int king_team = get_piece_team(board, king_x, king_y);

    printf("Is_in_check: %d && King position: %d %d && King_team: %d \n",is_king_in_check(board, king_x, king_y, king_team), king_x, king_y, king_team);

    // Check if king is in check
    if (!is_king_in_check(board, king_x, king_y, king_team)) {
        return false;
    }

    // If king is in check, see if it can escape
    return !can_king_escape(board, king_x, king_y, king_team);
}

bool check_win_game(wchar_t **board, int player_one, int player_two, int time) {
    bool black_king_alive = is_piece_on_board(board, black_king);
    bool white_king_alive = is_piece_on_board(board, white_king);

    // If the black king is not alive or is checkmated, white wins
    if (!black_king_alive || (black_king_alive && checkmate(board, black_king))) {
        printf("We have white as the winner\n");
        send(player_one, "i-w", 4, 0);
        send(player_two, "i-l", 4, 0);
        // * calculate_elo(1, 2, time, 0)
        printf("TIME IS: %d\n", time);
        elo_diff result = calculate_elo(player_one, player_two, time, 0);
        printf("HERE is result of 1: %d and 2 when user 1 win: %d\n", result.elo1, result.elo2);
        for (int i = 0; i < MAX_CLIENTS; ++ i)
        {
          if (clients[i].socket == player_one || clients[i].socket == player_two) {
            clients[i].ready = 0;
          }
        }
        return true;
    }

    // If the white king is not alive or is checkmated, black wins
    if (!white_king_alive || (white_king_alive && checkmate(board, white_king))) {
        printf("We have black as the winner\n");
        send(player_one, "i-l", 4, 0);
        send(player_two, "i-w", 4, 0);
        // * calculate_elo(1, 2, time, 1)
        printf("TIME IS: %d\n", time);
        elo_diff result = calculate_elo(player_one, player_two, time, 1);
        printf("HERE is result of 1: %d and 2 when user 2 win: %d\n", result.elo1, result.elo2);
        for (int i = 0; i < MAX_CLIENTS; ++ i)
        {
          if (clients[i].socket == player_one || clients[i].socket == player_two) {
            clients[i].ready = 0;
          }
        }
        return true;
    }

    return false; // No winner yet
}


elo_diff calculate_elo(int player_one_socket, int player_two_socket, int time, bool rs) {
  elo_diff result = {0, 0};
  result.elo1 = 0;
  result.elo2 = 0;
  int elo_1, elo_2;

  printf("Chuan Bi day elo ne!: %d && %d\n", player_one_socket, player_two_socket);

  for (int i = 0; i < MAX_CLIENTS; ++ i)
  {
    if (clients[i].socket == player_one_socket) {
      elo_1 = clients[i].elo;
      continue;
    }
    if (clients[i].socket == player_two_socket) {
      elo_2 = clients[i].elo;
      continue;
    }
    
  }
  printf("Player One Socket: %d, ELO: %d\n", player_one_socket, elo_1);
  printf("Player Two Socket: %d, ELO: %d\n", player_two_socket, elo_2);

  if (player_one_socket < 0 || player_two_socket < 0) {
    fprintf(stderr, "Invalid player socket(s): %d, %d\n", player_one_socket, player_two_socket);
    result.elo1 = 0;
    result.elo2 = 0;
    return result;
}

  if (rs == 0) {
    if (elo_1 < elo_2) {
      if (time <= 120) {
        result.elo2 = -50;
        result.elo1 = 40;
      }
      else {
        result.elo2 = -40;
        result.elo1 = 35;
      }
    }
    else if (elo_1 > elo_2) {
      if (time <= 120) {
        result.elo2 = -30;
        result.elo1 = 35;
      }
      else {
        result.elo2 = -25;
        result.elo1 = 30;
      }
    }
    else {
      if (time <= 120) {
        result.elo2 = -30;
        result.elo1 = 30;
      }
      else {
        result.elo2 = -25;
        result.elo1 = 25;
      }
    }
  }
  else {
    if (elo_1 > elo_2) {
      if (time <= 120) {
        result.elo1 = -50;
        result.elo2 = 40;
      }
      else {
        result.elo1 = -40;
        result.elo2 = 35;
      }
    }
    else if (elo_1 < elo_2) {
      if (time <= 120) {
        result.elo1 = -30;
        result.elo2 = 35;
      }
      else {
        result.elo1 = -25;
        result.elo2 = 30;
      }
    }
    else {
      if (time <= 120) {
        result.elo1 = -30;
        result.elo2 = 30;
      }
      else {
        result.elo1 = -25;
        result.elo2 = 25;
      }
    }
  }
  return result;
}

// bool has_legal_moves(wchar_t **board, int team) {
//     // Check all pieces of the given team for any legal moves
//     for (int i = 0; i < 8; i++) {
//         for (int j = 0; j < 8; j++) {
//             if (board[i][j] != 0 && get_piece_team(board, i, j) == team) {
//                 // Try all possible destination squares
//                 for (int x = 0; x < 8; x++) {
//                     for (int y = 0; y < 8; y++) {
//                         // Create a move array in the format expected by is_move_valid
//                         int move[4] = {i, j, x, y};

//                         // Check if this move would be legal
//                         if (is_move_valid(board, -1, team, move)) {
//                             // Make temporary move
//                             wchar_t temp = board[x][y];
//                             board[x][y] = board[i][j];
//                             board[i][j] = 0;

//                             // Find king position
//                             int king_x = -1, king_y = -1;
//                             wchar_t king_piece = (team == 1) ? black_king : white_king;
//                             for (int ki = 0; ki < 8; ki++) {
//                                 for (int kj = 0; kj < 8; kj++) {
//                                     if (board[ki][kj] == king_piece) {
//                                         king_x = ki;
//                                         king_y = kj;
//                                         break;
//                                     }
//                                 }
//                                 if (king_x != -1) break;
//                             }

//                             // Check if king would be safe after move
//                             bool king_safe = !is_king_in_check(board, king_x, king_y, team);

//                             // Undo temporary move
//                             board[i][j] = board[x][y];
//                             board[x][y] = temp;

//                             if (king_safe) {
//                                 return true;
//                             }
//                         }
//                     }
//                 }
//             }
//         }
//     }
//     return false;
// }

// bool is_insufficient_material(wchar_t **board) {
//     int white_pieces = 0, black_pieces = 0;
//     int white_bishops = 0, black_bishops = 0;
//     int white_knights = 0, black_knights = 0;
//     bool white_bishop_color = false; // false for light, true for dark
//     bool black_bishop_color = false;

//     // Count pieces and track their types
//     for (int i = 0; i < 8; i++) {
//         for (int j = 0; j < 8; j++) {
//             if (board[i][j] != 0) {
//                 int team = get_piece_team(board, i, j);
//                 int type = get_piece_type(board[i][j]);

//                 if (team == -1) { // White pieces
//                     white_pieces++;
//                     if (type == 3) { // Bishop
//                         white_bishops++;
//                         white_bishop_color = ((i + j) % 2 == 0);
//                     } else if (type == 4) { // Knight
//                         white_knights++;
//                     }
//                 } else { // Black pieces
//                     black_pieces++;
//                     if (type == 3) { // Bishop
//                         black_bishops++;
//                         black_bishop_color = ((i + j) % 2 == 0);
//                     } else if (type == 4) { // Knight
//                         black_knights++;
//                     }
//                 }

//                 // Early exit if we find pieces that can checkmate
//                 if (type == 1 || type == 2 || type == 5) { // Queen, Rook, or Pawn
//                     return false;
//                 }
//             }
//         }
//     }

//     // Check insufficient material conditions
//     if (white_pieces == 1 && black_pieces == 1) { // King vs King
//         return true;
//     }
//     if (white_pieces == 2 && black_pieces == 1) { // King + Bishop/Knight vs King
//         if (white_bishops == 1 || white_knights == 1) return true;
//     }
//     if (white_pieces == 1 && black_pieces == 2) { // King vs King + Bishop/Knight
//         if (black_bishops == 1 || black_knights == 1) return true;
//     }
//     if (white_pieces == 2 && black_pieces == 2) { // King + Bishop vs King + Bishop (same color)
//         if (white_bishops == 1 && black_bishops == 1 &&
//             white_bishop_color == black_bishop_color) {
//             return true;
//         }
//     }

//     return false;
// }

// bool draw_game(wchar_t **board, int team) {
//     // Check for stalemate - no legal moves but king is not in check
//     if (!has_legal_moves(board, team)) {
//         wchar_t king = (team == 1) ? black_king : white_king;
//         int king_x = -1, king_y = -1;

//         // Find king position
//         for (int i = 0; i < 8; i++) {
//             for (int j = 0; j < 8; j++) {
//                 if (board[i][j] == king) {
//                     king_x = i;
//                     king_y = j;
//                     break;
//                 }
//             }
//             if (king_x != -1) break;
//         }

//         if (!is_king_in_check(board, king_x, king_y, team)) {
//             return true;  // Stalemate
//         }
//     }

//     // Check for insufficient material
//     if (is_insufficient_material(board)) {
//         return true;
//     }

//     return false;
// }

bool check_draw_game(wchar_t ** board, int player_one, int player_two){
  // if(draw_game(board, 1) || draw_game(board, -1)){
  //   send(player_one, "i-d", 4, 0);
  //   send(player_two, "i-d", 4, 0);
  //   return true;
  // }
  return false;
}
