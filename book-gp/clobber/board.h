#ifndef INCLUDED_BOARD_H_
#define INCLUDED_BOARD_H_
/* $Id: board.h,v 1.4 2010/12/23 23:59:43 apollo Exp apollo $ */

#include <stdio.h>
#include "util.h"

typedef enum side {
  none=0, white=1, black=2,
} side;
typedef enum side_ {
  sidemask=white|black,
} side_;
typedef union movet {
  ui32 i;
  struct {
    unsigned int f_x: 3;
    unsigned int f_y: 3;
    unsigned int t_x: 3;
    unsigned int t_y: 3;
    unsigned int null: 1;
  };
} movet;

#define MOVE_IS_NULL(m_) ((m_).null)
#define MOVE_IS_VALID(m_) (1)
#define MAXNMOVES (8*8)
#define MAXNCAPMOVES (8*8)
#define MAXBOARDI (8*8)
#define MOVE_FROM_I(m_) ((m_).f_x + 8*(m_).f_y)
#define MOVE_TO_I(m_) ((m_).t_x + 8*(m_).t_y)

typedef struct board {
  ui8 b[8][8];
  side to_move;
  hasht hash_stack[128];
  movet move_stack[128];
  int ply;
} board;

#define DRAW (0)
#define W_INF (+100000001)
#define W_WIN (+100000000)
#define W_MATE (+100000000-100)
#define B_INF (-100000001)
#define B_WIN (-100000000)
#define B_MATE (-100000000+100)

int show_board(FILE* f, const board* b);
int dump_board(FILE* f, const board* b);
int show_move(FILE* f, movet m);
int show_move_b(FILE* f, const board* b, movet m);

#define last_move(b_) ((b_)->move_stack[(b_)->ply-1])

int validate_board(const board* b);
int setup_board(board* b);

int make_move(board* b, movet m);
int unmake_move(board* b);
int gen_moves(const board* b, movet* ml);
int gen_moves_cap(const board* b, movet* ml);
movet null_move();

void init_hash();
hasht hash_board(const board* b);
#define board_hash(b_) ((b_)->hash_stack[(b_)->ply])

typedef int evalt;
typedef struct evaluator {
} evaluator;

int show_evaluator(FILE* f, const evaluator* e);

int simple_evaluator(evaluator* e);
int monte_carlo_evaluator(evaluator* e);
int monte_carlo_2_evaluator(evaluator* e);

int terminal(const board* b);
evalt evaluate(const evaluator* e, const board* b, int ply);
evalt evaluate_relative(const evaluator* e, const board* b, int ply);
int evaluate_moves_for_search(const evaluator* e, const board* b,
                              const movet* ml, evalt* vl, int nm);
int evaluate_moves_for_quiescence(const evaluator* e, const board* b,
                                  const movet* ml, evalt* vl, int nm);


#endif /*INCLUDED_BOARD_H_*/
