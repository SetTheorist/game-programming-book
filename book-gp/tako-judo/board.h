#ifndef INCLUDED_BOARD_H_
#define INCLUDED_BOARD_H_
/* $Id: board.h,v 1.1 2010/12/17 19:42:02 ahogan Exp $ */
#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

#include <stdio.h>
#include "util.h"

/* **************************************** */

#define MAXBOARDI (64)
#define MAXNCAPMOVES 0
#define MAXNMOVES 256

typedef union movet {
  ui32 i;
  struct {
    unsigned f:6;
    unsigned t:6;
    unsigned head: 1;
    unsigned null: 1;
    unsigned valid: 1;
  };
} movet;

#define MOVE_FROM_I(m_) ((m_).f)
#define MOVE_TO_I(m_) ((m_).t)
#define MOVE_IS_VALID(m_) ((m_).valid)
#define MOVE_IS_NULL(m_) ((m_).null)

typedef enum piece {
  empty=0, head, tentacle,
  num_piece
} piece;
typedef enum color {
  white, black,
  num_color
} color;
typedef struct board {
  hasht h;
  int ply;
  piece p[64];
  color c[64];
  int head[2];
  color to_move;
  hasht hash_stack[256];
  movet move_stack[256];
} board;

/* **************************************** */

int gen_moves(const board* b, movet* ml);
int gen_moves_cap(const board* b, movet* ml);
movet null_move();

int init_hash();
//hasht board_hash(const board* b);
#define board_hash(b_) ((b_)->h)

int init_board(board* b);
int terminal(const board* b);

int make_move(board* b, movet m);
int unmake_move(board* b);
//movet last_move(const board* b);
#define last_move(b_) ((b_)->move_stack[(b_)->ply-1])

int board_to_fen(const board* b, char* chp);
int fen_to_board(board* b, const char* chp);
int board_to_sgf(const board* b, char* chp);
int sgf_to_board(board* b, const char* chp);

int show_move(FILE* f, movet m);
int show_board(FILE* f, const board* b);

#ifdef  __cplusplus
};
#endif/*__cplusplus*/
#endif /* INCLUDED_BOARD_H_ */
