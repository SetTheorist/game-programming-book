#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* ************************************************************ */
typedef unsigned char byte;
typedef unsigned long long hash;
enum {
  diag=1, ortho=2, runner=4,
  white=8, black=16,
  goal=32, border=64,
};
#define side_mask (white|black)
#define piece_mask (diag|ortho|runner)
typedef struct move {
  unsigned int fr:6;
  unsigned int to:6;
  unsigned int cap:1;
  unsigned int goal:1;
  unsigned int invalid:1;
} move;
typedef struct board {
  int to_move;
  int piece_count[2];
  int n_cap;
  int n_ply;
  hash h;
  byte b[64];
  byte cap_stack[256];
  hash hash_stack[256];
} board;
typedef enum tttype { ttinvalid=0, ttupper, ttlower, ttexact } tttype;
typedef struct ttentry {
  hash lock;
  double value;
  move best_move;
  tttype type;
  int depth;
} ttentry;
/* ************************************************************ */
#define XY2B(x_,y_) ((x_)+((y_)*8))
#define B2X(b_) ((b_)%8)
#define B2Y(b_) ((b_)/8)
/* ************************************************************ */
hash hashes[64][16];
hash wtm_hash;
int nodes = 0;
int pv_length[32];
move pv[32][32];
static double piece_vals[8] = {
  0.0,   1.0,   2.0,   3.0,
  0.0, 3*1.0, 3*2.0, 3*3.0,
};
static double piece_vals_td[8] = {
  0.0,   1.0,   2.3,   6.9,
  0.0,   7.9,   6.7,   8.9,
};

static double piece_square_vals[36][8];
#define W_WIN (1e10)
#define B_WIN (-1e10)
#define DRAW (0.0)
#define TTNUM (1024*256)
ttentry tt[TTNUM];
int tthits, tthits_false, tthits_shallow, tthits_deep,
    tthits_lower, tthits_exact, tthits_upper;
/* ************************************************************ */
unsigned int read_clock(void) {
  struct timeval timeval;
  struct timezone timezone;
  gettimeofday(&timeval, &timezone);
  return (timeval.tv_sec * 1000000 + (timeval.tv_usec));
}
/* ************************************************************ */
void show_board(FILE* f, const board* b);
void setup_board(board* b);
hash hash_board(const board* b);
void init_hash();
void tt_clear();
int tt_find(hash h, ttentry* tte);
int tt_store(hash lock, double value, move best_move, tttype type, int depth);
double evaluate(const board* b);
void del_evaluate(const board* b, double* d);
/* ************************************************************ */
void setup_board(board* b) {
  int i;
  memset(b, '\x00', sizeof(board));
  for (i=0; i<8; ++i) {
    b->b[XY2B(i,0)] = white|black|border;
    b->b[XY2B(i,7)] = white|black|border;
    b->b[XY2B(0,i)] = white|black|border;
    b->b[XY2B(7,i)] = white|black|border;
  }
  for (i=1; i<7; ++i) {
    b->b[XY2B(i,1)] = white|runner;
    b->b[XY2B(i,2)] = white;
    b->b[XY2B(i,5)] = black;
    b->b[XY2B(i,6)] = black|runner;
  }
  b->b[XY2B(2,1)] |= diag;
  b->b[XY2B(2,2)] |= diag;
  b->b[XY2B(2,5)] |= diag;
  b->b[XY2B(2,6)] |= diag;
  b->b[XY2B(3,1)] |= diag;
  b->b[XY2B(3,2)] |= diag;
  b->b[XY2B(3,5)] |= diag;
  b->b[XY2B(3,6)] |= diag;
  b->b[XY2B(4,1)] |= diag;
  b->b[XY2B(4,2)] |= diag;
  b->b[XY2B(4,5)] |= diag;
  b->b[XY2B(4,6)] |= diag;
  b->b[XY2B(5,1)] |= diag;
  b->b[XY2B(5,2)] |= diag;
  b->b[XY2B(5,5)] |= diag;
  b->b[XY2B(5,6)] |= diag;

  b->b[XY2B(1,1)] |= ortho;
  b->b[XY2B(1,2)] |= ortho;
  b->b[XY2B(1,5)] |= ortho;
  b->b[XY2B(1,6)] |= ortho;
  b->b[XY2B(3,1)] |= ortho;
  b->b[XY2B(3,2)] |= ortho;
  b->b[XY2B(3,5)] |= ortho;
  b->b[XY2B(3,6)] |= ortho;
  b->b[XY2B(4,1)] |= ortho;
  b->b[XY2B(4,2)] |= ortho;
  b->b[XY2B(4,5)] |= ortho;
  b->b[XY2B(4,6)] |= ortho;
  b->b[XY2B(6,1)] |= ortho;
  b->b[XY2B(6,2)] |= ortho;
  b->b[XY2B(6,5)] |= ortho;
  b->b[XY2B(6,6)] |= ortho;

  b->to_move = white;
  b->piece_count[0] = b->piece_count[1] = 12;
  b->hash_stack[b->n_ply++] = hash_board(b);
}
void show_board(FILE* f, const board* b) {
  int i, j;
  for (i=7; i>=0; --i) {
    for (j=0; j<8; ++j) {
      if (b->b[XY2B(j,i)]&border) {
        fprintf(f, "#####");
      } else {
        if (b->b[XY2B(j,i)]&piece_mask) {
          fprintf(f, "%c", b->b[XY2B(j,i)]&black ? 'B' : 'W');
          fprintf(f, "%c", b->b[XY2B(j,i)]&diag ? '/' : '_');
          fprintf(f, "%c", b->b[XY2B(j,i)]&ortho ? '+' : '_');
          fprintf(f, "%c", b->b[XY2B(j,i)]&runner ? '!' : '_');
          fprintf(f, " ");
        } else {
          fprintf(f, "     ");
        }
      }
    }
    fprintf(f, "\n");
  }
}
void show_move(FILE* f, move m) {
  if (m.invalid) { fprintf(f, "<invalid>"); return; }
  fprintf(f, "%c%c%c%c%c",
          'a'+B2X(m.fr)-1, '0'+B2Y(m.fr),
          m.cap ? 'x' : '-',
          'a'+B2X(m.to)-1, '0'+B2Y(m.to));
  fprintf(f, m.goal ? "+" : " ");
}

