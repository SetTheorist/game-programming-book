/* $Id: board.c,v 1.10 2010/12/21 16:48:47 ahogan Exp $ */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "search.h"
#include "util.h"

/* **************************************** */

static hasht piece_square_hash[30][num_color][num_piece];
static hasht piece_hand_hash[num_color][num_piece_base][3];
static hasht btm_hash;

//#define VALIDBB (0b000000111001110011100111000000)
#define VALIDBB   (00071634700)
//#define WGOALBB (0b000000111000000000000000000000)
#define WGOALBB   (00070000000)
//#define BGOALBB (0b000000000000000000000111000000)
#define BGOALBB   (00000000700)
/* move masks relative to piece at
 * 0,0 = 1<<6 = (0b000000000100000)
 *                 2cba21cba10cba0 */
//#define WPAWNBB (0b000100000000000)
#define WPAWNBB   (004000)
//#define WGOLDBB (0b001110010100010)
#define WGOLDBB   (016242)
//#define BPAWNBB (0b000000000000010)
#define BPAWNBB   (000002)
//#define BGOLDBB (0b000100010100111)
#define BGOLDBB   (004247)
//#define KINGBB  (0b001110010100111)
#define KINGBB    (016247)
//#define FERZBB  (0b001010000000101)
#define FERZBB    (012005)
//#define WAZIRBB (0b000100010100010)
#define WAZIRBB   (004242)

static const char piece_names[] = "?PWFKG";

/* **************************************** */

static const int debruijn_index64[64] = {
  63,  0, 58,  1, 59, 47, 53,  2,
  60, 39, 48, 27, 54, 33, 42,  3,
  61, 51, 37, 40, 49, 18, 28, 20,
  55, 30, 34, 11, 43, 14, 22,  4,
  62, 57, 46, 52, 38, 26, 32, 41,
  50, 36, 17, 19, 29, 10, 13, 21,
  56, 45, 25, 31, 35, 16,  9, 12,
  44, 24, 15,  8, 23,  7,  6,  5
};
// requires bb != 0
static inline int bit_scan_forward64(ui64 bb) {
  return debruijn_index64[((bb & -bb) * 0x07EDD5E59A4E28C2ULL) >> 58];
}
static const int debruijn_index32[32] = {
  0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
  31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};
// requires bb != 0
static inline int bit_scan_forward32(ui32 bb) {
  return debruijn_index32[((ui32)((bb & -bb) * 0x077CB531U)) >> 27];
}
static int  count_bits32(ui32 x) {
  x  = x - ((x >> 1) & 0x55555555);
  x  = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  x  = x + (x >> 4);
  x &= 0xF0F0F0F;
  return (x * 0x01010101) >> 24;
}
#define CB_m1  (0x5555555555555555ULL)
#define CB_m2  (0x3333333333333333ULL)
#define CB_m4  (0x0f0f0f0f0f0f0f0fULL)
#define CB_m8  (0x00ff00ff00ff00ffULL)
#define CB_m16 (0x0000ffff0000ffffULL)
#define CB_m32 (0x00000000ffffffffULL)
#define CB_hff (0xffffffffffffffffULL)
#define CB_h01 (0x0101010101010101ULL)
static int count_bits64(ui64 x) {
  x -= (x >> 1) & CB_m1;
  x = (x & CB_m2) + ((x >> 2) & CB_m2);
  x = (x + (x >> 4)) & CB_m4;
  return (x * CB_h01)>>56;
}

/* **************************************** */

static int add_move(piece p, int f, bitb tb, bitb oppo, movet* ml, int* n) {
  int t;
  while (tb) {
    t = bit_scan_forward32(tb);
    const bitb tsh = 1<<t;
    tb ^= tsh;
    ml[*n].i = 0;
    ml[*n].f = f;
    ml[*n].t = t;
    ml[*n].p = p;
    ml[*n].null = 0;
    ml[*n].valid = 1;
    ml[*n].capture = (oppo&tsh) ? 1 : 0;
    ml[*n].drop = (f<6) ? 1 : 0;
    ml[*n].promote = ((p==pawn) && !(f<6) && ((WGOALBB&tsh) || (BGOALBB&tsh)))
                    ? 1 : 0;
    ++*n;
  }
  return 0;
}
#define MOVES_FOR(c_,p_,all_,oppo_,mask_) { \
  bb = b->bb[(c_)][(p_)]; \
  while (bb) { \
    f = bit_scan_forward32(bb); \
    bb ^= (1<<f); \
    add_move((p_), f, VALIDBB&(all_)&((mask_)<<(f-6)), (oppo_), ml, &n); \
  } }
