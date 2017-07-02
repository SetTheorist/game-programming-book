/* $Id: mancala.c,v 1.1 2009/12/18 04:21:04 apollo Exp apollo $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define  SHOW_MOVES    1

typedef unsigned char    uint8;
typedef unsigned short    uint16;
typedef unsigned int    uint32;
typedef unsigned long long  uint64;

/* a version of mancala:
 *
 * - 6 pits on each side
 * - start with 4 seeds in each pit
 * - sow into pits and your own kalah but not opponent's
 * - extra move if last seed in your own kalah
 *
 */

/* utility */
unsigned int read_clock(void) {
  struct timeval timeval;
  struct timezone timezone;
  gettimeofday(&timeval, &timezone);
  return (timeval.tv_sec * 1000000 + (timeval.tv_usec));
}

typedef  uint64  hasht;

typedef enum colort {
  white, black, none
} colort;
typedef enum flagst {
  extramove=1, capture=2, sweep=4, gameover=8
} flagst;

typedef struct move {
  uint8  pit;
  uint8  num;
  uint8  flags;
  uint8  capture;
} move;

typedef struct board {
  int    pits[12];
  int    kalah[2];
  colort  side;
  colort  xside;
  hasht  hash;
  int    ply;
  move  hist[1024];
  hasht  hash_hist[1024];
} board;

typedef struct state {
  int    pits[12];
  int    kalah[2];
  colort  side;
  hasht  hash;
} state;

typedef enum ttent_flags {
  tt_valid=1, tt_exact=2, tt_upper=4, tt_lower=8
} ttent_flags;

typedef struct ttent {
  hasht  hash;
  int    score;
  int    flags;
  move  best_move;
  int    depth;
} ttent;

hasht  hash_values[12][49];
hasht  hash_to_move;

hasht gen_hash()
{
  hasht  h = 0;
  int  i;
  for (i=0; i<64; ++i)
    h = h ^ ((hasht)(lrand48())<<i) ^ ((hasht)(lrand48())>>i);
  return h;
}
void init_hash()
{
  int  i, j;
  for (i=0; i<12; ++i)
    for (j=0; j<49; ++j)
      hash_values[i][j] = gen_hash();
  hash_to_move = gen_hash();
}
hasht hash_board(board* b)
{
  int  i;
  hasht  hash = b->side==white ? hash_to_move : 0;
  for (i=0; i<12; ++i)
    hash = hash ^ hash_values[i][b->pits[i]];
  b->hash = hash;
  return hash;
}

#define  NTT  (1024*16)
ttent  ttable[NTT];
void init_ttable()
{
  memset(ttable, 0, sizeof(ttable));
}

inline int
put_ttable(hasht hash, int score, move m, int flags, int depth)
{
  int  loc = hash%NTT;
  if (loc<0) printf("!Error in add_ttable()!\n");
  /* most-recent: replaces without checking
   * deepest: replaces only if as deep or deeper
   * --- inital testing seems to say most-recent gives fewer nodes
   */
//  if (!(ttable[loc].flags&tt_valid) || (ttable[loc].depth>=depth))
  {
    ttable[loc].hash = hash;
    ttable[loc].score = score;
    ttable[loc].best_move = m;
    ttable[loc].flags = flags|tt_valid;
    ttable[loc].depth = depth;
  }
  return 0;
}
inline ttent*
get_ttable(hasht hash)
{
  return &ttable[hash%NTT];
}

void init_board(board* b)
{
  int  i;
  memset(b, 0, sizeof(*b));
  for (i=0; i<12; ++i)
    b->pits[i] = 4;
  b->side = white;
  b->xside = black;
  b->ply = 0;
  b->hash_hist[b->ply] = hash_board(b);
}

int
show_board(FILE* f, board* b)
{
  int  i;
  fprintf(f, "    +-----------------------------+     [%016llX]\n", b->hash);//hash_board(b));
  fprintf(f, "(%2i)|", b->kalah[black]);
  for (i=11; i>=6; --i) fprintf(f, " %2i |", b->pits[i]);
  fprintf(f, "\n");
  fprintf(f, "    +-----------------------------+\n");
  fprintf(f, "    |");
  for (i=0; i<6; ++i) fprintf(f, " %2i |", b->pits[i]);
  fprintf(f, "(%2i)\n", b->kalah[white]);
  fprintf(f, "    +-----------------------------+\n");
  if (b->side==white)
    fprintf(f, " White to move\n");
  else if (b->side==black)
    fprintf(f, " Black to move\n");
  else if ((b->side==none) && (b->xside==white))
    fprintf(f, " ** White wins **\n");
  else if ((b->side==none) && (b->xside==black))
    fprintf(f, " ** Black wins **\n");
  else if ((b->side==none) && (b->xside==none))
    fprintf(f, " ** Draw **\n");
  return 0;
}