/* ************************************************************ */

static const int ortho_steps[4] = {+8, -8, +1, -1};
static const int diag_steps[4] = {+7, +9, -7, -9};
int add_move(const board* b, int fr, int to, move* ml, int* n) {
  if (b->b[to] & b->to_move) return 0;
  ml[*n].fr = fr;
  ml[*n].to = to;
  ml[*n].invalid = 0;
  ml[*n].goal =
      (b->to_move==white && (XY2B(1,6)<=to && to<=XY2B(6,6)))
      || (b->to_move==black && (XY2B(1,1)<=to && to<=XY2B(6,1)));
  if (b->b[to] & (b->to_move^side_mask)) {
    ml[*n].cap = 1;
    ++*n;
    return 0;
  } else {
    ml[*n].cap = 0;
    ++*n;
    return 1;
  }
}
int add_cap(const board* b, int fr, int to, move* ml, int* n) {
  if (b->b[to] & b->to_move) return 0;
  if (b->b[to] & (b->to_move^side_mask)) {
    ml[*n].fr = fr;
    ml[*n].invalid = 0;
    ml[*n].to = to;
    ml[*n].goal = (b->to_move==white && (XY2B(1,6)<=to && to<=XY2B(6,6)))
        || (b->to_move==black && (XY2B(1,1)<=to && to<=XY2B(6,1)));
    ml[*n].cap = 1;
    ++*n;
    return 0;
  } else if ((b->to_move==white && (XY2B(1,6)<=to && to<=XY2B(6,6)))
             || (b->to_move==black && (XY2B(1,1)<=to && to<=XY2B(6,1)))) {
    ml[*n].fr = fr;
    ml[*n].invalid = 0;
    ml[*n].to = to;
    ml[*n].goal = 1;
    ml[*n].cap = 0;
    ++*n;
    return 1;
  } else {
    return 1;
  }
}
int all_moves(const board* b, move* ml) {
  int i, j, to, n=0;
  for (i=XY2B(1,1); i<=XY2B(6,6); ++i) {
    const byte sq = b->b[i];
    if (sq & piece_mask) {
      if ((sq & side_mask) == b->to_move) {
        switch (sq & piece_mask) {
          case diag:
            for (j=0; j<4; ++j) add_move(b, i, i+diag_steps[j], ml, &n);
            break;
          case ortho:
            for (j=0; j<4; ++j) add_move(b, i, i+ortho_steps[j], ml, &n);
            break;
          case diag|ortho:
            for (j=0; j<4; ++j) add_move(b, i, i+diag_steps[j], ml, &n);
            for (j=0; j<4; ++j) add_move(b, i, i+ortho_steps[j], ml, &n);
            break;
          case runner|diag:
            for (j=0; j<4; ++j)
              for (to=i+diag_steps[j]; add_move(b, i, to, ml, &n);
                   to+=diag_steps[j]);
            break;
          case runner|ortho:
            for (j=0; j<4; ++j)
              for (to=i+ortho_steps[j]; add_move(b, i, to, ml, &n);
                   to+=ortho_steps[j]);
            break;
          case runner|diag|ortho:
            for (j=0; j<4; ++j)
              for (to=i+diag_steps[j]; add_move(b, i, to, ml, &n);
                   to+=diag_steps[j]);
            for (j=0; j<4; ++j)
              for (to=i+ortho_steps[j]; add_move(b, i, to, ml, &n);
                   to+=ortho_steps[j]);
            break;
          default: break;
        }
      }
    }
  }
  return n;
}
int all_caps(const board* b, move* ml) {
  int i, j, to, n=0;
  for (i=XY2B(1,1); i<=XY2B(6,6); ++i) {
    const byte sq = b->b[i];
    if (sq & piece_mask) {
      if ((sq & side_mask) == b->to_move) {
        switch (sq & piece_mask) {
          case diag:
            for (j=0; j<4; ++j) add_cap(b, i, i+diag_steps[j], ml, &n);
            break;
          case ortho:
            for (j=0; j<4; ++j) add_cap(b, i, i+ortho_steps[j], ml, &n);
            break;
          case diag|ortho:
            for (j=0; j<4; ++j) add_cap(b, i, i+diag_steps[j], ml, &n);
            for (j=0; j<4; ++j) add_cap(b, i, i+ortho_steps[j], ml, &n);
            break;
          case runner|diag:
            for (j=0; j<4; ++j)
              for (to=i+diag_steps[j]; add_cap(b, i, to, ml, &n);
                   to+=diag_steps[j]);
            break;
          case runner|ortho:
            for (j=0; j<4; ++j)
              for (to=i+ortho_steps[j]; add_cap(b, i, to, ml, &n);
                   to+=ortho_steps[j]);
            break;
          case runner|diag|ortho:
            for (j=0; j<4; ++j)
              for (to=i+diag_steps[j]; add_cap(b, i, to, ml, &n);
                   to+=diag_steps[j]);
            for (j=0; j<4; ++j)
              for (to=i+ortho_steps[j]; add_cap(b, i, to, ml, &n);
                   to+=ortho_steps[j]);
            break;
          default: break;
        }
      }
    }
  }
  return n;
}
void make_move(board* b, move m) {
  if (m.cap) {
    b->cap_stack[b->n_cap++] = b->b[m.to];
    if (b->to_move==white)
      --b->piece_count[1];
    else
      --b->piece_count[0];
  }
  b->b[m.to] = b->b[m.fr];
  b->b[m.fr] = 0;
  b->to_move ^= side_mask;
  b->h = b->hash_stack[b->n_ply++] = hash_board(b);
}
void unmake_move(board* b, move m) {
  b->h = b->hash_stack[--b->n_ply];
  b->to_move ^= side_mask;
  b->b[m.fr] = b->b[m.to];
  if (m.cap) {
    b->b[m.to] = b->cap_stack[--b->n_cap];
    if (b->to_move==white)
      ++b->piece_count[1];
    else
      ++b->piece_count[0];
  } else {
    b->b[m.to] = 0;
  }
}
void make_nullmove(board* b) {
  b->to_move ^= side_mask;
  b->h = b->hash_stack[b->n_ply++] = hash_board(b);
}
void unmake_nullmove(board* b) {
  b->to_move ^= side_mask;
  b->h = b->hash_stack[--b->n_ply];
}

