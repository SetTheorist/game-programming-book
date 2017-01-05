/* $Id: board.c,v 1.4 2010/12/23 23:59:43 apollo Exp apollo $ */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "board.h"
#include "search.h"
#include "util.h"

//#define VALIDATE_MOVE

static hasht hash_piece[8][8][3];
static hasht hash_to_move[2];

/* ************************************************************ */

int show_board(FILE* f, const board* b) {
  return 0;
}

int show_move(FILE* f, movet m) {
  fprintf(f, "%c%c%c%c%c%c%c",
    (m.f_z==0 ? 'B' : m.f_z==1 ? 'M' : m.f_z==2 ? 'T' : '?'),
    'a'+m.f_x, '1'+m.f_y,
    m.flags&f_cap ? (m.flags&f_farcap ? '*' : 'x') : '-',
    (m.t_z==0 ? 'B' : m.t_z==1 ? 'M' : m.t_z==2 ? 'T' : '?'),
    'a'+m.t_x, '1'+m.t_y
  );
  return 0;
}

int show_move_b(FILE* f, const board* b, movet m) {
  return 0;
}

int show_evaluator(FILE* f, const evaluator* e) {
  fprintf(f, "[[");
  fprintf(f, "]]");
  return 0;
}

/* ************************************************************ */

int setup_board(board* b) {
  int i;
  memset(b, '\x00', sizeof(board));
  b->to_move = white;
  b->hash_stack[0] = hash_board(b);
  return 0;
}

movet null_move() {
  movet m;
  m.i = 0;
  m.null = 1;
  return m;
}

int make_move(board* b, movet m) {
  if (m.flags & f_null) {
    b->to_move ^= sidemask;
    b->hash_stack[b->ply+1] = b->hash_stack[b->ply] ^ hash_to_move[0] ^ hash_to_move[1];
    b->move_stack[b->ply] = m;
    ++(b->ply);
    return 0;
  }
  side s = b->to_move, o = (b->to_move ^ sidemask);

  ...

  b->to_move ^= sidemask;

  b->move_stack[b->ply] = m;
  b->hash_stack[++(b->ply)] = hash_board(b);
  return 0;
}

int unmake_move(board* b) {
  movet m = b->move_stack[b->ply-1];
  if (m.flags&f_null) {
    b->to_move ^= sidemask;
    --(b->ply);
    return 0;
  }
  side s = (b->to_move ^ sidemask), o = b->to_move;

  ...

  b->to_move ^= sidemask;
  --(b->ply);
  return 0;
}

int gen_moves(const board* b, movet* ms) {
  only_captures = 0;
  return all_moves(b, ms);
}
int gen_moves_cap(const board* b, movet* ms) {
  only_captures = 1;
  return all_moves(b, ms);
}

/* ************************************************************ */
void init_hash() {
  int s, p, x, y, z;
  for (s=0; s<2; ++s)
    for (p=0; p<max_piece-1; ++p)
      for (z=0; z<3; ++z)
        for (y=0; y<8; ++y)
          for (x=0; x<12; ++x)
            hash_piece[s][p][z][y][x] = gen_hash();
  hash_to_move[0] = gen_hash();
  hash_to_move[1] = gen_hash();
}
hasht hash_board(const board* b) {
  int z, y, x;
  hasht h = hash_to_move[b->to_move - 1];
  for (z=0; z<3; ++z)
    for (y=0; y<8; ++y)
      for (x=0; x<12; ++x)
        if (b->b[z][y][x].s)
          h ^= hash_piece[b->b[z][y][x].s-1][b->b[z][y][x].p-1][z][y][x];
  return h;
}

/* ************************************************************ */

int terminal(const board* b) {
  int i;
  if (b->pieces[white-1][king][0].captured
      || b->pieces[black-1][king][0].captured)
    return 1;

  const hasht h = b->hash_stack[b->ply];
  for (i=b->ply-1; i>=0; --i)
    if (b->hash_stack[i] == h)
      return 1;
  return 0;
}