int
show_move(FILE* f, move m)
{
  fprintf(f, " %2i", m.pit);
  if (m.flags & capture) fprintf(f, "x");
  if (m.flags & extramove) fprintf(f, "+");
  if (m.flags & sweep) fprintf(f, "!");
  if (m.flags & gameover) fprintf(f, "#");
  return 0;
}

const int sow_map[2][13] = {
  {0,1,2,3,4,5,-1,6,7,8,9,10,11},
  {0,1,2,3,4,5,6,7,8,9,10,11,-1}
};

int
gen_moves(board* b, move* ml)
{
  int  i, n=0;
  /* TODO: flags for capture, endgame, sweep */
  if (b->side==white) {
    for (i=0; i<6; ++i) {
      if (b->pits[i]) {
        ml[n].pit = i;
        ml[n].num = b->pits[i];
        ml[n].flags = 0;
        /* extra move */
        if ((b->pits[i]%13)==(6-i)) ml[n].flags |= extramove;
        /* captures */
        if (ml[n].num==13) {
          ml[n].flags |= capture;
          ml[n].capture = i;
        } else if (ml[n].num<13) {
          int sow = sow_map[b->side][(ml[n].pit+ml[n].num)%13];
          if ((sow>=0)&&(sow<6)&&(b->pits[sow]==0)) {
            ml[n].flags |= capture;
            ml[n].capture = i;
          }
        }
        ++n;
      }
    }
  } else if (b->side==black) {
    for (i=6; i<12; ++i) {
      if (b->pits[i]) {
        ml[n].pit = i;
        ml[n].num = b->pits[i];
        ml[n].flags = 0;
        /* extra move */
        if ((b->pits[i]%13)==(12-i)) ml[n].flags |= extramove;
        /* captures */
        if (ml[n].num==13) {
          ml[n].flags |= capture;
          ml[n].capture = i;
        } else if (ml[n].num<13) {
          int sow = sow_map[b->side][(ml[n].pit+ml[n].num)%13];
          if ((sow>=6)&&(sow<13)&&(b->pits[sow]==0)) {
            ml[n].flags |= capture;
            ml[n].capture = i;
          }
        }
        ++n;
      }
    }
  } else {
    /* TODO: */
  }
  return n;
}
move make_move(board* b, move m, state* s)
{
  memcpy(s->pits, b->pits, sizeof(s->pits));
  memcpy(s->kalah, b->kalah, sizeof(s->kalah));
  s->side = b->side;
  s->hash = b->hash;

  if (b->side==none) { printf("<NOBODY TO MOVE IN MAKE_MOVE?!>\n"); return m; }

  int  i;
  int  per = m.num / 13;
  int rem = m.num % 13;

  b->pits[m.pit] = 0;
  b->kalah[b->side] += per;
  for (i=0; i<12; ++i) b->pits[i] += per;
  for (i=0; i<rem; ++i) {
    int sow = sow_map[b->side][(m.pit+1+i)%13];
    if (sow!=-1)
      ++b->pits[sow];
    else
      ++b->kalah[b->side];
  }
  if (m.flags&capture) {
    int sow = sow_map[b->side][(m.pit+rem)%13];
    b->kalah[b->side] += 1;
    b->pits[sow] = 0;
    b->kalah[b->side] += b->pits[11-sow];
    b->pits[11-sow] = 0;
  }

  int tot=0;
  //if (((b->side==black)&&!(m.flags&extramove))||((b->side==white)&&(m.flags&extramove))) 
  {
    tot = 0;
    for (i=0; i<6; ++i)
      tot += b->pits[i];
    if (tot==0) {
      m.flags |= sweep|gameover;
      for (i=6; i<12; ++i) {
        tot += b->pits[i];
        b->pits[i] = 0;
      }
      b->kalah[black] += tot;
    }
  }
  // else if (((b->side==white)&&!(m.flags&extramove))||((b->side==black)&&(m.flags&extramove))) 
  {
    tot = 0;
    for (i=6; i<12; ++i)
      tot += b->pits[i];
    if (tot==0) {
      m.flags |= sweep|gameover;
      for (i=0; i<6; ++i) {
        tot += b->pits[i];
        b->pits[i] = 0;
      }
      b->kalah[white] += tot;
    }
  }

  /* TODO: cleanup */
  if (m.flags&gameover) {
    b->side = none;
    if (b->kalah[white]>b->kalah[black])
      b->xside = white;
    else if (b->kalah[white]<b->kalah[black])
      b->xside = black;
    else
      b->xside = none;
  } else {
    if (!(m.flags&extramove)) {
      const colort  t = b->side;
      b->side = b->xside;
      b->xside = t;
    }
  }

  b->hash_hist[b->ply] = hash_board(b);
  b->hist[b->ply] = m;
  ++b->ply;

  return m;
}
int unmake(board* b, state* s)
{
  memcpy(b->pits, s->pits, sizeof(s->pits));
  memcpy(b->kalah, s->kalah, sizeof(s->kalah));
  b->hash = s->hash;
  b->side = s->side;
  b->xside = (s->side==white) ? black : white;
  --b->ply;
  return 0;
}