/* ************************************************************ */
int terminal(const board* b, double* v) {
  int i;
  /* stalemate */
  if (b->piece_count[0]==0) { *v=B_WIN; return 1; }
  else if (b->piece_count[1]==0) { *v=W_WIN; return 1; }
  /* goal attainment */
  if (b->to_move==white) {
    for (i=XY2B(1,6); i<=XY2B(6,6); ++i) {
      if ((b->b[i] & side_mask) == white) {
        *v = W_WIN;
        return 1;
      }
    }
  } else if (b->to_move==black) {
    for (i=XY2B(1,1); i<=XY2B(6,1); ++i) {
      if ((b->b[i] & side_mask) == black) {
        *v = W_WIN;
        return 1;
      }
    }
  }
  /* draw by repetition */
  for (i=b->n_ply-2; i>=0; --i) {
    if (b->hash_stack[i] == b->hash_stack[b->n_ply-1]) {
      *v = DRAW;
      return 1;
    }
  }
  return 0;
}
int evaluate_new = 0;
double evaluate(const board* b) {
  double v = 0.0;
  int i;
  if (terminal(b, &v)) return v;
  if (evaluate_new) {
    for (i=XY2B(1,1); i<=XY2B(6,6); ++i) {
      if (b->b[i] & piece_mask) {
        v += ((b->b[i]&side_mask)==b->to_move)
            ? piece_vals_td[b->b[i]&piece_mask] : -piece_vals_td[b->b[i]&piece_mask];
      }
    }
  } else {
    for (i=XY2B(1,1); i<=XY2B(6,6); ++i) {
      if (b->b[i] & piece_mask) {
        v += ((b->b[i]&side_mask)==b->to_move)
            ? piece_vals[b->b[i]&piece_mask] : -piece_vals[b->b[i]&piece_mask];
      }
    }
  }
  v += ((int)(b->h % 201)-100)/500.0;
  return v;
}
void del_evaluate(const board* b, double* d) {
  int i;
  for (i=0; i<8; ++i) {
    double t = piece_vals[i];
    piece_vals[i] = t + (1.0/16.0);
    double vp = evaluate(b);
    piece_vals[i] = t - (1.0/16.0);
    double vm = evaluate(b);
    piece_vals[i] = t;
    //d[i] = (vp - vm) / (2.0*1.0/16.0);
    d[i] = (tanh(vp*0.5) - tanh(vm*0.5)) / (2.0*1.0/16.0);
  }
}
/* ************************************************************ */
static hash randhash() {
  hash h = 0ULL;
  int i;
  for (i=0; i<32; ++i)
    h ^= (((hash)lrand48())<<i)^((hash)lrand48())^(((hash)lrand48())<<(3*i));
  return h;
}
void init_hash() {
  int i, j;
  for (i=0; i<64; ++i)
    for (j=0; j<16; ++j)
      hashes[i][j] = randhash();
  wtm_hash = randhash();
}
hash hash_board(const board* b) {
  hash h= (b->to_move == white) ? wtm_hash : 0ULL;
  int i;
  for (i=XY2B(1,1); i<=XY2B(6,6); ++i) {
    if (b->b[i] & piece_mask)
      h ^= hashes[i][b->b[i]];
  }
  return h;
}
void tt_clear() {
  memset(tt, '\x00', sizeof(tt));
  tthits = tthits_false = tthits_shallow = tthits_deep
      = tthits_lower = tthits_exact = tthits_upper = 0;
}
int tt_find(hash h, ttentry* tte) {
  if (tt[h%TTNUM].type!=ttinvalid) {
    ++tthits;
    if (tt[h%TTNUM].lock != h) { ++tthits_false; return 0; }
    *tte = tt[h%TTNUM];
    return 1;
  }
  return 0;
}
int tt_store(hash lock, double value, move best_move, tttype type, int depth) {
  tt[lock%TTNUM].lock = lock;
  tt[lock%TTNUM].value = value;
  tt[lock%TTNUM].best_move = best_move;
  tt[lock%TTNUM].type = type;
  tt[lock%TTNUM].depth = depth;
  return 1;
}
/* ************************************************************ */
void clear_pv() {
  memset(pv_length, '\x00', sizeof(pv_length));
  memset(pv, '\x00', sizeof(pv));
}
double negamax_i(board* b, int depth, int use_tt) {
  ++nodes;
  hash h = b->h;
  if (use_tt) {
    ttentry tte;
    if (tt_find(h, &tte)) {
      if (tte.depth < depth) { ++tthits_shallow; goto ttdone; }
      if (tte.depth > depth) { ++tthits_deep; goto ttdone; }
      if (tte.type==ttexact) {
        ++tthits_exact;
        return tte.value;
      }
    }
  }
ttdone:
  { double v;
    move m = {0,0,0,0,1};
    if (terminal(b, &v)) {
      if (use_tt) tt_store(h, v, m, ttexact, depth);
      return v;
    } else if (depth<=0) {
      v = evaluate(b);
      if (use_tt) tt_store(h, v, m, ttexact, depth);
      return v;
    }
  }
  move ml[64];
  int i, nm=all_moves(b, ml), bm=0;
  double bv=-1.0/0.0;
  if (nm==0) return B_WIN;
  for (i=0; i<nm; ++i) {
    make_move(b, ml[i]);
    double v = -negamax_i(b, depth-10, use_tt);
    unmake_move(b, ml[i]);
    if (v > bv) { bv = v; bm = i; }
  }
  if (use_tt) tt_store(h, bv, ml[bm], ttexact, depth);
  return bv;
}
double negamax(board* b, move* m, int depth, int use_tt) {
  ++nodes;
  move ml[64];
  int i, bm=-1, nm=all_moves(b, ml);
  double bv=-1.0/0.0;
  for (i=0; i<nm; ++i) {
    make_move(b, ml[i]);
    double v = -negamax_i(b, depth-10, use_tt);
    unmake_move(b, ml[i]);
    if (v > bv) { bv = v; bm = i; }
  }
  *m = ml[bm];
  return bv;
}
double alphabeta_q(board* b, int qdepth, double alpha, double beta, int ply) {
  ++nodes;
  { double v;
    if (terminal(b, &v)) {
      pv_length[ply] = 0;
      return v;
    } else if (qdepth<=0) {
      v = evaluate(b);
      pv_length[ply] = 0;
      return v;
    }
  }
  move ml[64];
  double v = evaluate(b);
  if (v > alpha) {
    v=alpha;
    if (alpha>=beta) return alpha;
  }
  int bm=0, i, nm=all_caps(b, ml);
  for (i=0; i<nm; ++i) {
    make_move(b, ml[i]);
    double v = -alphabeta_q(b, qdepth-10, -beta, -alpha, ply+1);
    unmake_move(b, ml[i]);
    if (v > alpha) {
      alpha = v; bm = i;
      if (alpha>=beta) { return alpha; }
      memcpy(&pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*pv_length[ply+1]);
      pv[ply][ply] = ml[i];
      pv_length[ply] = pv_length[ply+1]+1;
    }
  }
  return alpha;
}
double alphabeta_i(board* b, int depth, double alpha, double beta, int use_tt, int ply, int qdepth) {
  ++nodes;
  hash h = b->h;
  { double v;
    move m = {0,0,0,0,1};
    if (terminal(b, &v)) {
      if (use_tt) tt_store(h, v, m, ttexact, depth);
      pv_length[ply] = 0;
      return v;
    } else if (depth<=0) {
      if (qdepth > 0) v = alphabeta_q(b, qdepth, alpha, beta, ply);
      else v = evaluate(b);
      if (use_tt) tt_store(h, v, m, ttexact, depth);
      pv_length[ply] = 0;
      return v;
    }
  }
  if (use_tt) {
    ttentry tte;
    if (tt_find(h, &tte)) {
      if (tte.depth < depth) { ++tthits_shallow; goto ttdone; }
      if (tte.depth > depth) { ++tthits_deep; goto ttdone; }
      switch (tte.type) {
        case ttexact:
          ++tthits_exact;
          if (tte.best_move.invalid) {
            pv_length[ply] = 0;
          } else {
            pv[ply][ply] = tte.best_move;
            pv_length[ply] = 1;
          }
          return tte.value;
        case ttupper:
          ++tthits_upper; if (tte.value < beta) beta = tte.value;
          if (alpha >= beta) {
            if (tte.best_move.invalid) {
              pv_length[ply] = 0;
            } else {
              pv[ply][ply] = tte.best_move;
              pv_length[ply] = 1;
            }
            return alpha;
          }
          break;
        case ttlower:
          ++tthits_lower; if (tte.value > alpha) alpha = tte.value;
          if (alpha >= beta) {
            if (tte.best_move.invalid) {
              pv_length[ply] = 0;
            } else {
              pv[ply][ply] = tte.best_move;
              pv_length[ply] = 1;
            }
            return beta;
          }
          break;
        default:
          goto ttdone;
      }
    }
  }
ttdone:{}
  move ml[64];
  int bm=-1, i, nm=all_moves(b, ml);
  tttype type = ttupper;
  if (nm==0) return B_WIN;
  for (i=0; i<nm; ++i) {
    make_move(b, ml[i]);
    double v = -alphabeta_i(b, depth-10, -beta, -alpha, use_tt, ply+1, qdepth);
    unmake_move(b, ml[i]);
    if (v > alpha) {
      alpha = v; bm = i; type = ttexact;
      memcpy(&pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*pv_length[ply+1]);
      pv[ply][ply] = ml[i];
      pv_length[ply] = pv_length[ply+1]+1;
    }
    if (alpha>=beta) {
      if (use_tt) tt_store(h, beta, ml[bm], ttlower, depth);
      return beta;
    }
  }
  if (use_tt) {
    if (bm>=0) tt_store(h, alpha, ml[bm], type, depth);
    else { move m={0,0,0,0,1}; tt_store(h, alpha, m, type, depth); }
  }
  return alpha;
}
double alphabeta(board* b, move* m, int depth, int use_tt, int qdepth) {
  ++nodes;
  move ml[64];
  int i, bm=-1, nm=all_moves(b, ml);
  double bv=-1.0/0.0;
  clear_pv();
  for (i=0; i<nm; ++i) {
    make_move(b, ml[i]);
    double v = -alphabeta_i(b, depth-10, B_WIN, W_WIN, use_tt, 1, qdepth);
    unmake_move(b, ml[i]);
    if (v > bv) {
      bv = v; bm = i;
      memcpy(&pv[0][1], &pv[1][1], sizeof(move)*pv_length[1]);
      pv[0][0] = ml[bm];
      pv_length[0] = pv_length[1]+1;
    }
  }
  *m = ml[bm];
  return bv;
}

