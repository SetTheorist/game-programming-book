#ifndef INCLUDED_BOARD_H_
#define INCLUDED_BOARD_H_
/* $Id: board.h,v 1.1 2010/12/21 16:30:32 ahogan Exp ahogan $ */
#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

#include <stdio.h>
#include "util.h"

/* **************************************** */

#define MAXBOARDI (32)
#define MAXNCAPMOVES (64)
#define MAXNMOVES (64)

typedef union movet {
  ui32 i;
  struct {
    unsigned f:5;
    unsigned t:5;
    unsigned p:2;
    unsigned x:5;
    unsigned xp:5;
    unsigned capture:1;
    unsigned promote:1;
    unsigned null:1;
    unsigned valid:1;
  };
} movet;

#define MOVE_FROM_I(m_) ((m_).f)
#define MOVE_TO_I(m_) ((m_).t)
#define MOVE_IS_VALID(m_) ((m_).valid)
#define MOVE_IS_NULL(m_) ((m_).null)

typedef enum piece {
  empty=0, man, king,
  num_piece
} piece;

typedef enum color {
  none=0, white=1, black=2,
  num_color
} color;

typedef ui32 bitb;
typedef struct board {
  hasht h;
  int ply;
  int jumping;
  color to_move;
  bitb bb[num_color][num_piece];
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

/* **************************************** */

typedef signed int evalt;
typedef struct evaluator {
  int piece_square_vals[num_piece][8*8];
} evaluator;

int simple_evaluator(evaluator* e);

evalt evaluate(const evaluator* e, const board* b, int ply);
evalt evaluate_relative(const evaluator* e, const board* b, int ply);
int evaluate_moves_for_search(const evaluator* e, const board* b,
                              const movet* ml, evalt* vl, int nm);
int evaluate_moves_for_quiescence(const evaluator* e, const board* b,
                                  const movet* ml, evalt* vl, int nm);
#define W_WIN (+1000000000)
#define B_WIN (-1000000000)
#define W_WIN_IN_N(ply_) (+1000000000-(ply_))
#define B_WIN_IN_N(ply_) (-1000000000+(ply_))
#define W_INF (W_WIN+1)
#define B_INF (B_WIN-1)
#define DRAW  (0)

int show_evaluator(FILE* f, const evaluator* e);

/* **************************************** */

#ifdef  __cplusplus
};
#endif/*__cplusplus*/
#endif /* INCLUDED_BOARD_H_ */