int  nodes_evaluated;
int
evaluate(board* b, int ply)
{
  ++nodes_evaluated;
  int  sum = b->kalah[white] - b->kalah[black];
  return sum;
}

/* sort moves by score value (in stupid fashion) */
void
sort_moves(int n, move* ml, int* scorel)
{
  int  i, j;
  for (i=0; i<n; ++i) {
    for (j=i+1; j<n; ++j) {
      if (scorel[i] < scorel[j]) {
        int ts = scorel[i];
        move tm = ml[i];

        scorel[i] = scorel[j];
        ml[i] = ml[j];

        scorel[j] = ts;
        ml[j] = tm;
      }
    }
  }
}
/* just bring the best score from ml[i]..ml[n-1] to position i (swap it) */
inline void
sort_it(int i, int n, move* ml, int* scorel)
{
  int  best_score = scorel[i];
  int  besti = i;
  int  j;
  for (j=i+1; j<n; ++j) {
    if (scorel[j]>best_score) {
      best_score = scorel[j];
      besti = j;
    }
  }
  if (besti!=i) {
    int ts = scorel[i];
    move tm = ml[i];

    scorel[i] = scorel[besti];
    ml[i] = ml[besti];

    scorel[besti] = ts;
    ml[besti] = tm;
  }
}

move  pv[64][64];
int    pv_length[64];

int
search_minmax(board* b, int depth, int ply)
{
  if (depth<=0||b->side==none)
    return evaluate(b, ply);

  move  ml[6];
  int    n, i;
  int    best_eval = b->side==white ? -49 : 49;
  n = gen_moves(b, ml);
  if (n==0) {printf("<ZERO MOVES IN MINMAX!>\n");fflush(stdout);return b->side==white?-49:49;}

  for (i=0; i<n; ++i) {
    state  s;
    ml[i] = make_move(b, ml[i], &s);
    int val = search_minmax(b, depth-100, ply+1);
    unmake(b, &s);
    if (b->side==white) {
      if (val>best_eval) {
        best_eval = val;
        memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*(depth-100)/100);
        pv[ply][ply] = ml[i];
      }
    } else {
      if (val<best_eval) {
        best_eval = val;
        memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*(depth-100)/100);
        pv[ply][ply] = ml[i];
      }
    }
  }

  return best_eval;
}
int
search_negamax(board* b, int depth, int ply)
{
  /* NB that all non-white & non-black sides are draws==0 */
  if (b->side==none)
    return b->xside==black ? evaluate(b, ply) : -evaluate(b, ply);
  if (depth<=0)
    return b->side==white ? evaluate(b, ply) : -evaluate(b, ply);

  move  ml[6];
  int    n, i;
  int    best_eval = -49;
  colort  side = b->side;
  n = gen_moves(b, ml);
  if (n==0) {printf("<ZERO MOVES IN NEGAMAX!>\n");fflush(stdout);return -49;}

  for (i=0; i<n; ++i) {
    int  val;
    state  s;
    ml[i] = make_move(b, ml[i], &s);
    if (b->side==side) {
      val = search_negamax(b, depth-100, ply+1);
    } else {
      val = -search_negamax(b, depth-100, ply+1);
    }
    unmake(b, &s);
    if (val>best_eval) {
      best_eval = val;
      memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*(depth-100)/100);
      pv[ply][ply] = ml[i];
    }
  }

  return best_eval;
}
int
search_alphabeta(board* b, int depth, int alpha, int beta, int ply)
{
  /* NB that all (non-white & non-black) sides are draws==0 */
  if (b->side==none)
    return b->xside==black ? evaluate(b, ply) : -evaluate(b, ply);
  if (depth<=0)
    return b->side==white ? evaluate(b, ply) : -evaluate(b, ply);

  move  ml[6];
  int    n, i;
  colort  side = b->side;
  n = gen_moves(b, ml);
  if (n==0) {printf("<ZERO MOVES IN ALPHABETA!>\n");fflush(stdout);return -49;}

  for (i=0; i<n; ++i) {
    state  s;
    int  val;
    ml[i] = make_move(b, ml[i], &s);
    if (b->side==side) {
      val = search_alphabeta(b, depth-100, alpha, beta, ply+1);
    } else {
      val = -search_alphabeta(b, depth-100, -beta, -alpha, ply+1);
    }
    unmake(b, &s);
    if (val>alpha) {
      alpha = val;
      /* cutoff */
      if (alpha>=beta) break;
      memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*(depth-100)/100);
      pv[ply][ply] = ml[i];
    }
  }

  return alpha;
}