int match(int d1, int d2, int qd1, int qd2, int dump, int ne1, int ne2,
          move game_pv[][32], int game_pv_length[], int* n_game) {
  board b;
  int i, j, nm=0;
  move gm[256];
  double v;
  setup_board(&b);
  for (i=0; i<200 && !terminal(&b, &v); ++i) {
    if (dump) printf("----------------------------------------\n");
    if (dump) { v=0.0; int t=terminal(&b, &v);
      printf("*** terminal=%i/%g  evaluation=%g", t, v, evaluate(&b));
      printf("  [%i|%i]", b.piece_count[0], b.piece_count[1]);
      printf("\n");
    }
    //if (dump) show_board(stdout, &b);
    if (dump) { move ml[256];
      nm=all_caps(&b, ml);
      for (j=0; j<nm; ++j) {
        show_move(stdout, ml[j]); printf((j%10)==9 ? "\n" : "  ");
      }
      printf("\n");
    }{ move ml[256];
      nm=all_moves(&b, ml);
      if (dump) for (j=0; j<nm; ++j) {
        show_move(stdout, ml[j]); printf((j%10)==9 ? "\n" : "  ");
      }
      if (dump) printf("\n");
      if (nm==0) break;
    }
    unsigned int bt, et, tt;
    int depth = b.to_move==white ? d1 : d2;
    int qdepth = b.to_move==white ? qd1 : qd2;
    evaluate_new = b.to_move==white ? ne1 : ne2;
    move m = {0,0,0,0,1};
    {
      nodes = 0;
      bt = read_clock();
      tt_clear();
      double v = alphabeta(&b, &m, depth, 1, qdepth);
      et = read_clock();
      tt = et - bt;
      if (dump) printf((i%2)?"%i.":"%i...", (i/2)+1);
      if (dump) show_move(stdout, m);
      if (dump) printf("  ( ");
      if (dump) for (j=0; j<pv_length[0]; ++j) {
        show_move(stdout, pv[0][j]);
        printf(" ");
      }
      if (dump) printf(")\n");
      if (dump) printf("    Alphabeta(%i): <<%3i>> {%6.2f} [%8i; %8.2fms; %8.2fknps]  ",
             1, depth, v, nodes, tt*1e-3, nodes/(tt*1e-3));
      if (dump) printf("    [tt:%i F:%i <:%i >:%i E:%i U:%i L:%i]\n",
             tthits, tthits_false, tthits_shallow, tthits_deep,
             tthits_exact, tthits_upper, tthits_lower);
    }
    game_pv_length[i] = pv_length[0];
    memcpy(game_pv[i], pv[0], sizeof(move)*pv_length[0]);
    make_move(&b, m);
    gm[i] = m;
  }
  *n_game = i;
  //if (dump) show_board(stdout, &b);
  if (terminal(&b, &v)) {
    if (v==0.0) {
      if (dump) printf("** DRAW BY REPETITION **\n");
      return 0;
    } else {
      if (b.to_move == white) {
        if (dump) printf("** WHITE WINS **\n");
        return 1;
      } else {
        if (dump) printf("** BLACK WINS **\n");
        return -1;
      }
    }
  } else {
    if (nm==0) {if (dump) printf("** STALEMATE **\n");}
    else {if (dump) printf("** DRAW BY EXHAUSTION **\n");}
    return 0;
  }
}

