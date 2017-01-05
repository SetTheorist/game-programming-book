/* $Id: board.c,v 1.1 2010/12/21 16:30:32 ahogan Exp ahogan $ */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "search.h"
#include "util.h"
#include "util-bits.h"


/* **************************************** */

static hasht piece_square_hash[8*8][num_color][num_piece];
static hasht jumping_hash[8*8+1];
static hasht btm_hash;

static const char piece_names[num_color][num_piece] = {
  {'-', '=', '+'},
  {'?', 'w', 'W'},
  {'!', 'b', 'B'},
};

static const bitb w_man_moves[32] = {
  0x00000010, 0x00000030, 0x00000060, 0x000000C0,
  0x00000300, 0x00000600, 0x00000C00, 0x00000800,
  0x00001000, 0x00003000, 0x00006000, 0x0000C000,
  0x00030000, 0x00060000, 0x000C0000, 0x00080000,
};
static int all_moves_w(const board* b, movet* ml) {
  int f, fi, t, ti, n=0;
  movet m;
  m.i = 0;
  m.valid = 1;
  m.p = man;
  for (f=b->bb[white][man], fi=bit_scan_forward32(f); f; fi=bit_scan_forward32(f),f&=~(1<<fi)) {
    for (t=w_man_moves[fi], ti=bit_scan_forward32(t); t; ti=bit_scan_forward32(t),t&=~(1<<ti)) {
      m.f = f; m.t = t;
      ml[n++] = m;
    }
  }
  return n;
}
static int all_caps_w(const board* b, movet* ml) {
}

int gen_moves(const board* b, movet* ml) {
  if (b->to_move==white) {
    return all_moves_w(b, ml);
  } else {
  }
}
int gen_moves_cap(const board* b, movet* ml) {
}

movet null_move() {
  movet m;
  m.i = 0;
  m.null = 1;
  m.valid = 1;
  return m;
}

/* **************************************** */

int init_hash() {
  int i, j, k;
  btm_hash = gen_hash();
  for (i=0; i<8*8; ++i)
    for (j=0; j<num_color; ++j)
      for (k=0; k<num_piece; ++k)
        piece_square_hash[i][j][k] = gen_hash();
  for (i=0; i<8*8; ++i)
    jumping_hash[i] = gen_hash();
  return 0;
}

static hasht rehash_board(const board* b) {
  hasht h = b->to_move==white ? 0ULL : btm_hash;
  h ^= jumping_hash[1+b->jumping];
  int i;
  for (i=b->bb[white][man]; i; i^=(i&(i^(i-1))))
    h ^= piece_square_hash[white][man][bit_scan_forward32(i&(i^(i-1)))];
  for (i=b->bb[white][king]; i; i^=(i&(i^(i-1))))
    h ^= piece_square_hash[white][king][bit_scan_forward32(i&(i^(i-1)))];
  for (i=b->bb[black][man]; i; i^=(i&(i^(i-1))))
    h ^= piece_square_hash[black][man][bit_scan_forward32(i&(i^(i-1)))];
  for (i=b->bb[black][king]; i; i^=(i&(i^(i-1))))
    h ^= piece_square_hash[black][king][bit_scan_forward32(i&(i^(i-1)))];
  return h;
}

int init_board(board* b) {
  int i;
  memset(b, '\x00', sizeof(board));
  b->to_move = white;
  b->jumping = -1;
  b->bb[white][man] = 0x00000FFF;
  b->bb[black][man] = 0xFFF00000;
  b->h = rehash_board(b);
  b->hash_stack[0] = b->h;
  return 0;
}

/* **************************************** */

int draw_by_repetition(const board* b) {
  int i;
  for (i=b->ply-1; i>=0; --i)
    if (b->hash_stack[b->ply] == b->hash_stack[i])
      return 1;
  return 0;
}

int terminal(const board* b) {
  return 0;
  return (!b->bb[white][man] && !b->bb[white][king])
      || (!b->bb[black][man] && !b->bb[black][king])
      || draw_by_repetition(b);
}