int tt_hit;
int tt_false;
int tt_deep;
int tt_shallow;
int tt_used;
int
search_abtt(board* b, int depth, int alpha, int beta, int ply)
{
  /* NB that all (non-white & non-black) sides are draws==0 */
  if (b->side==none)
    return b->xside==black ? evaluate(b, ply) : -evaluate(b, ply);
  if (depth<=0)
    return b->side==white ? evaluate(b, ply) : -evaluate(b, ply);

  move  ml[6];
  int    n, i;
  colort  side = b->side;

  hash_board(b);
  ttent*  tt = get_ttable(b->hash);
  if (tt->flags & tt_valid) {
    ++tt_hit;
    if (tt->hash!=b->hash) {
      ++tt_false;
      goto skip_tt;
    }
    if (tt->depth<depth) {
      ++tt_shallow;
      goto skip_tt;
    }
    if (tt->depth>depth) {
      ++tt_deep;
      goto skip_tt;
    }
    ++tt_used;
    return tt->score;
  }
skip_tt:


  n = gen_moves(b, ml);
  if (n==0) {printf("<ZERO MOVES IN ABTT!>\n");fflush(stdout);return -49;}

  hasht  hpre = b->hash;
  //hasht  hpre_ = hash_board(b);
  //if (hpre!=hpre_) { printf("$<%016llX!=%016llX>$", hpre, hpre_); }
  
  for (i=0; i<n; ++i) {
    int  val;
    state  s;
    ml[i] = make_move(b, ml[i], &s);
    if (b->side==side) {
      val = search_abtt(b, depth-100, alpha, beta, ply+1);
    } else {
      val = -search_abtt(b, depth-100, -beta, -alpha, ply+1);
    }
    unmake(b, &s);
    if (val>alpha) {
      alpha = val;
      /* cutoff */
      if (alpha>=beta) break;
      memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*(depth-100)/100);
      pv[ply][ply] = ml[i];
    }
  }
  hasht  hpost = b->hash;
  if (hpre!=hpost) { printf("!<%016llX!=%016llX>!", hpre, hpost); }
  //hasht  hpost_ = hash_board(b);
  //if (hpost!=hpost_) { printf("@<%016llX!=%016llX>@", hpre, hpre_); }

  if (alpha<beta) {
    put_ttable(b->hash, alpha, pv[ply][ply], tt_exact, depth);
  }
  return alpha;
}
int
search_abttx(board* b, int depth, int alpha, int beta, int ply)
{
  /* NB that all (non-white & non-black) sides are draws==0 */
  if (b->side==none) return b->xside==black ? evaluate(b, ply) : -evaluate(b, ply);
  if (depth<=0) return b->side==white ? evaluate(b, ply) : -evaluate(b, ply);

  move  ml[6];
  int    scorel[6];
  int    n, i;
  move  goodm;
  int    have_goodm = 0;
  colort  side = b->side;

  hash_board(b);
  ttent*  tt = get_ttable(b->hash);
  if (tt->flags & tt_valid) {
    ++tt_hit;
    if (tt->hash!=b->hash) {
      ++tt_false;
      goto skip_tt;
    }
    if (tt->depth<depth) {
      ++tt_shallow;
      have_goodm = 1;
      goodm = tt->best_move;
      goto skip_tt;
    }
    if (tt->depth>depth) {
      ++tt_deep;
      //goto skip_tt;
    }
    ++tt_used;
    return tt->score;
  }
skip_tt:

  n = gen_moves(b, ml);
  if (n==0) {printf("<ZERO MOVES IN ABTTX!>\n");fflush(stdout);return -49;}
  if (n>6) {printf("<TOO MANY MOVES IN ABTTX!  INTERNAL INCONSISTENCY!>\n");fflush(stdout);return -49;}
  /* initialize scores by capture value */
  //sort_moves(n, ml, scorel);

  if (have_goodm) {
    /* bonus score to transition-table iteration good move so it's used first */
    for (i=0; i<n; ++i)
    {
      if (goodm.pit==ml[i].pit) {
        scorel[i] += 49;
        break;
      }
    }
  }

  hasht  hpre = b->hash;
  for (i=0; i<n; ++i) {
    state  s;
    int  val;
    sort_it(i, n, ml, scorel);
    ml[i] = make_move(b, ml[i], &s);
    if (b->side==side) {
      val = search_abttx(b, depth-100, alpha, beta, ply+1);
    } else {
      val = -search_abttx(b, depth-100, -beta, -alpha, ply+1);
    }
    unmake(b, &s);
    if (val>alpha) {
      alpha = val;
      /* cutoff */
      if (alpha>=beta) break;
      memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*(depth-100)/100);
      pv[ply][ply] = ml[i];
    }
  }
  hasht  hpost = b->hash;

  if (hpre!=hpost) { printf("!<%016llX!=%016llX>!", hpre, hpost); }
  if (alpha<beta) {
    put_ttable(b->hash, alpha, pv[ply][ply], tt_exact, depth);
  }

  return alpha;
}

