#ifndef INCLUDED_BOARD_H_
#define INCLUDED_BOARD_H_
/* $Id: $ */
#include <stdio.h>
#include "util.h"

#define  VALIDATE_MOVE  0
#define  VALIDATE_BOARD  0
#define  SHOW_MOVES    0

typedef enum colort {
  white, black, none
} colort;
typedef enum piecet {
  wman, bman, bking, empty
} piecet;
typedef enum squaret {
  cnt=0, frt=1, kng=2, crn=3
} squaret;
typedef enum flagst {
  w_win=1, b_win=2, w_draw=4, b_draw=8,
  nullmove=128
} flagst;
#define  draw  (w_draw|b_draw)
typedef struct movet {
  ui8  from;
  ui8  to;
  ui8  caps;
  ui8  flags;
} movet;

typedef struct board {
  piecet  pieces[81];
  colort  side;
  colort  xside;
  int    bking_loc;
  int    n_wman;
  int    n_bman;
  int    terminal;
  colort result;
  hasht  hash;
  int    grid[81];
  int    ply;
  movet  hist[1024];
  hasht  hash_hist[1024];
} board;

#define MOVE_IS_NULL(m_) (((m_).flags&nullmove)==nullmove)
#define MOVE_IS_VALID(m_) (1)
#define MAXNMOVES (256)
#define MAXNCAPMOVES (32)
#define MAXBOARDI (81)
#define MOVE_FROM_I(m_) ((m_).from)
#define MOVE_TO_I(m_) ((m_).to)

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

#define last_move(b_) ((b_)->hist[(b_)->ply-1])

int validate_board_chp(const board* b, const char* chp);
int validate_board(const board* b);
int setup_board(board* b);

int make_move(board* b, movet m);
int unmake_move(board* b);
int gen_moves(const board* b, movet* ml);
int gen_moves_cap(const board* b, movet* ml);
movet null_move();

void init_hash();
hasht hash_board(const board* b);
#define board_hash(b_) ((b_)->hash_hist[(b_)->ply])

typedef int evalt;
typedef struct evaluator {
  int use_mobility;
} evaluator;
int show_evaluator(FILE* f, const evaluator* e);
int simple_evaluator(evaluator* e);

int terminal(const board* b);
evalt evaluate(const evaluator* e, const board* b, int ply);
evalt evaluate_relative(const evaluator* e, const board* b, int ply);
int evaluate_moves_for_search(const evaluator* e, const board* b,
                              const movet* ml, evalt* vl, int nm);
int evaluate_moves_for_quiescence(const evaluator* e, const board* b,
                                  const movet* ml, evalt* vl, int nm);

#endif /* INCLUDED_BOARD_H_ */
