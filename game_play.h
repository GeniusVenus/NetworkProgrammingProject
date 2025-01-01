#ifndef GAME_PLAY_H
#define GAME_PLAY_H

#include "server.h"
#include "board.h"

bool is_syntax_valid(int player, char *move);
bool is_move_valid(wchar_t **board, int player, int team, int *move);
void move_piece(wchar_t **board, int *move);
void translate_to_move(int *move, char *buffer);
void broadcast(wchar_t **board, char *one_dimension_board, int player_one, int player_two);
bool is_diagonal(int x_moves, int y_moves);
bool is_diagonal_clear(wchar_t **board, int *move);
bool is_rect(int *move);
int is_rect_clear(wchar_t **board, int *move, int x_moves, int y_moves);
int get_piece_team(wchar_t **board, int x, int y);
int get_piece_type(wchar_t piece);
void promote_piece(wchar_t **board, int destX, int destY, int team);
bool check_win_game(wchar_t ** board, int player_one, int player_two, int time);
bool check_draw_game(wchar_t ** board, int player_one, int player_two);
bool is_insufficient_material(wchar_t **board);
bool draw_game(wchar_t **board, int team);
bool has_legal_moves(wchar_t **board, int team);
bool is_piece_on_board(wchar_t **board, wchar_t piece);
bool checkmate(wchar_t **board, wchar_t piece);
bool can_king_escape(wchar_t **board, int king_x, int king_y, int king_team);
bool is_king_in_check(wchar_t **board, int king_x, int king_y, int king_team);
void reset_player_status(int player_one, int player_two);

typedef struct{
    int elo1;
    int elo2;
} elo_diff;
void calculate_elo_change(int elo_a, int elo_b, int time, int *winner_points, int *loser_points);
elo_diff calculate_elo(int player_one_socket, int player_two_socket, int time, bool result);


#endif