move
search1(board* b, int depth, int* eval)
{
  move  ml[6];
  move  bestm={0,0};
  int    bestv = b->side==white ? -49 : 49;
  int    i, n;

  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH1!>\n");

  for (i=0; i<n; ++i) {
    state s;
    ml[i] = make_move(b, ml[i], &s);
    int  v = search_minmax(b, depth, 1);
    unmake(b, &s);
    if (SHOW_MOVES) { printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%8==7) printf("\n"); }

    if (b->side==white) {
      if (v>bestv) {
        bestm = ml[i];
        bestv = v;
        memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(depth-100)/100);
        pv[0][0] = ml[i];
      }
    } else {
      if (v<bestv) {
        bestm = ml[i];
        bestv = v;
        memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(depth-100)/100);
        pv[0][0] = ml[i];
      }
    }
  }
  if (SHOW_MOVES) printf("\n");
  
  *eval = bestv;
  return bestm;
}

move
search2(board* b, int depth, int* eval)
{
  move  ml[6];
  move  bestm={0,0};
  int    bestv = -49;
  int    i, n;
  colort  side = b->side;

  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH2!>\n");

  for (i=0; i<n; ++i) {
    int  v;
    state s;
    ml[i] = make_move(b, ml[i], &s);
    if (b->side==side) {
      v = search_negamax(b, depth, 1);
    } else {
      v = -search_negamax(b, depth, 1);
    }
    unmake(b, &s);
    if (SHOW_MOVES) { printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%8==7) printf("\n"); }

    if (v>bestv) {
      bestm = ml[i];
      bestv = v;
      memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(depth-100)/100);
      pv[0][0] = ml[i];
    }
  }
  if (SHOW_MOVES) printf("\n");
  
  *eval = bestv;
  return bestm;
}

