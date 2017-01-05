#ifndef INCLUDED_BOARD_H_
#define INCLUDED_BOARD_H_
/* $Id: board.h,v 1.4 2010/12/23 23:59:43 apollo Exp apollo $ */

#include <stdio.h>
#include "util.h"

typedef enum piece {
  empty=0,
  sylph, griffin, dragon,
  oliphant, unicorn, hero, thief, cleric, mage, king, paladin, warrior,
  basilisk, elemental, dwarf,
} piece;

typedef enum piece_ {
  max_piece=dwarf+1, piecemask=0x0F
} piece_;

typedef enum side {
  none=0, white=1, black=2,
} side;

typedef enum side_ {
  sidemask=white|black,
} side_;

enum flags {
  f_cap=1, f_farcap=2, f_pro=4, f_chk=8, f_null=16
};

typedef union movet {
  ui32 i;
  struct {
    unsigned int f_x: 4;
    unsigned int f_y: 3;
    unsigned int f_z: 2;
    unsigned int t_x: 4;
    unsigned int t_y: 3;
    unsigned int t_z: 2;
    unsigned int cap_piece: 4;
    unsigned int cap_idx: 4;
    unsigned int flags: 6;
  };
} movet;

#define MOVE_IS_NULL(m_) (((m_).flags&f_null)==f_null)
#define MOVE_IS_VALID(m_) (1)
#define MAXNMOVES (1024)
#define MAXNCAPMOVES (512)
#define MAXBOARDI (8*12*3)
#define MOVE_FROM_I(m_) ((m_).f_x + 12*(m_).f_y + 3*12*(m_).f_z)
#define MOVE_TO_I(m_) ((m_).t_x + 12*(m_).t_y + 3*12*(m_).t_z)

typedef union pos {
  ui32 i;
  struct {
    int x: 6;
    int y: 5;
    int z: 4;
    unsigned int valid: 1;
    unsigned int frozen: 1;
    unsigned int captured: 1;
  };
} pos;

typedef union square {
  ui32 i;
  struct {
    side s: 2;
    piece p: 4;
    unsigned int idx: 4;
  };
} square;

typedef struct board {
  pos pieces[2][max_piece][16];
  square b[3][8][12];
  side to_move;
  hasht hash_stack[1024];
  movet move_stack[1024];
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
  int f_monte_carlo;
  int f_material;
  int f_mobility;
  int f_control;
  int f_king_tropism;
  // from chessvariants.org
  evalt piece_values[max_piece];
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