int comparison() {
  board b;
  int i;
  setup_board(&b);
  for (i=10; i<=80; i+=10) {
    fflush(stdout);
    unsigned int bt, et, tt;
    int t;
    if (i<=50) for (t=0; t<=1; ++t) {
      nodes = 0;
      move m = {0,0,0,0,1};
      bt = read_clock();
      if (t) tt_clear();
      double v = negamax(&b, &m, i, t);
      et = read_clock();
      tt = et - bt;
      printf("Negamax(%i)    <<%3i>> {%6.2f} [%8i; %8.2fms; %8.2fknps]  ",
             t, i, v, nodes, tt*1e-3, nodes/(tt*1e-3));
      show_move(stdout, m);
      printf("\n");
      if (t) printf("    [tt:%i F:%i <:%i >:%i E:%i U:%i L:%i]\n",
                    tthits, tthits_false, tthits_shallow, tthits_deep,
                    tthits_exact, tthits_upper, tthits_lower);
    }
    for (t=0; t<=2; ++t) {
      nodes = 0;
      move m = {0,0,0,0,1};
      bt = read_clock();
      if (t) tt_clear();
      double v = alphabeta(&b, &m, i, t, t==2 ? 4 : 0);
      et = read_clock();
      tt = et - bt;
      printf("Alphabeta(%i): <<%3i>> {%6.2f} [%8i; %8.2fms; %8.2fknps]  ",
             t, i, v, nodes, tt*1e-3, nodes/(tt*1e-3));
      show_move(stdout, m);
      printf("\n");
      if (t) printf("    [tt:%i F:%i <:%i >:%i E:%i U:%i L:%i]\n",
                    tthits, tthits_false, tthits_shallow, tthits_deep,
                    tthits_exact, tthits_upper, tthits_lower);
    }
  }
  return 0;
}