int gen_moves(const board* b, movet* ml) {
  int bb, f, k, n=0;
  bitb w_all = b->bb[white][gold]|b->bb[white][king]|b->bb[white][ferz]
              |b->bb[white][pawn]|b->bb[white][wazir];
  bitb b_all = b->bb[black][gold]|b->bb[black][king]|b->bb[black][ferz]
              |b->bb[black][pawn]|b->bb[black][wazir];
  if (b->to_move==white) {
    MOVES_FOR(white,gold,~w_all,b_all,WGOLDBB);
    MOVES_FOR(white,ferz,~w_all,b_all,FERZBB);
    MOVES_FOR(white,wazir,~w_all,b_all,WAZIRBB);
    MOVES_FOR(white,pawn,~w_all,b_all,WPAWNBB);
    MOVES_FOR(white,king,~w_all,b_all,KINGBB);
    if (b->hand[white][pawn])
      add_move(pawn, pawn, VALIDBB&~(w_all|b_all), b_all, ml, &n);
    if (b->hand[white][ferz])
      add_move(ferz, ferz, VALIDBB&~(w_all|b_all), b_all, ml, &n);
    if (b->hand[white][wazir])
      add_move(wazir, wazir, VALIDBB&~(w_all|b_all), b_all, ml, &n);
  } else if (b->to_move==black) {
    MOVES_FOR(black,gold,~b_all,w_all,BGOLDBB);
    MOVES_FOR(black,ferz,~b_all,w_all,FERZBB);
    MOVES_FOR(black,wazir,~b_all,w_all,WAZIRBB);
    MOVES_FOR(black,pawn,~b_all,w_all,BPAWNBB);
    MOVES_FOR(black,king,~b_all,w_all,KINGBB);
    if (b->hand[black][pawn])
      add_move(pawn, pawn, VALIDBB&~(w_all|b_all), w_all, ml, &n);
    if (b->hand[black][ferz])
      add_move(ferz, ferz, VALIDBB&~(w_all|b_all), w_all, ml, &n);
    if (b->hand[black][wazir])
      add_move(wazir, wazir, VALIDBB&~(w_all|b_all), w_all, ml, &n);
  }
  return n;
}

int gen_moves_cap(const board* b, movet* ml) {
  int bb, f, k, n=0;
  bitb w_all = b->bb[white][gold]|b->bb[white][king]|b->bb[white][ferz]
              |b->bb[white][pawn]|b->bb[white][wazir];
  bitb b_all = b->bb[black][gold]|b->bb[black][king]|b->bb[black][ferz]
              |b->bb[black][pawn]|b->bb[black][wazir];
  if (b->to_move==white) {
    MOVES_FOR(white,gold,b_all,b_all,WGOLDBB);
    MOVES_FOR(white,ferz,b_all,b_all,FERZBB);
    MOVES_FOR(white,wazir,b_all,b_all,WAZIRBB);
    MOVES_FOR(white,pawn,b_all,b_all,WPAWNBB);
    MOVES_FOR(white,king,b_all,b_all,KINGBB);
  } else if (b->to_move==black) {
    MOVES_FOR(black,gold,w_all,w_all,BGOLDBB);
    MOVES_FOR(black,ferz,w_all,w_all,FERZBB);
    MOVES_FOR(black,wazir,w_all,w_all,WAZIRBB);
    MOVES_FOR(black,pawn,w_all,w_all,BPAWNBB);
    MOVES_FOR(black,king,w_all,w_all,KINGBB);
  }
  return n;
}

#define ATTACKS_FOR(c_,p_,all_,mask_) { \
  bb = b->bb[(c_)][(p_)]; \
  while (bb) { \
    f = bit_scan_forward32(bb); \
    bb ^= (1<<f); \
    att |= (VALIDBB&(all_)&((mask_)<<(f-6))); \
  } }
static bitb attacks(const board* b, color c) {
  bitb att=0;
  int bb, f;
  bitb o_all = b->bb[!c][gold]|b->bb[!c][king]|b->bb[!c][ferz]
              |b->bb[!c][pawn]|b->bb[!c][wazir];
  ATTACKS_FOR(c,gold,o_all,c==white ? WGOLDBB : BGOLDBB);
  ATTACKS_FOR(c,ferz,o_all,FERZBB);
  ATTACKS_FOR(c,wazir,o_all,WAZIRBB);
  ATTACKS_FOR(c,pawn,o_all,c==white ? WPAWNBB : BPAWNBB);
  ATTACKS_FOR(c,king,o_all,KINGBB);
  return att;
}
static inline int in_check(const board* b, color c) {
  return (b->bb[c][king] & attacks(b,!c))!=0;
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
  b->bb[white][pawn]  = 1<<(2*5+2);
  b->bb[white][ferz]  = 1<<(1*5+1);
  b->bb[white][king]  = 1<<(1*5+2);
  b->bb[white][wazir] = 1<<(1*5+3);
  b->bb[black][pawn]  = 1<<(3*5+2);
  b->bb[black][ferz]  = 1<<(4*5+1);
  b->bb[black][king]  = 1<<(4*5+2);
  b->bb[black][wazir] = 1<<(4*5+3);
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
  sum += (b->h%21) - 10;
  return sum;
}
evalt evaluate_relative(const evaluator* e, const board* b, int ply) {
  return b->to_move==white ? evaluate(e,b,ply) : -evaluate(e,b,ply);
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