int make_move(board* b, movet m) {
  int i;
  if (!m.valid) { fprintf(stderr, "!"); return invalid_move; }
  const color tm = b->to_move;
  if (m.null) {
  } else if (m.capture) {
    b->bb[tm][m.p] ^= (1<<m.f) ^ (1<<m.t);
    b->bb[tm^3][m.xp] ^= (1<<m.x);
    b->h ^= piece_square_hash[m.f][tm][m.p] ^ piece_square_hash[m.t][tm][m.p]
        ^ piece_square_hash[m.x][tm^3][m.xp];
  } else {
    b->bb[tm][m.p] ^= (1<<m.f) ^ (1<<m.t);
    b->h ^= piece_square_hash[m.f][tm][m.p] ^ piece_square_hash[m.t][tm][m.p];
  }
  if (m.promote) {
    /* TODO: promotion */
  }
  b->to_move = (b->to_move ^ 3);
  b->h ^= btm_hash;
  b->move_stack[b->ply] = m;
  ++b->ply;
  b->hash_stack[b->ply] = b->h;
  return 0;
}

int unmake_move(board* b) {
  --b->ply;
  movet m = b->move_stack[b->ply];
  color tm = b->to_move = b->to_move ^ 3;
  /* TODO: ... */
  b->h = b->hash_stack[b->ply];
  return 0;
}

/* **************************************** */

evalt evaluate(const evaluator* e, const board* b, int ply) {
  int i, sum=0;
  return sum;
}
evalt evaluate_relative(const evaluator* e, const board* b, int ply) {
  return b->to_move==white ? evaluate(e,b,ply) : -evaluate(e,b,ply);
  return DRAW;
}
int evaluate_moves_for_search(const evaluator* e, const board* b,
                              const movet* ml, evalt* vl, int nm) {
  int i;
  memset(vl, '\x00', sizeof(evalt)*nm);
  for (i=0; i<nm; ++i) vl[i] = -i;
  return DRAW;
}
int evaluate_moves_for_quiescence(const evaluator* e, const board* b,
                                  const movet* ml, evalt* vl, int nm) {
  int i;
  memset(vl, '\x00', sizeof(evalt)*nm);
  for (i=0; i<nm; ++i) vl[i] = -i;
  return DRAW;
}
int simple_evaluator(evaluator* e) {
  int i;
  memset(e, '\x00', sizeof(evaluator));
  return 0;
}
int show_evaluator(FILE* f, const evaluator* e) {
  int i;
  fprintf(f, "{");
  fprintf(f, "}");
  return 0;
}

/* **************************************** */

int board_to_fen(const board* b, char* chp) {
  int i, j, c;
  return 0;
}

int fen_to_board(board* b, const char* chp) {
  return (void*)b==(void*)chp;
}

int board_to_sgf(const board* b, char* chp) {
  return (void*)b==(void*)chp;
}

int sgf_to_board(board* b, const char* chp) {
  return (void*)b==(void*)chp;
}

int show_move(FILE* f, movet m) {
  fprintf(f, "%c%c%c%c%c",
    'a'+(m.f%8), '1'+(m.f/8),
    m.capture ? 'X' : '-',
    'a'+(m.t%8), '1'+(m.t/8)
    );
  if (m.promote) fprintf(f, "++");
  return 0;
}

int show_board(FILE* f, const board* b) {
  int i, j;
  ansi_fg_magenta(f); fprintf(f, " abcdefgh"); ansi_reset(f);
  fprintf(f, "  [%016llX]\n", b->h);
  for (i=7; i>=0; --i) {
    ansi_fg_magenta(f); fprintf(f, "%c", '1'+i); ansi_reset(f);
    for (j=0; j<4; ++j) {
      char c=' ';
      if (b->bb[white][man] & (1<<(j+4*i)))
        c = piece_names[white][man];
      else if (b->bb[white][king] & (1<<(j+4*i)))
        c = piece_names[white][king];
      else if (b->bb[black][man] & (1<<(j+4*i)))
        c = piece_names[black][man];
      else if (b->bb[black][king] & (1<<(j+4*i)))
        c = piece_names[black][king];
      if (i%2) {
        ansi_bg_white(f); fprintf(f, " "); ansi_reset(f);
        fprintf(f, "%c", c);
      } else {
        fprintf(f, "%c", c);
        ansi_bg_white(f); fprintf(f, " "); ansi_reset(f);
      }
    }
    {
      int ply = ((7-i) + b->ply/2-8);
      fprintf(f, "  %2i", 2*ply);
      fprintf(f, "  %2i", 2*ply+1);
      /* ... */
    }
    fprintf(f, "\n");
  }
  ansi_fg_blue(f);
  fprintf(f, "%s to move\n", b->to_move==white ? "White" : "Black");
  ansi_reset(f);
  return 0;
}