int round_robin() {
  int ni=3, nj=3;
  int i, j, k;
  int res[ni][nj][3];
  move game_pv[256][32];
  int game_pv_length[256];
  int n_game;
  memset(res, '\x00', sizeof(res));
  for (k=0; k<10; ++k) {
    for (i=0; i<ni; ++i) {
      for (j=0; j<nj; ++j) {
        init_hash();
        int r = match((i+1)*10,(j+1)*10,20,20,0,0,0,game_pv,game_pv_length,&n_game);
        if (r==0) {
          ++res[i][j][1];
        } else if (r==-1) {
          ++res[i][j][2];
        } else if (r==+1) {
          ++res[i][j][0];
        } else {
          printf("?");
        }
        int ii, jj;
        printf("\n");
        for (ii=0; ii<ni; ++ii) {
          for (jj=0; jj<nj; ++jj) {
            printf("(+%3i/-%3i/=%3i)   ", res[ii][jj][0], res[ii][jj][2], res[ii][jj][1]);
          }
          printf("\n");
        }
      }
    }
  }
  return 0;
}

int compare_evals() {
  int ni=4, nj=4;
  int i, j, k;
  int res[ni][nj][3];
  move game_pv[256][32];
  int game_pv_length[256];
  int n_game;
  memset(res, '\x00', sizeof(res));
  for (k=0; k<20; ++k) {
    for (i=0; i<ni; ++i) {
      for (j=0; j<nj; ++j) {
        int d1 = 10*((i/2)+2);
        int d2 = 10*((j/2)+2);
        int ne1 = !(i%2);
        int ne2 = !(j%2);
        init_hash();
        int r = match(d1,d2,10,10,0,ne1,ne2,game_pv,game_pv_length,&n_game);
        if (r==0) {
          ++res[i][j][1];
        } else if (r==-1) {
          ++res[i][j][2];
        } else if (r==+1) {
          ++res[i][j][0];
        } else {
          printf("?");
        }
        int ii, jj;
        printf("\n");
        for (ii=0; ii<ni; ++ii) {
          for (jj=0; jj<nj; ++jj) {
            printf("(+%3i/-%3i/=%3i)   ", res[ii][jj][0], res[ii][jj][2], res[ii][jj][1]);
          }
          printf("\n");
        }
      }
    }
  }
  return 0;
}