move
search3(board* b, int depth, int* eval)
{
  move  ml[6];
  move  bestm={0,0};
  int    bestv = -49;
  int    i, n;
  colort  side = b->side;

  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH3!>\n");

  for (i=0; i<n; ++i) {
    state s;
    int  v;
    ml[i] = make_move(b, ml[i], &s);
    if (b->side==side) {
      v = search_alphabeta(b, depth, bestv, 49, 1);
    } else {
      v = -search_alphabeta(b, depth, -49, -bestv, 1);
    }
    unmake(b, &s);
    if (SHOW_MOVES) { printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%8==7) printf("\n"); }

    if (v>bestv) {
      bestm = ml[i];
      bestv = v;
      memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(depth-100)/100);
      pv[0][0] = ml[i];
    }
  }
  if (SHOW_MOVES) printf("\n");
  
  *eval = bestv;
  return bestm;
}
move
search3tt(board* b, int depth, int* eval)
{
  move  ml[6];
  move  bestm={0,0};
  int    bestv = -49;
  int    i, n;

  
  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH3TT!>\n");

  hasht hpre = b->hash;
  for (i=0; i<n; ++i) {
    state s;
    ml[i] = make_move(b, ml[i], &s);
    int  v = -search_abtt(b, depth, -49, -bestv, 1);
    unmake(b, &s);
    if (b->hash!=hpre) printf("<!>");
    if (SHOW_MOVES) { printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%8==7) printf("\n"); }

    if (v>bestv) {
      bestm = ml[i];
      bestv = v;
      memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(depth-100)/100);
      pv[0][0] = ml[i];
    }
  }
  if (SHOW_MOVES) printf("\n");
  
  *eval = bestv;
  return bestm;
}

move
search4(board* b, int depth, int* eval)
{
  move  ml[6];
  int    scorel[6];
  move  bestm={0,0};
  int    bestv = -49;
  int    i, n, d;
  int    pre, post;


  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH4!>\n");

  /* initialize scores by capture value */

  for (d=100; d<=depth; d+=100) {
    bestv = -49;
    pre = nodes_evaluated;
    for (i=0; i<n; ++i) {
      state s;
      sort_it(i, n, ml, scorel);
      ml[i] = make_move(b, ml[i], &s);
      int  v = -search_alphabeta(b, d, -49, -bestv, 1);
      unmake(b, &s);
      if (SHOW_MOVES) {
        printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%8==7) printf("\n");
      }

      if (v>bestv) {
        bestm = ml[i];
        bestv = v;
        memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(d-100)/100);
        pv[0][0] = ml[i];
      }
      scorel[i] = v;
    }
    if (SHOW_MOVES) printf("\n");
    post = nodes_evaluated;
    printf("{{Nodes for iter %i = %i}}\n", d, post-pre);
    fflush(stdout);
  }
  *eval = bestv;
  return bestm;
}
move search4tt(board* b, int depth, int* eval) {
  move  ml[6];
  int    scorel[6];
  move  bestm={0,0};
  int    bestv = -49;
  int    i, n, d;
  int    pre, post;


  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH4TT!>\n");

  /* initialize scores by capture value */

  for (d=100; d<=depth; d+=100) {
    bestv = -49;
    pre = nodes_evaluated;
    for (i=0; i<n; ++i) {
      state s;
      sort_it(i, n, ml, scorel);
      ml[i] = make_move(b, ml[i], &s);
      int  v = -search_abtt(b, d, -49, -bestv, 1);
      unmake(b, &s);
      if (SHOW_MOVES) {
        printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%8==7) printf("\n");
      }

      if (v>bestv) {
        bestm = ml[i];
        bestv = v;
        memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(d-100)/100);
        pv[0][0] = ml[i];
      }
      scorel[i] = v;
    }
    if (SHOW_MOVES) printf("\n");
    post = nodes_evaluated;
    printf("{{Nodes for iter %i = %i}}\n", d, post-pre);
    fflush(stdout);
  }
  *eval = bestv;
  return bestm;
}
move search4ttx(board* b, int depth, int* eval) {
  move  ml[6];
  int    scorel[6];
  move  bestm={0,0};
  int    bestv = -49;
  int    i, n, d;
  int    pre, post;


  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH4TT!>\n");

  /* initialize scores by capture value */

  for (d=100; d<=depth; d+=100) {
    bestv = -49;
    pre = nodes_evaluated;
    for (i=0; i<n; ++i) {
      state s;
      sort_it(i, n, ml, scorel);
      ml[i] = make_move(b, ml[i], &s);
      int  v = -search_abttx(b, d, -49, -bestv, 1);
      unmake(b, &s);
      if (SHOW_MOVES) {
        printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%8==7) printf("\n");
      }

      if (v>bestv) {
        bestm = ml[i];
        bestv = v;
        memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(d-100)/100);
        pv[0][0] = ml[i];
      }
      /* save value for sorting next iteration */
      scorel[i] = v;
    }
    if (SHOW_MOVES) printf("\n");
    post = nodes_evaluated;
    printf("{{Nodes for iter %i = %i}}\n", d, post-pre);
    fflush(stdout);
  }
  *eval = bestv;
  return bestm;
}

