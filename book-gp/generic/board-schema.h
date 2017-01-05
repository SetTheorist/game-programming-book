#ifndef INCLUDED_BOARD_H_
#define INCLUDED_BOARD_H_
/* $Id: board-schema.h,v 1.2 2010/12/21 16:47:44 apollo Exp $ */
#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

/* **************************************** */

#include <stdio.h>

/* **************************************** */

/* history-heuristic assumes any move has a "from" and a "to"
 * square that is in the range 0..MAXBOARDI and can be retrieved
 * via the MOVE_FROM_I() and MOVE_TO_I() macros */
#define MAXBOARDI ...
#define MAXNCAPMOVES ...
#define MAXNMOVES ...

typedef struct movet {
  ...
} movet;
#define MOVE_FROM_I(m_) ...
#define MOVE_TO_I(m_) ...
#define MOVE_IS_VALID(m_) ...
#define MOVE_IS_NULL(m_) ...
typedef struct board {
  ...
} board;

int gen_moves(const board* b, movet* ml);
int gen_moves_cap(const board* b, movet* ml);
movet null_move();

int init_hash();
hasht hash_board(const board* b);

int init_board(board* b);
int terminal(const board* b);

movet make_move(board* b, movet m);
int unmake_move(board* b, movet m);
int unmake(board* b);

int board_to_fen(const board* b, char* chp);
int fen_to_board(board* b, const char* chp);
int board_to_sgf(const board* b, char* chp);
int sgf_to_board(board* b, const char* chp);

int show_move(FILE* f, movet m);
int show_board(FILE* f, const board* b);

/* **************************************** */

typedef int evalt;
typedef struct evaluator {
  ...
} evaluator;

evalt evaluate(const evaluator* e, const board* b, int ply);
evalt evaluate_relative(const evaluator* e, const board* b, int ply);
evalt evaluate_moves_for_search(const evaluator* e, const board* b,
                                const movet* ml, evalt* vl, int nm);
evalt evaluate_moves_for_quiesce(const evaluator* e, const board* b,
                                 const movet* ml, evalt* vl, int nm);
#define W_WIN ...
#define B_WIN ...
#define W_WIN_IN_N(ply_) ...
#define B_WIN_IN_N(ply_) ...
#define W_INF (W_WIN+1)
#define B_INF (B_WIN-1)
#define DRAW  ...

int show_evaluator(FILE* f, const evaluator* e);

/* **************************************** */

#ifdef  __cplusplus
};
#endif/*__cplusplus*/
#endif /* INCLUDED_BOARD_H_ */