// from chessvariants.org
static evalt piece_values[max_piece] = {
    0,
// sylph, griffin, dragon,
  100,  500,  800,
// oliphant, unicorn, hero, thief, cleric, mage, king, paladin, warrior,
  500,  250,  450,  400,  900, 1100, 99999, 1000,  100,
// basilisk, elemental, dwarf,
  300,  400,  200
};

int simple_evaluator(evaluator* e) {
  memset(e, '\x00', sizeof(evaluator));
  memcpy(e->piece_values, piece_values, sizeof(piece_values));
  e->f_mobility = 1;
  e->f_material = 1;
  e->f_control = 0;
  e->f_king_tropism = 0;
  return 0;
}

static evalt evaluate_material(const evaluator* e, const board* b) {
  evalt eval = 0;
  int i, j;
  for (i=1; i<max_piece; ++i)
    for (j=0; b->pieces[white-1][i][j].valid; ++j)
      if (!b->pieces[white-1][i][j].captured)
        eval += e->piece_values[i];
  for (i=1; i<max_piece; ++i)
    for (j=0; b->pieces[black-1][i][j].valid; ++j)
      if (!b->pieces[black-1][i][j].captured)
        eval -= e->piece_values[i];
  return eval;
}

static evalt evaluate_mobility(const evaluator* e, const board* b) {
  evalt eval = 0;
  char brd[3][8][12];
  movet  ms[256];
  int n, i;
  int mob_w=0, mob_b=0;

  n = all_moves(b, ms);
  for (i=0; i<n; ++i) {
    mob_w += (ms[i].t_z==1) ? 2 : 1;
    mob_w += (ms[i].flags&f_cap) ? 2 : 0;
  }

  if (e->f_control) {
    memset(brd, '\x00', sizeof(brd));
    for (i=0; i<n; ++i)
      brd[ms[i].t_z][ms[i].t_y][ms[i].t_x] |= white;
  }

  movet nm = null_move();
  make_move((board*)b, nm);
  n = all_moves(b, ms);
  for (i=0; i<n; ++i) {
    mob_b -= (ms[i].t_z==1) ? 2 : 1;
    mob_b -= (ms[i].flags&f_cap) ? 2 : 0;
  }
  unmake_move((board*)b);

  eval += (mob_w/4) - (mob_b/4);

  if (e->f_control) {
    for (i=0; i<n; ++i)
      brd[ms[i].t_z][ms[i].t_y][ms[i].t_x] |= black;

    int x, y, z;
    for (z=0; z<3; ++z) {
      for (y=0; y<8; ++y) {
        for (x=0; x<12; ++x) {
          if (brd[z][y][x]==white)
            eval += (z==1) ? 3 : 1;
          else if (brd[z][y][x]==black)
            eval += (z==1) ? 3 : 1;
        }
      }
    }
  }
  return eval;
}

static evalt evaluate_king_tropism(const evaluator* e, const board* b) {
  evalt eval = 0;
  int i, j;
  if (b->pieces[black-1][king][0].valid) {
    int kx = b->pieces[black-1][king][0].x;
    int ky = b->pieces[black-1][king][0].y;
    int kz = b->pieces[black-1][king][0].z;
    for (i=1; i<max_piece; ++i) {
      for (j=0; b->pieces[white-1][i][j].valid; ++j) {
        if (!b->pieces[white-1][i][j].captured) {
          int px = b->pieces[white-1][i][0].x;
          int py = b->pieces[white-1][i][0].y;
          int pz = b->pieces[white-1][i][0].z;
          eval -= (int)floor(sqrt(abs(kx-px) + abs(ky-py) + abs(kz-pz))*10.0);
        }
      }
    }
  }
  if (b->pieces[white-1][king][0].valid) {
    int kx = b->pieces[white-1][king][0].x;
    int ky = b->pieces[white-1][king][0].y;
    int kz = b->pieces[white-1][king][0].z;
    for (i=1; i<max_piece; ++i) {
      for (j=0; b->pieces[black-1][i][j].valid; ++j) {
        if (!b->pieces[black-1][i][j].captured) {
          int px = b->pieces[black-1][i][0].x;
          int py = b->pieces[black-1][i][0].y;
          int pz = b->pieces[black-1][i][0].z;
          eval += (int)floor(sqrt(abs(kx-px) + abs(ky-py) + abs(kz-pz))*10.0);
        }
      }
    }
  }
  return eval;
}

