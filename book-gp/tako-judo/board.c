/* $Id: board.c,v 1.9 2010/12/17 23:23:27 ahogan Exp ahogan $ */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "evaluator.h"
#include "search.h"
#include "util.h"

/* **************************************** */

static hasht piece_square_hash[64][num_color][num_piece];
static hasht btm_hash;

static const char piece_names[] = "?HT";

/* **************************************** */

static int add_move(const board* b, piece p, int f, int t, movet* ml, int* n) {
  if (b->c[t]!=none) return 0;
  ml[*n].i = 0;
  ml[*n].f = f;
  ml[*n].t = t;
  ml[*n].head = p==head;
  ml[*n].null = 0;
  ml[*n].valid = 1;
  ++*n;
  return 1;
}
static const  int delt[8] = { +8, +9, +1, -7, -8, -9, -1, +7 };
int gen_moves(const board* b, movet* ml) {
  int f, t, d, n = 0;
  for (f=0; f<64; ++f) {
    for (d=0; d<8; ++d) {
      for () {
      }
    }
  }
  return n;
}

int gen_moves_cap(const board* b, movet* ml) {
  return 0;
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
  for (i=0; i<30; ++i)
    for (j=0; j<num_color; ++j)
      for (k=1; k<num_piece; ++k)
        piece_square_hash[i][j][k] = gen_hash();
  for (i=0; i<num_color; ++i)
    for (j=1; j<num_piece_base; ++j)
      for (k=0; k<3; ++k)
        piece_hand_hash[i][j][k] = gen_hash();
  return 0;
}
static hasht rehash_board(const board* b) {
  hasht h = b->to_move==white ? 0ULL : btm_hash;
  int i, j;
  for (i=1; i<num_piece; ++i) {
    for (j=0; j<30; ++j) {
      if (b->bb[white][i] & (1<<j))
        h ^= piece_square_hash[j][white][i];
      else if (b->bb[black][i] & (1<<j))
        h ^= piece_square_hash[j][black][i];
    }
  }
  for (i=1; i<num_piece_base; ++i)
    h ^= piece_hand_hash[white][i][b->hand[white][i]]
        ^ piece_hand_hash[black][i][b->hand[black][i]];
  return h;
}
int init_board(board* b) {
  int i;
  memset(b, '\x00', sizeof(board));
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
  return (b->bb[white][king]&WGOALBB)
      || (b->bb[black][king]&BGOALBB)
      || in_check(b,!b->to_move)
      || draw_by_repetition(b);
  return 0;
}

int make_move(board* b, movet m) {
  int i;
  int inch = in_check(b,b->to_move);
  /*printf("<%i,%i,%i=%c,%i|%c%c%c%c%c%c%c>\n",
    m.f, m.t, m.p, piece_names[m.p], m.tm,
    m.drop?'D':'-', m.capture?'C':'-', m.promote?'P':'-', m.win?'W':'-',
    m.check?'K':'-', m.null?'N':'-', m.valid?'V':'-'
  );*/
  if (!m.null) {
    if (m.capture) {
      for (i=1; i<num_piece; ++i) {
        if (b->bb[!b->to_move][i]&(1<<m.t)) {
          int ki = i==gold ? pawn : i;
          if (i==king) { printf("!(King captured)!"); break; }
          b->cap_stack[b->ply] = i;
          b->bb[!b->to_move][i] ^= (1<<m.t);
          ++b->hand[b->to_move][ki];
          b->h ^= piece_square_hash[!b->to_move][i][m.t]
              ^ piece_hand_hash[b->to_move][ki][b->hand[b->to_move][ki]];
          break;
        }
      }
    }
    if (m.drop) {
      b->bb[b->to_move][m.p] ^= (1<<m.t);
      b->h ^= piece_square_hash[b->to_move][m.p][m.t]
          ^ piece_hand_hash[b->to_move][m.p][b->hand[b->to_move][m.p]];
      --b->hand[b->to_move][m.p];
    } else {
      b->bb[b->to_move][m.p] ^= (1<<m.f)|(1<<m.t);
      b->h ^= piece_square_hash[b->to_move][m.p][m.f]
          ^ piece_square_hash[b->to_move][m.p][m.t];
      if (m.promote) {
        /* fixup */
        b->bb[b->to_move][m.p] ^= (1<<m.t);
        b->bb[b->to_move][gold] ^= (1<<m.t);
        b->h ^= piece_square_hash[b->to_move][m.p][m.t]
            ^ piece_square_hash[b->to_move][gold][m.t];
      }
    }
  }
  b->to_move = !(b->to_move);
  b->h ^= btm_hash;
  b->move_stack[b->ply] = m;
  ++b->ply;
  b->hash_stack[b->ply] = b->h;
  if (in_check(b,!b->to_move)) { unmake_move(b); return invalid_move; }
  return 0;
}

int unmake_move(board* b) {
  --b->ply;
  movet m = b->move_stack[b->ply];
  /*printf("{%i,%i,%i=%c,%i|%c%c%c%c%c%c%c}\n",
    m.f, m.t, m.p, piece_names[m.p], m.tm,
    m.drop?'D':'-', m.capture?'C':'-', m.promote?'P':'-', m.win?'W':'-',
    m.check?'K':'-', m.null?'N':'-', m.valid?'V':'-'
  );*/
  b->h = b->hash_stack[b->ply];
  b->to_move = !b->to_move;
  if (m.drop) {
    b->bb[b->to_move][m.p] ^= (1<<m.t);
    ++b->hand[b->to_move][m.p];
  } else {
    b->bb[b->to_move][m.p] ^= (1<<m.f)|(1<<m.t);
    if (m.capture) {
      const int k = b->cap_stack[b->ply];
      const int kk = k==gold ? pawn : k;
      b->bb[!b->to_move][k] ^= (1<<m.t);
      --b->hand[b->to_move][kk];
    }
    if (m.promote) {
      b->bb[b->to_move][m.p] ^= (1<<m.t);
      b->bb[b->to_move][gold] ^= (1<<m.t);
    }
  }
  return 0;
}

