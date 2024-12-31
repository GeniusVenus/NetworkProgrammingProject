#ifndef BOARD_H
#define BOARD_H

#include <wchar.h>
#include <locale.h>
#include <stdlib.h>

#define white_king   0x2654 // ♔
#define white_queen  0x2655 // ♕
#define white_rook   0x2656 // ♖
#define white_bishop 0x2657 // ♗
#define white_knight 0x2658 // ♘
#define white_pawn   0x2659 // ♙
#define black_king   0x265A // ♚
#define black_queen  0x265B // ♛
#define black_rook   0x265C // ♜
#define black_bishop 0x265D // ♝
#define black_knight 0x265E // ♞
#define black_pawn   0x265F // ♟

// Board functions
wchar_t **create_board(void);
char *create_od_board(void);
void initialize_board(wchar_t **board);
void free_board(wchar_t **board);
void to_one_dimension_char(wchar_t **board, char *one_dimension);

#endif