int td_lambda() {
  int kk, i, j, dump=0;
  for (i=0; i<8; ++i) {
    printf("%.4f ", piece_vals[i]);
  }
  printf("\n");

  for (kk=0; kk<1000; ++kk) {
    move game_pv[256][32];
    int game_pv_length[256];
    int n_game;
    double dels[256][16];
    double dw[256];
    double ee[256];
    double jj[256];
    double dd[256];
    int n_states = 0;
    double lambda = 0.7, alpha = 0.001; // * (100.0/(100.0+kk));
    int result, parity;
    board b;
    setup_board(&b);
    init_hash();
    //static double piece_square_vals[36][8];

    result = match(20,20,10,10,0,0,0,game_pv,game_pv_length,&n_game);

    for (parity=0; parity<=1; ++parity) {
      for (i=0; i<n_game; ++i) {
        if (dump) {
          for (j=0; j<game_pv_length[i]; ++j) {
            show_move(stdout, game_pv[i][j]);
            printf("  ");
          }
          printf("\n");
        }
        for (j=0; j<game_pv_length[i]; ++j) make_move(&b, game_pv[i][j]);
        if (i%2==parity) {
          del_evaluate(&b, dels[n_states]);
          ee[n_states] = evaluate(&b);
          jj[n_states] = tanh(ee[n_states]*0.5);
        }
        for (j=game_pv_length[i]-1; j>=0; --j) unmake_move(&b, game_pv[i][j]);
        if (i%2==parity) {
          if (dump) {
            printf("[%5.2f](%5.4f) ", ee[n_states], jj[n_states]);
            for (j=0; j<8; ++j) printf(" %5.2f", dels[n_states][j]);
            printf("\n");
          }
          ++n_states;
        }
        make_move(&b, game_pv[i][0]);
      }
      if (dump) printf("<<%i>>\n", result);

      jj[n_states] = (parity==0) ? result : -result;
      for(i=0; i<n_states; ++i) dd[i] = jj[i+1] - jj[i];
      if (dump) {for(i=0;i<n_states;++i)printf("%.4f ",dd[i]);printf("\n");}

      for (i=0; i<8; ++i) {
        int t;
        double sum = 0.0;
        for (t=0; t<n_states; ++t) {
          double sum2 = 0.0;
          for (j=t; j<n_states; ++j) {
            sum2 += pow(lambda, j-t)*dd[j];
          }
          sum += dels[t][i];
        }
        dw[i] = alpha * sum;
      }
      if (dump) {for(i=0;i<8;++i)printf("%.4f ",dw[i]);printf("\n");}

      for (i=0; i<8; ++i) piece_vals[i] += dw[i];
      for (i=0; i<8; ++i) piece_vals[i] /= piece_vals[1];
      for (i=0; i<8; ++i) printf("%.4f ", piece_vals[i]); printf("\n");
    }
  }

  return 0;
}

/* ************************************************************ */
int main(int argc, char* argv[]) {
  srand48(argc>1 ? atoi(argv[1]) : 13171923);
  init_hash();

  comparison();
  //round_robin();
  //td_lambda();
  //compare_evals();

  return 0;
}