/* **************************************** */

evalt evaluate(const evaluator* e, const board* b, int ply) {
  if (terminal(b)) {
    if (b->bb[white][king]&WGOALBB)
      return W_WIN-ply;
    else if (b->bb[black][king]&BGOALBB)
      return B_WIN+ply;
    else if (in_check(b,!b->to_move))
      return b->to_move==white ? W_WIN-ply : B_WIN+ply;
    else return DRAW;
  }
  int i, sum = 0;
  for (i=1; i<num_piece; ++i) {
    sum += count_bits32(b->bb[white][i]) * e->piece_vals[i];
    sum -= count_bits32(b->bb[black][i]) * e->piece_vals[i];
  }
  return DRAW;
}
evalt evaluate_relative(const evaluator* e, const board* b, int ply) {
  return b->to_move==white ? evaluate(e,b,ply) : -evaluate(e,b,ply);
  return DRAW;
}
int evaluate_moves_for_search(const evaluator* e, const board* b,
                              const movet* ml, evalt* vl, int nm) {
  memset(vl, '\x00', sizeof(evalt)*nm);
  return DRAW;
}
int evaluate_moves_for_quiescence(const evaluator* e, const board* b,
                                  const movet* ml, evalt* vl, int nm) {
  memset(vl, '\x00', sizeof(evalt)*nm);
  return DRAW;
}
int simple_evaluator(evaluator* e) {
  e->piece_vals[none] = 0;
  e->piece_vals[pawn] = 100;
  e->piece_vals[ferz] = 200;
  e->piece_vals[wazir] = 300;
  e->piece_vals[gold] = 400;
  e->piece_vals[king] = 1000;
  return 0;
}
int show_evaluator(FILE* f, const evaluator* e) {
  int i;
  fprintf(f, "{");
  for (i=1; i<num_piece; ++i) {
    fprintf(f, "%c(%i)", piece_names[i], e->piece_vals[i]);
  }
  fprintf(f, "}");
  return 0;
}

/* **************************************** */

int board_to_fen(const board* b, char* chp) {
  int i, j;
  for (i=3; i>=0; --i) {
    for (j=0; j<3; ++j) {
      *chp++ = 'x';
    }
    *chp++ = '/';
  }
  *chp++ = '\x00';
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
  if (!m.valid) {
    fprintf(f, "<invalid>");
  } else if (m.null) {
    fprintf(f, "<pass>");
  } else if (m.drop) {
    fprintf(f, "%c*%c%c", piece_names[m.p], 'a'+(m.t%3), '1'+(m.t/3));
  } else {
    fprintf(f, "%c%c%c%c%c%c",
            piece_names[m.p],
            'a'+(m.f%5)-1, '0'+(m.f/5),
            m.capture ? 'x' : '-',
            'a'+(m.t%5)-1, '0'+(m.t/5));
  }
  if (m.promote) fprintf(f, "+");
  //if (m.check) fprintf(f, ":");
  return 0;
}

int show_bitb(FILE* f, bitb b) {
  int i, j;
  for (i=5; i>=0; --i) {
    for (j=0; j<5; ++j) {
      if (i==0 || i==5 || j==0 || j==4)
        fprintf(f, "%c", (b & (1<<(i*5 + j))) ? '#' : '*');
      else
        fprintf(f, "%c", (b & (1<<(i*5 + j))) ? 'x' : '.');
    }
    printf("\n");
  }
  return 0;
}
int show_board(FILE* f, const board* b) {
  int i, j, k, hi;
  fprintf(f, "+---+\n");
  for (j=3; j>=0; --j) {
    fprintf(f, "|");
    for (i=0; i<3; ++i) {
      for (k=1; k<num_piece; ++k) {
        if (b->bb[white][k] & (1<<(1+i+(j+1)*5))) {
          fprintf(f, "%c", piece_names[k]);
          break;
        } else if (b->bb[black][k] & (1<<(1+i+(j+1)*5))) {
          fprintf(f, "%c", tolower(piece_names[k]));
          break;
        }
      }
      if (k==6) fprintf(f, ".");
    }
    fprintf(f, "|");
    if (j==3) fprintf(f, "  [%016llX]", b->h);
    else if (j==2) {
      fprintf(f, "  B Hand:");
      for (hi=1; hi<num_piece_base; ++hi)
        fprintf(f, "%c%i ", piece_names[hi], b->hand[black][hi]);
    } else if (j==1) {
      fprintf(f, "  W Hand:");
      for (hi=1; hi<num_piece_base; ++hi)
        fprintf(f, "%c%i ", piece_names[hi], b->hand[white][hi]);
    } else if (j==0) {
      fprintf(f, "(%i) %s", b->ply,
              b->to_move==white ? "White to move" : "Black to move");
    }
    fprintf(f, "\n");
  }
  fprintf(f, "+---+\n");
  return 0;
}