static evalt standard_evaluate(const evaluator* e, const board* b, int ply) {
  evalt eval = 0;
  if (terminal(b)) {
    if (b->pieces[white-1][king][0].captured
        && b->pieces[black-1][king][0].captured) return DRAW;
    if (b->pieces[black-1][king][0].captured) return W_WIN-ply;
    if (b->pieces[white-1][king][0].captured) return B_WIN+ply;
    const hasht h = b->hash_stack[b->ply];
    int i;
    for (i=b->ply-1; i>=0; --i)
      if (b->hash_stack[i] == h)
        return DRAW;
  }
  if (e->f_material)
    eval += evaluate_material(e, b);
  if (e->f_mobility)
    eval += evaluate_mobility(e, b);
  if (e->f_king_tropism)
    eval += evaluate_king_tropism(e, b);

  eval += (board_hash(b) % 21) - 10;
  return eval;
}

static evalt monte_carlo_evaluate(const evaluator* e, const board* b, int n, int depth, int ply) {
  evaluator e2;
  simple_evaluator(&e2);
  double sum = 0.0;
  int i, j;
  for (i=0; i<n; ++i) {
    board bt;
    movet  ms[256];
    memcpy(&bt, b, sizeof(board));
    for (j=0; j<depth && !terminal(&bt); ++j) {
      int num_m = all_moves(&bt, ms);
      make_move(&bt, ms[lrand48() % num_m]);
    }
    sum += standard_evaluate(&e2, &bt, 0);
  }
  return (int)floor(sum/n);
}

static evalt monte_carlo_evaluate_2(const evaluator* e, const board* b, int n, int depth, int ply) {
  evaluator e2;
  simple_evaluator(&e2);
  double sum = 0.0;
  int i, j;
  for (i=0; i<n; ++i) {
    movet ms[256];
    for (j=0; j<depth && !terminal(b); ++j) {
      int num_m = all_moves(b, ms);
      int which = lrand48() % num_m;
      make_move((board*)b, ms[which]);
    }
    sum += standard_evaluate(&e2, b, 0);
    for (j=j-1; j>=0; --j) unmake_move((board*)b);
  }
  return (int)floor(sum/n);
}

evalt evaluate(const evaluator* e, const board* b, int ply) {
  /* TODO: monte-carlo params in e */
  switch (e->f_monte_carlo) {
    case 1: return monte_carlo_evaluate(e, b, 100, 10, ply);
    case 2: return monte_carlo_evaluate_2(e, b, 100, 10, ply);
    case 0:
    default: return standard_evaluate(e, b, ply);
  }
}
evalt evaluate_relative(const evaluator* e, const board* b, int ply) {
  return (b->to_move==white) ? evaluate(e, b, ply) : -evaluate(e, b, ply);
}
int evaluate_moves_for_search(const evaluator* e, const board* b,
                              const movet* ml, evalt* vl, int nm) {
  int i;
  /* TODO: MVV/LVA */
  for (i=0; i<nm; ++i)
    vl[i] = (nm-i) + ((ml[i].flags&f_cap) ? 1000 : 0);
  return 0;
}
int evaluate_moves_for_quiescence(const evaluator* e, const board* b,
                              const movet* ml, evalt* vl, int nm) {
  int i;
  /* TODO: MVV/LVA */
  for (i=0; i<nm; ++i)
    vl[i] = (nm-i);
  return 0;
}

/* ************************************************************ */