int main(int argc, char* argv[]) {
  board  b;
  int  i, n, d;
  int  depths[2] = {800,800};
  move  ml[6];
  state s[256];
//#define  MAXW  16
#define  MAXW  8
#define  MAXB  4
  colort  results[MAXW][MAXB];
  int  dw, db;

  srand48(-13);

#if 1
  for (dw=0; dw<MAXW; ++dw) {
  depths[white] = 100*(dw+1);
  for (db=0; db<MAXB; ++db) {
  depths[black] = 100*(db+1);
  printf("\n ***************************************************************************\n");
  printf(" ****************************** %4i vs. %4i ******************************\n", depths[white], depths[black]);
#endif
  init_hash();
  init_board(&b);
  for(i=0; ; ++i) {
    unsigned int bt, et;
    move  m;
    int  val;
    show_board(stdout, &b);
    //for (d=2; d<=6; ++d)
    d=1;
    {
      nodes_evaluated = 0;
      tt_hit = tt_false = tt_deep = tt_shallow = tt_used = 0;
      init_ttable();
      bt = read_clock();
      switch (d) {
      case 0:
        printf(" --- Alpha-beta(tt+id++)(%i) ---\n", depths[b.side]);
        m = search4ttx(&b, depths[b.side], &val);
        break;
      case 1:
        printf(" --- Alpha-beta(tt+id)(%i) ---\n", depths[b.side]);
        m = search4tt(&b, depths[b.side], &val);
        break;
      case 2:
        printf(" --- Alpha-beta(tt)(%i) ---\n", depths[b.side]);
        m = search3tt(&b, depths[b.side], &val);
        break;
      case 3:
        printf(" --- Alpha-beta(id)(%i) ---\n", depths[b.side]);
        m = search4(&b, depths[b.side], &val);
        break;
      case 4:
        printf(" --- Alpha-beta(simple)(%i) ---\n", depths[b.side]);
        m = search3(&b, depths[b.side], &val);
        break;
      case 5:
        printf(" --- Minmax(%i) ---\n", depths[b.side]);
        m = search1(&b, depths[b.side], &val);
        break;
      case 6:
        printf(" --- Negamax(%i) ---\n", depths[b.side]);
        m = search2(&b, depths[b.side], &val);
        break;
      }
      et = read_clock();
      for (n=0; n<depths[b.side]/100; ++n) show_move(stdout, pv[0][n]); printf("\n");
      printf("[%8in, %.4fs : %9.2fnps || tt:hit=%i false=%i deep=%i shallow=%i used=%i] %i:%c",
        nodes_evaluated, (double)(et-bt)*1e-6, 1e6*(double)nodes_evaluated/(double)(et-bt),
        tt_hit, tt_false, tt_deep, tt_shallow, tt_used,
        i, "WBN"[b.side]
        );
      show_move(stdout, m); printf(" {{%i}}\n", val);
      fflush(stdout);
    }
    state s;
    m = make_move(&b, m, &s);
    if (b.side==none) break;
  }
  show_board(stdout, &b);
  printf("\n [[[%i]]]\n", evaluate(&b, 0));

#if 1
  init_hash();
  results[dw][db] = b.xside;
  }
  }
  printf("\n\n ===========================================================================\n");
  printf(" ===========================================================================\n");
  printf(" ===========================================================================\n");
  printf(" ===========================================================================\n");

  printf("           Black\n");
  for (dw=0; dw<MAXW; ++dw) {
    printf("%c (%4i) : ", "White                              "[dw], 100*(dw+1));
    for (db=0; db<MAXB; ++db) {
      printf(" %c ", "WB-"[results[dw][db]]);
    }
    printf("\n");
  }
#endif
  init_hash();

  return 0;
}
