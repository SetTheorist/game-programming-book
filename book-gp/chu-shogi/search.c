/* $Id: search.c,v 1.7 2010/12/26 18:10:28 apollo Exp apollo $ */

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

/* ************************************************************ */

//#define VALIDATE_HASH 1

#define LOGGING 1
//#define LOGGING 0
#ifdef LOGGING
static int log_ind=0;
#define LIN(i_, fmt_, ...) do{ if ((i_)<=LOGGING) {\
  if (logfile()) fstatus(logfile(), "%*s[%i:%s]>>>>" fmt_ "\n", 2*log_ind, "", __LINE__, __func__, ##__VA_ARGS__); \
  ++log_ind; \
}}while(0)
#define LOUT(i_, fmt_, ...) do{ if ((i_)<=LOGGING) {\
  --log_ind; \
  if (logfile()) fstatus(logfile(), "%*s[%i:%s]<--<" fmt_ "\n", 2*log_ind, "", __LINE__, __func__, ##__VA_ARGS__); \
}}while(0)
#define LOG(i_,fmt_, ...) do{ if ((i_)<=LOGGING) {\
  if (logfile()) fstatus(logfile(), "%*s[%i:%s]: " fmt_ "\n", 2*log_ind, "", __LINE__, __func__, ##__VA_ARGS__); \
}}while(0)
#else
#define LIN(i_, fmt_, ...)
#define LOUT(i_, fmt_, ...)
#define LOG(i_, fmt_, ...)
#endif

/* ************************************************************ */

// The alternative (color/piece/to) doesn't seem any better (in fact, a bit worse)
#define HH_TO_FROM

// These don't seem to help at all (in fact make it much worse ...)
// many variations tried...
#define F_QIID    0
#define F_IID     0

//#define CHECK_EXTENSION 75
static const int CHECK_EXTENSION[MAXSEARCHDEPTH] = {
  90, 75, 70, 70, 65, 65, 60, 60,
  60, 50, 45, 40, 35, 30, 25, 20,
  10, 10, 10, 10, 10, 10, 10, 10,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
};

//#define PROMOTION_EXTENSION 50
static const int PROMOTION_EXTENSION[MAXSEARCHDEPTH] = {
  50, 50, 50, 40, 40, 30, 20, 10,
  10, 10, 10, 10, 10, 10, 10, 10,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
};

//#define LION_CHECK_EXTENSION 60
static const int LION_CHECK_EXTENSION[MAXSEARCHDEPTH] = {
  75, 75, 60, 55, 50, 40, 30, 30,
  25, 25, 20, 15, 10, 10, 10, 10,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
};

//#define LMR_REDUCTION 100
#define LMR_REDUCTION 50

//#define LMR_QMOVES  4
#define LMR_QMOVES  10
//#define LMR_MOVES   10
static const int LMR_MOVES[MAXSEARCHDEPTH] = {
 100, 50, 30, 20,  8,  7,  6,  5,
   4,  4,  4,  4,  4,  4,  4,  4,
   4,  4,  4,  4,  4,  4,  4,  4,
   4,  4,  4,  4,  4,  4,  4,  4,
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   2,  2,  2,  2,  2,  2,  2,  2,
   2,  2,  2,  2,  2,  2,  2,  2,
};

/* ************************************************************ */

typedef enum node_type {
  pv_node=0, all_node=-1, cut_node=+1
} node_type;

/* ************************************************************ */


static movet pv[MAXSEARCHDEPTH][MAXSEARCHDEPTH];
static int pv_length[MAXSEARCHDEPTH];
#ifdef  HH_TO_FROM
static ui32 hist_heur[MAXBOARDI][MAXBOARDI];
static ui32 qhist_heur[MAXBOARDI][MAXBOARDI];
#else
static ui32 hist_heur[2][num_piece][MAXBOARDI];
static ui32 qhist_heur[2][num_piece][MAXBOARDI];
#endif
//static movet qkillers[MAXSEARCHDEPTH][2];
static search_settings settings;
static ttentry ttable[TTNUM];
static ttentry qttable[QTTNUM];
static int tt_hit;
static int tt_false;
static int tt_deep;
static int tt_shallow;
static int tt_used;
static int tt_used_exact;
static int tt_used_upper;
static int tt_used_lower;
static int tt_cutoff;

ui64 nodes_searched;
ui64 qnodes_searched;
ui64 nodes_evaluated;
ui64 qnodes_evaluated;

ui64 nodes_evaluated_depth[MAXSEARCHDEPTH+1];
ui64 qnodes_evaluated_depth[MAXSEARCHDEPTH+1];
ui64 nodes_searched_depth[MAXSEARCHDEPTH+1];
ui64 qnodes_searched_depth[MAXSEARCHDEPTH+1];

int max_ply_reached, max_qply_reached;

/* ************************************************************ */

static inline void get_best_move(int i, int n, movet* ml, evalt* scorel);
static inline void update_pv(int ply, movet m);

static evalt search_alphabeta_quiesce(const evaluator* e, board* b, int qdepth, evalt alpha, evalt beta, int ply, node_type type);
static evalt search_alphabeta_general(const evaluator* e, board* b, int depth, evalt alpha, evalt beta, int ply, node_type type);
static evalt search_alphabeta(const evaluator* e, const time_control* tc, board* b, movet* out_pv, int* out_pv_length);

static inline int ttable_retrieve(hasht hash, ttentry* tte);
static inline int ttable_store(hasht hash, evalt score, movet m, int flags, int depth);
static inline int qttable_retrieve(hasht hash, ttentry* tte);
static inline int qttable_store(hasht hash, evalt score, movet m, int flags, int depth);

static inline void history_store(movet m, int depth);
static inline ui32 history_retrieve(movet m);
static inline void qhistory_store(movet m, int depth);
static inline ui32 qhistory_retrieve(movet m);

/* ************************************************************ */

static inline void qhistory_store(movet m, int depth)
{
  if (depth < 100) return;
  if (m.is_special)
  {
    if (m.special.movetype)
    {
      const int to1 = m.special.from + dir_map[m.special.to1];
      const int to2 = to1 + dir_map[m.special.to2];
#ifdef  HH_TO_FROM
      qhist_heur[m.special.from][to1] += 1<<((depth-100)/100)/2;
      qhist_heur[m.special.from][to2] += 1<<((depth-100)/100)/2;
#else
      qhist_heur[m.special.side_is_white][m.special.piece][to1] += 1<<((depth-100)/100)/2;
      qhist_heur[m.special.side_is_white][m.special.piece][to2] += 1<<((depth-100)/100)/2;
#endif
    }
  }
  else
  {
#ifdef  HH_TO_FROM
    qhist_heur[m.from][m.to] += 1<<((depth-100)/100);
#else
    qhist_heur[m.side_is_white][m.piece][m.to] += 1<<((depth-100)/100);
#endif
  }
}

static inline ui32 qhistory_retrieve(movet m)
{
  if (m.is_special)
  {
    if (m.special.movetype)
    {
      const int to1 = m.special.from + dir_map[m.special.to1];
      const int to2 = to1 + dir_map[m.special.to2];
#ifdef  HH_TO_FROM
      return qhist_heur[m.special.from][to1] + qhist_heur[m.special.from][to2];
#else
      return qhist_heur[m.side_is_white][m.piece][to1] + qhist_heur[m.side_is_white][m.piece][to2];
#endif
    }
    else
    {
      return 0;
    }
  }
  else
  {
#ifdef  HH_TO_FROM
    return qhist_heur[m.from][m.to];
#else
    return qhist_heur[m.side_is_white][m.piece][m.to];
#endif
  }
}

static inline void history_store(movet m, int depth)
{
  if (depth < 100) return;
  if (m.is_special)
  {
    if (m.special.movetype)
    {
      const int to1 = m.special.from + dir_map[m.special.to1];
      const int to2 = to1 + dir_map[m.special.to2];
#ifdef  HH_TO_FROM
      hist_heur[m.special.from][to1] += 1<<((depth-100)/100)/2;
      hist_heur[m.special.from][to2] += 1<<((depth-100)/100)/2;
#else
      hist_heur[m.special.side_is_white][m.special.piece][to1] += 1<<((depth-100)/100)/2;
      hist_heur[m.special.side_is_white][m.special.piece][to2] += 1<<((depth-100)/100)/2;
#endif
    }
  }
  else
  {
#ifdef  HH_TO_FROM
    hist_heur[m.from][m.to] += 1<<((depth-100)/100);
#else
    hist_heur[m.side_is_white][m.piece][m.to] += 1<<((depth-100)/100);
#endif
  }
}

static inline ui32 history_retrieve(movet m)
{
  if (m.is_special)
  {
    if (m.special.movetype)
    {
      const int to1 = m.special.from + dir_map[m.special.to1];
      const int to2 = to1 + dir_map[m.special.to2];
#ifdef  HH_TO_FROM
      return hist_heur[m.special.from][to1] + hist_heur[m.special.from][to2];
#else
      return hist_heur[m.side_is_white][m.piece][to1] + hist_heur[m.side_is_white][m.piece][to2];
#endif
    }
    else
    {
      return 0;
    }
  }
  else
  {
#ifdef  HH_TO_FROM
    return hist_heur[m.from][m.to];
#else
    return hist_heur[m.side_is_white][m.piece][m.to];
#endif
  }
}

/* ************************************************************ */

int ttable_init()
{
  memset(ttable, '\x00', sizeof(ttable));
  memset(qttable, '\x00', sizeof(qttable));
  return 0;
}

//#define TTLOC(h)  ((h) % TTNUM)
//#define QTTLOC(h) ((h) % QTTNUM)
#define TTLOC(h)  ((h) & TTMASK)
#define QTTLOC(h) ((h) & QTTMASK)

static inline int ttable_retrieve(hasht hash, ttentry* tte)
{
  if (ttable[TTLOC(hash)].flags == tt_invalid) return 0;
  ++tt_hit;
  if (ttable[TTLOC(hash)].lock != hash) { ++tt_false; return 0; }
  *tte = ttable[TTLOC(hash)];
  return 1;
}

static inline int ttable_store(hasht hash, evalt score, movet m, int flags, int depth)
{
  /* TODO: investigate other replacement schemes */
  /* most-recent replacement scheme: */
  ttable[TTLOC(hash)].lock = hash;
  ttable[TTLOC(hash)].value = score;
  ttable[TTLOC(hash)].best_move = m;
  ttable[TTLOC(hash)].depth = depth;
  ttable[TTLOC(hash)].flags = flags;
  return 0;
}

static inline int qttable_retrieve(hasht hash, ttentry* tte)
{
  if (qttable[QTTLOC(hash)].flags == tt_invalid) return 0;
  if (qttable[QTTLOC(hash)].lock != hash) return 0;
  *tte = qttable[QTTLOC(hash)];
  return 1;
}

static inline int qttable_store(hasht hash, evalt score, movet m, int flags, int depth)
{
  /* TODO: investigate other replacement schemes */
  /* most-recent replacement scheme: */
  qttable[QTTLOC(hash)].lock = hash;
  qttable[QTTLOC(hash)].value = score;
  qttable[QTTLOC(hash)].best_move = m;
  qttable[QTTLOC(hash)].depth = depth;
  qttable[QTTLOC(hash)].flags = flags;
  return 0;
}

/* ************************************************************ */

/* just bring the best score from ml[i]..ml[n-1] to position i (swap it) */
static inline void get_best_move(int i, int n, movet* ml, evalt* scorel)
{
  evalt  best_score = scorel[i];
  int  besti = i, j;
  for (j=i+1; j<n; ++j)
  {
    if (scorel[j]>best_score)
    {
      best_score = scorel[j];
      besti = j;
    }
  }
  if (besti!=i)
  {
    evalt ts = scorel[i];
    movet tm = ml[i];
    scorel[i] = scorel[besti];
    ml[i] = ml[besti];
    scorel[besti] = ts;
    ml[besti] = tm;
  }
}

/* ************************************************************ */

int stop_search;
static ui64 search_start_time;
static ui64 search_stop_time;
static void periodic_check()
{
  const ui64 t = read_clock();
  if (t >= search_stop_time)
  {
    sendlog("*** Enforcing hard stop on time [%'.2fs]***\n", (t - search_start_time)*1e-6);
    stop_search = 1;
  }
}

/* ************************************************************ */

/* move ml[i] to the front without disturbing (relative) order of the rest */
static inline void move_to_front(movet* ml, evalt* scorel, int i)
{
  if (!i) return;
  const movet m = ml[i];
  const evalt v = scorel[i];
  memmove(&ml[1], &ml[0], sizeof(*ml)*i);
  memmove(&scorel[1], &scorel[0], sizeof(*scorel)*i);
  ml[0] = m;
  scorel[0] = v;
}

/* ************************************************************ */

static inline void update_pv(int ply, movet m)
{
  memcpy(&pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(*pv[ply])*pv_length[ply+1]);
  pv[ply][ply] = m;
  pv_length[ply] = pv_length[ply+1]+1;
}

/* ************************************************************ */

static evalt search_alphabeta_quiesce( const evaluator* e, board* b, int qdepth, evalt alpha, evalt beta, int ply, node_type type )
{
  ++qnodes_searched;
  ++qnodes_searched_depth[ply];
  if (ply > max_qply_reached) max_qply_reached = ply;
  pv_length[ply] = 0;

  if (!(qnodes_searched & 0x003FFF)) periodic_check();

  if ((qdepth <= 0) || terminal(b) || (ply >= MAXSEARCHDEPTH-1))
  {
    ++qnodes_evaluated;
    ++qnodes_evaluated_depth[ply];
    return evaluate_relative(e, b, ply, alpha, beta);
  }

  /*
   * TODO: This should probably be eliminated, as it causes
   * loss of ply in a the new-search framework... !?
   */
  /*
  alpha = max(alpha, B_WIN_IN_N(ply));
  beta = min(beta, W_WIN_IN_N(ply+1));
  if (alpha >= beta) return alpha;
  */

  movet tt_goodm; tt_goodm.i = 0;
  int   have_tt_goodm = 0;

  /* transposition table */
  {
    ttentry  tt;
    hasht h = board_hash(b);
    if (qttable_retrieve(h, &tt))
    {
      tt_goodm = tt.best_move;
      have_tt_goodm = tt.flags & tt_have_move;
      if (tt.depth < qdepth)
        goto skip_tt;
      if (tt.flags & tt_exact)
      {
        if (have_tt_goodm && MOVE_IS_VALID(tt.best_move))
        {
          pv[ply][ply] = tt.best_move;
          pv_length[ply] = 1;
        }
        return tt.value;
      }
      else if ((tt.flags & tt_upper) && (type != pv_node))
      {
        if (tt.value <= alpha)
        {
          if (have_tt_goodm && MOVE_IS_VALID(tt.best_move))
          {
            pv[ply][ply] = tt.best_move;
            pv_length[ply] = 1;
          }
          return tt.value;
        }
      }
      else if ((tt.flags & tt_lower) && (type != pv_node))
      {
        if (tt.value >= beta)
        {
          if (have_tt_goodm && MOVE_IS_VALID(tt.best_move))
          {
            pv[ply][ply] = tt.best_move;
            pv_length[ply] = 1;
          }
          return tt.value;
        }
      }
    }
  }
skip_tt: { }

  const int incheck = in_check(b, b->to_move);
  evalt best_val = B_INF;
  int store_tt = 0;
  /* first do "stand-pat" (if not in check) */
  if (!incheck)
  {
    ++qnodes_evaluated;
    ++qnodes_evaluated_depth[ply];
    best_val = evaluate_relative(e, b, ply, alpha, beta);
    if (best_val > alpha)
    {
      alpha = best_val;
      if (alpha >= beta)
        return alpha;
    }
  }

  int   i, n;
  movet ml[MAXNMOVES];
  evalt scorel[MAXNMOVES];

  if (incheck)
    n = gen_moves(b, ml);
  else
    n = gen_moves_cap(b, ml);

  evaluate_moves_for_search(e, b, ml, scorel, n);
  for (i=0; i<n; ++i)
    scorel[i] += 10*qhistory_retrieve(ml[i]);

  if (F_QIID && !have_tt_goodm && (qdepth > 200) && (type==pv_node || type==cut_node))
  {
    /* internal iterative-deepening to find the best move */
    (void)search_alphabeta_quiesce(e, b, min(qdepth-200, qdepth/2), alpha, beta, ply, type);
    if (pv_length[ply])
    {
      have_tt_goodm = 1;
      tt_goodm = pv[ply][ply];
      pv_length[ply] = 0;
    }
  }

  if (have_tt_goodm)
  {
    /* bonus score to transition-table move and move to first */
    for (i=0; i<n; ++i)
    {
      if (tt_goodm.i == ml[i].i)
      {
        if (i)
        {
          const movet m = ml[0];
          ml[0] = ml[i];
          ml[i] = m;
          const evalt s = scorel[0];
          scorel[0] = scorel[i] + 1000000000;
          scorel[i] = s;
        }
        else
        {
          scorel[0]  += 1000000000;
        }
        break;
      }
    }
  }
  else
  {
    get_best_move(0, n, ml, scorel);
  }

  int global_extensions = 0;
  if (CHECK_EXTENSION[ply] && (type == pv_node) && incheck)
    global_extensions += CHECK_EXTENSION[ply];
  else if (LION_CHECK_EXTENSION[ply] && (type == pv_node) && in_check_lion(b, b->to_move))
    global_extensions += LION_CHECK_EXTENSION[ply];

  int num_valid_moves = 0;
  for (i=0; i<n && !stop_search; ++i)
  {
    evalt val;
    if (i) get_best_move(i, n, ml, scorel); /* best move is already at front ... */

    if (make_move(b, ml[i]) == invalid_move) {
      LOG(2, "Invalid move: %m", ml[i]);
      continue;
    }
    ++num_valid_moves;

    int extensions = global_extensions;

    if (PROMOTION_EXTENSION[ply]
        && (type == pv_node)
        && (!ml[i].is_special && ml[i].can_promote)
        )
      extensions += PROMOTION_EXTENSION[ply];

    if (settings.f_lmr
        && !store_tt
        && !extensions
        && (type != pv_node)
        && (num_valid_moves > LMR_QMOVES)
        && (qdepth > (LMR_REDUCTION+100))
        )
      extensions -= LMR_REDUCTION;

    val = -search_alphabeta_quiesce(e, b, (qdepth-100+extensions), -beta, -alpha, ply+1,
        (!store_tt ? (-type) : ((type==cut_node) ? all_node : cut_node)));

    if ((val > best_val) && (extensions < 0))
    {
      /* re-search seemingly good move at full depth... */
      val = -search_alphabeta_quiesce(e, b, (qdepth-100), -beta, -alpha, ply+1,
          (!store_tt ? (-type) : ((type==cut_node) ? all_node : cut_node)));
    }

    /* TODO: put in the pvs search here in q-search... */
    unmake_move(b);

    if (!stop_search)
    {
      if (val > best_val)
        best_val = val;
      if (best_val > alpha)
      {
        store_tt = 1;
        alpha = best_val;
        if (alpha >= beta)
        {
          qhistory_store(ml[i], qdepth);
          qttable_store(board_hash(b), alpha, ml[i], tt_lower|tt_have_move, qdepth);
          pv_length[ply] = 1;
          pv[ply][ply] = ml[i];
          return alpha;
        }
        update_pv(ply, ml[i]);
      }
    }
  }

  if (!stop_search)
  {
    if (store_tt)
    {
      qhistory_store(pv[ply][ply], qdepth);
      qttable_store(board_hash(b), alpha, pv[ply][ply], tt_exact|tt_have_move, qdepth);
    }
    else
    {
      movet m; m.i = 0;
      qttable_store(board_hash(b), best_val, m, tt_upper, qdepth);
    }
  }
  return best_val;
}

/* ************************************************************ */

static evalt search_alphabeta_general( const evaluator* e, board* b, int depth, evalt alpha, evalt beta, int ply, node_type type )
{
  ++nodes_searched;
  ++nodes_searched_depth[ply];
  if (ply > max_ply_reached) max_ply_reached = ply;
  pv_length[ply] = 0;

  if (!(nodes_evaluated & 0x003FFF)) periodic_check();

  if (terminal(b) || (ply >= MAXSEARCHDEPTH-1))
  {
    ++nodes_evaluated;
    ++nodes_evaluated_depth[ply];
    return evaluate_relative(e, b, ply, alpha, beta);
  }

  if (depth <= 0)
    return search_alphabeta_quiesce(e, b, settings.qdepth, alpha, beta, ply, type);

  /*
   * TODO: This should probably be eliminated, as it causes
   * loss of ply in a the new-search framework... !?
   */
  /*
  alpha = max(alpha, B_WIN_IN_N(ply));
  beta = min(beta, W_WIN_IN_N(ply+1));
  if (alpha >= beta) return alpha;
  */

  movet tt_goodm; tt_goodm.i = 0;
  int   have_tt_goodm = 0;

  /* transposition table */
  {
    ttentry  tt;
    hasht h = board_hash(b);
    if (ttable_retrieve(h, &tt))
    {
      tt_goodm = tt.best_move;
      have_tt_goodm = tt.flags & tt_have_move;
      if (tt.depth < depth)
      {
        ++tt_shallow;
        goto skip_tt;
      }
      if (tt.depth > depth) ++tt_deep;
      ++tt_used;
      //if (tt.flags & tt_exact)
      if ((tt.flags & tt_exact) && (type != pv_node))
      {
        ++tt_used_exact;
        if (have_tt_goodm && MOVE_IS_VALID(tt.best_move))
        {
          pv[ply][ply] = tt.best_move;
          pv_length[ply] = 1;
        }
        ++tt_cutoff;
        return tt.value;
      }
      else if ((tt.flags & tt_upper) && (type != pv_node))
      {
        ++tt_used_upper;
        if (tt.value <= alpha) { ++tt_cutoff; return tt.value; }
      }
      else if ((tt.flags & tt_lower) && (type != pv_node))
      {
        ++tt_used_lower;
        if (tt.value >= beta) { ++tt_cutoff; return tt.value; }
      }
    }
  }
skip_tt: { }

  /* null-move-pruning:
   *  - swap sides
   *  - search with reduced depth
   *  - if result is <alpha, then prune
   */
  if (settings.f_nmp && (type != pv_node) && (beta < W_WIN_IN_N(ply+1)))
  {
    int nmr = (depth>settings.nmp_cutoff) ? settings.nmp_R1 : settings.nmp_R2;
    if ((depth > (nmr+100)) && !MOVE_IS_NULL(last_move(b)) && !in_check(b, b->to_move))
    {
      if (make_move(b, null_move()) != invalid_move)
      {
        evalt nmv = -search_alphabeta_general(e, b, depth-100-nmr, -beta, -beta+1, ply+1, ((type == cut_node) ? all_node : cut_node));
        unmake_move(b);
        if (nmv>=beta && !stop_search)
          return beta;
      }
    }
  }

  movet ml[MAXNMOVES];
  evalt scorel[MAXNMOVES];
  int i, n = gen_moves(b, ml);
  if (n==0) return B_WIN_IN_N(ply);

  evaluate_moves_for_search(e, b, ml, scorel, n);
  for (i=0; i<n; ++i)
    scorel[i] += 10*history_retrieve(ml[i]);

  if (have_tt_goodm)
  {
    /* bonus score to transition-table move, and move to front */
    for (i=0; i<n; ++i)
    {
      if (tt_goodm.i == ml[i].i)
      {
        if (i)
        {
          const movet m = ml[0];
          ml[0] = ml[i];
          ml[i] = m;
          const evalt s = scorel[0];
          scorel[0] = scorel[i] + 1000000000;
          scorel[i] = s;
        }
        else
        {
          scorel[0] += 1000000000;
        }
        break;
      }
    }
  }
  else
  {
    if (F_IID && (depth > 200))
    {
      /* internal iterative-deepening to find the best move */
      int i;
      int besti = 0;
      evalt bestval = B_INF;
      for (i=0; i<n; ++i)
      {
        if (make_move(b, ml[i]) == invalid_move) continue;
        evalt val = -search_alphabeta_general(e, b, 0, -beta, -max(bestval,alpha), ply+1, -type);
        unmake_move(b);
        if (val > bestval)
        {
          bestval = val;
          besti = i;
        }
      }
      {
        const movet m = ml[0];
        ml[0] = ml[besti];
        ml[besti] = m;
        const evalt s = scorel[0];
        scorel[0] = scorel[besti] + 1000000000;
        scorel[besti] = s;
      }
    }
    else
    {
      /* move best to front */
      get_best_move(0, n, ml, scorel);
    }
  }

  int global_extensions = 0;
  if (CHECK_EXTENSION[ply] && (type == pv_node) && in_check(b, b->to_move))
    global_extensions += CHECK_EXTENSION[ply];
  else if (LION_CHECK_EXTENSION[ply] && (type == pv_node) && in_check_lion(b, b->to_move))
    global_extensions += LION_CHECK_EXTENSION[ply];

  /* loop over all moves */
  int num_valid_moves = 0;
  int store_tt = 0;
  evalt best_val = B_INF;
  for (i=0; i<n && !stop_search; ++i)
  {
    evalt val;
    if (i) get_best_move(i, n, ml, scorel);
    if (make_move(b, ml[i]) == invalid_move) {
      LOG(2, "Invalid move: %m", ml[i]);
      continue;
    }
    ++num_valid_moves;

    int extensions = global_extensions;

    if (PROMOTION_EXTENSION[ply]
        && (type == pv_node)
        && (!ml[i].is_special && ml[i].can_promote)
        )
      extensions += PROMOTION_EXTENSION[ply];

    if (settings.f_lmr
        && !extensions
        && !store_tt
        && (type != pv_node)
        && (depth > (LMR_REDUCTION+100))
        && (num_valid_moves > LMR_MOVES[ply])
        )
      extensions -= LMR_REDUCTION;

    {
      if (!store_tt)
      {
        val = -search_alphabeta_general(e, b, (depth-100+extensions), -beta, -alpha, ply+1, -type);
        if ((val > best_val) && (extensions < 0))
        {
          /* Re-search full-depth */
          val = -search_alphabeta_general(e, b, (depth-100), -beta, -alpha, ply+1, -type);
        }
      }
      else
      {
        val = -search_alphabeta_general(e, b, (depth-100+extensions), -(alpha+1),  -alpha, ply+1, ((type==cut_node) ? all_node : cut_node));
        if ((val >= (alpha+1)) && (val < beta))
        {
          history_store(ml[i], depth);
          if (extensions < 0)
            extensions = 0;
          val = -search_alphabeta_general(e, b, (depth-100+extensions), -beta, -alpha, ply+1, type);
        }
      }
    }
    unmake_move(b);

    if (!stop_search)
    {
      if (val > best_val)
        best_val = val;
      if (best_val > alpha)
      {
        store_tt = 1;
        alpha = best_val;
        if (alpha >= beta)
        {
          history_store(ml[i], depth+extensions);
          ttable_store(board_hash(b), alpha, ml[i], tt_lower|tt_have_move, depth);
          return alpha;
        }
        update_pv(ply, ml[i]);
      }
    }
  } /* end loop over all moves */

  if (!stop_search)
  {
    /* stalemate is loss */
    if (!num_valid_moves)
      return B_WIN_IN_N(ply);

    if (store_tt)
    {
      ttable_store(board_hash(b), alpha, pv[ply][ply], tt_exact|tt_have_move, depth);
      history_store(pv[ply][ply], (depth+global_extensions));
    }
    else
    {
      movet m; m.i = 0;
      ttable_store(board_hash(b), best_val, m, tt_upper, depth);
    }
  }
  return best_val;
}

/* ************************************************************ */

static int early_abort_on_time(const time_control* tc, ui64 last_iter_time,
    evalt best_val, int iter, evalt evals_by_iter[], int num_last_iter_pv, movet last_iter_pv[])
{
  if (!tc || !num_last_iter_pv || ((int)(last_iter_time/10000) <= tc->allocated_cs))
    return 0;

  const evalt last_iter_eval = evals_by_iter[iter - 1];
  if ((pv[0][0].i == last_iter_pv[0].i) && ((double)(best_val - last_iter_eval)/(double)dmax(abs(last_iter_eval),100) >= -0.10))
  {
    /* if we have used our allocated time but move seems ok still (hasn't dropped by more than 10%), abort early */
    if (logfile())
      fstatus(logfile(), "Early abort on time, unchanged best move [%m], eval [%i was %i]\n",
          pv[0][0], (int)best_val, (int)last_iter_eval);
    return 1;
  }
  else if ((pv[0][0].i == last_iter_pv[0].i) && ((double)(best_val - last_iter_eval)/dmax(abs(last_iter_eval),100) >= -0.25))
  {
    /* if we've exceeded our panic time and move we still have in hand is not _too_ bad, abort early */
    if (logfile())
      fstatus(logfile(), "Early abort on panic time, best move [%m was %m], eval [%i was %i]\n",
          pv[0][0], last_iter_pv[0], (int)best_val, (int)last_iter_eval);
    return 1;
  }
  else
  {
    if (logfile())
      fstatus(logfile(), "No early abort on time, best move [%m was %m], eval [%i was %i]\n",
          pv[0][0], last_iter_pv[0], (int)best_val, (int)last_iter_eval);
    return 0;
  }
}

/* ************************************************************ */

static int another_iteration(const time_control* tc, ui64 last_iter_time, int iter, evalt evals_by_iter[])
{
  if (!tc || iter<=1)
    return 1;

  if (iter >= MAXSEARCHDEPTH)
    return 0;

  int est_time_cs = 5 * last_iter_time / 10000;
  if (logfile())
    fstatus(logfile(), "Estimated time: %.2f (last: %.2f)\n", est_time_cs/100.0, last_iter_time/1e6);

  if (est_time_cs*6 > tc->panic_cs)
    return 0;

  if (est_time_cs > tc->allocated_cs)
  {
    if (iter <= 1)
      return 0;

    /* don't break (yet) if evaluation dropped by a lot */
    if (evals_by_iter[iter-1] >= evals_by_iter[iter-2] - 300)
      return 0;
    if (logfile())
      fstatus(logfile(), "Extending into panic time on eval drop: %i --> %i\n",
          (int)evals_by_iter[iter-2], (int)evals_by_iter[iter-1]);
  }

  return 1;
}

/* ************************************************************ */

static evalt search_alphabeta( const evaluator* e, const time_control* tc, board* b, movet* out_pv, int* out_pv_length )
{
  LIN(1, "search_alphabeta()");
  int   i;

  stop_search = 0;

  pv_length[0] = 0;
  tt_hit = tt_false = tt_deep = tt_shallow = tt_used = tt_cutoff
      = tt_used_exact = tt_used_upper = tt_used_lower = 0;
  nodes_searched = 1;
  qnodes_searched = 0;
  nodes_evaluated = 0;
  qnodes_evaluated = 0;

  /* TODO: preserve between calls? */
  memset(hist_heur, 0, sizeof(hist_heur));
  memset(qhist_heur, 0, sizeof(qhist_heur));

  /* generate moves */
  movet ml[MAXNMOVES];
  int n = gen_moves(b, ml);

  /* prune invalid moves */
  for (i=0; i<n; ++i) {
    if (make_move(b, ml[i]) == invalid_move) {
      LOG(0, "Pruning invalid move: %m", ml[i]);
      ml[i--] = ml[n-- -1];
    } else {
      unmake_move(b);
    }
  }

  /* stalemate is loss */
  if (n==0)
    return B_WIN;

  /* evaluate and sort moves - could use more efficient sort, but this is done only once per search-call... */
  evalt scorel[MAXNMOVES];
  evaluate_moves_for_search(e, b, ml, scorel, n);
  for (i=0; i<n; ++i) get_best_move(i, n, ml, scorel);
#ifdef LOGGING
  if (logfile()) {
    fprintf(logfile(), "Sorted moves (pre-tt):\n");
    for (i=0; i<n; ++i) {
      fstatus(logfile(), "\t%m/%i", ml[i], (int)scorel[i]);
      if ((i%5)==4 || i==n-1) fprintf(logfile(), "\n");
    }
  }
#endif

  /* try transposition table move first, if we've got one */
  {
    ttentry  tt;
    hasht h = board_hash(b);
    if (ttable_retrieve(h, &tt)) {
      if (tt.flags & tt_have_move) {
        for (i=0; i<n; ++i) {
          if (ml[i].i == tt.best_move.i)
          {
            move_to_front(ml, scorel, i);
            break;
          }
        }
#ifdef LOGGING
        if (logfile()) {
          fprintf(logfile(), "Sorted moves (post-tt):\n");
          for (i=0; i<n; ++i) {
            fstatus(logfile(), "\t%m/%i", ml[i], (int)scorel[i]);
            if ((i%5)==4 || i==n-1) fprintf(logfile(), "\n");
          }
        }
#endif
      }
    }
  }

  const evalt static_eval = evaluate_relative(e, b, 0, B_INF, W_INF);

  ui64 last_iter_time = 0;
  int   num_last_iter_pv = 0;
  movet last_iter_pv[MAXSEARCHDEPTH];
  int   iter = 1;
  evalt evals_by_iter[MAXSEARCHDEPTH] = {static_eval};

  evalt bestv = B_INF;

  /* iterative deepening */
  int d;
  const int start_depth = settings.f_id ? settings.id_base : settings.depth;
  const int depth_increment = settings.f_id ? settings.id_step : 100;
  ui64 bt = read_clock();
  search_start_time = bt;
  search_stop_time = bt + (tc ? (tc->force_stop_cs * 10000LL) : 1000000000000000LL);
  int num_best_moves = 0;
  for (d=start_depth; d<=settings.depth; d+=depth_increment, ++iter)
  {
    /* can't get shorter mate by searching deeper... */
    if ((W_WIN_IN_N(d/100) < evals_by_iter[iter-1]) || (B_WIN_IN_N(d/100) > evals_by_iter[iter-1]))
      break;

    /* do another iteration? */
    if (!another_iteration(tc, last_iter_time, iter, evals_by_iter))
      break;

    /* shuffle previous iteration's good moves to the front */
    if (num_best_moves)
    {
      int i, j;
      for (i=1; i<=num_best_moves; ++i)
      {
        for (j=0; j<n; ++j)
        {
          if (scorel[j] == i)
          {
            move_to_front(ml, scorel, j);
            break;
          }
        }
      }
      num_best_moves = 0;
    }
    memset(scorel, '\x00', sizeof(scorel));

#ifdef LOGGING
    if (LOGGING>=1 && logfile())
    {
      fprintf(logfile(), "ID: DEPTH = %i\n# \t", d);
      for (i=0; i<n; ++i)
      {
        fstatus(logfile(), "\t%m(%.2f)H:%u", ml[i], scorel[i], history_retrieve(ml[i]));
        if (!((i+1)%5) || i==n-1) fprintf(logfile(), "\n");
      }
    }
#endif

    max_ply_reached = 0;
    max_qply_reached = 0;

    memset(nodes_searched_depth, '\x00', sizeof(nodes_searched_depth));
    memset(qnodes_searched_depth, '\x00', sizeof(qnodes_searched_depth));
    memset(nodes_evaluated_depth, '\x00', sizeof(nodes_evaluated_depth));
    memset(qnodes_evaluated_depth, '\x00', sizeof(qnodes_evaluated_depth));

    /* Loop over all moves */
    bestv = B_INF;
    pv_length[0] = 0;
    for (i=0; i<n && !stop_search; ++i)
    {
#ifdef  LOGGING
      const ui64 pre_nodes = (nodes_searched+qnodes_searched);
#endif

      evalt v;
      if (make_move(b, ml[i]) == invalid_move)
      {
        /* this shouldn't happen! - move should've been filtered out already*/
        if (logfile())
          fstatus(logfile(), "Unexpected invalid_move %m\n", ml[i]);
        if (i!=n-1) {
          memmove(&ml[i], &ml[i+1], sizeof(*ml)*(n-i-1));
          memmove(&scorel[i], &scorel[i+1], sizeof(*scorel)*(n-i-1));
        }
        --i;
        --n;
        continue;
      }

      if (i==0)
      {
        /* try aspiration window around previous iteration's result (or static eval for first iter),
         * make two attempts at small windows before maximizing side
         */
        evalt upper_bounds[3] = {
          evals_by_iter[iter-1] + settings.asp_width,
          evals_by_iter[iter-1] + 4*settings.asp_width,
          W_INF
        };
        evalt lower_bounds[3] = {
          evals_by_iter[iter-1] - settings.asp_width,
          evals_by_iter[iter-1] - 4*settings.asp_width,
          B_INF
        };
        int upper_level = 0;
        int lower_level = 0;
        evalt upper_bound, lower_bound;
        do
        {
          upper_bound = upper_bounds[upper_level];
          lower_bound = lower_bounds[lower_level];
          v = -search_alphabeta_general(e, b, d-100, -upper_bound, -lower_bound, 1, pv_node);
#ifdef  LOGGING
          const ui64 post_nodes=(nodes_searched+qnodes_searched);
          LOG(1, "ml[%i]=%m window=(%i,%i) --> %i  [%'llu]", i, ml[i], (int)lower_bound, (int)upper_bound, (int)v,
              (post_nodes - pre_nodes));
#endif
          if (v <= lower_bound)
            ++lower_level;
          else if (v >= upper_bound)
            ++upper_level;
        } while (((v <= lower_bound) || (v >= upper_bound)) && (lower_level<3 && upper_level<3)); // catch some "impossible" situation caused by a bug
      }
      else
      {
        /* try minimal window search on best val so-far */
        v = -search_alphabeta_general(e, b, d-100, -(bestv+1),  -bestv, 1, cut_node);
        if (v >= bestv + 1)
        {
          LOG(1, "ml[%i]=%m   MWS re-search (got %i)", i, ml[i], (int)v);
          v = -search_alphabeta_general(e, b, d-100, -W_INF, -bestv, 1, pv_node);
        }
      }
      unmake_move(b);
      ui64  et = read_clock();
      last_iter_time = et - bt;

      const ui64 post_nodes=(nodes_searched+qnodes_searched);

#ifdef LOGGING
      LOG(1, "ml[%i]=%m  %i  [%'llu]", i, ml[i], (int)v, (post_nodes - pre_nodes));
#endif
      if (v>bestv && !stop_search)
      {
        LOG(1, "New best move: %m (#%i)", ml[i], i);
        bestv = v;
        update_pv(0, ml[i]);
        history_store(ml[i], d);
        scorel[i] = ++num_best_moves;

        {
          /* N.B. we scale the eval down to match hachu more closely */
          evalt wb_eval = ((bestv > W_WIN_IN_N(MAXSEARCHDEPTH))||(bestv < B_WIN_IN_N(MAXSEARCHDEPTH))) ? bestv : bestv/3;
          send("%i %i %i %llu [%i/%i/%i] ",
              d/100, (int)wb_eval, (int)(last_iter_time/10000), post_nodes,
              d/100, max_ply_reached, max_qply_reached);
          if ((bestv > W_WIN_IN_N(MAXSEARCHDEPTH))||(bestv < B_WIN_IN_N(MAXSEARCHDEPTH)))
            send("(Mate-in-%i) ", (int)PLIES_TO_WIN(bestv));
          if (pv_length[0])
          {
            fstatus(stdout, "%Q\n", pv[0], pv_length[0]);
            if (in_winboard && logfile())
              fstatus(logfile(), "%Q\n", pv[0], pv_length[0]);
          }
          else
          {
            send("[]\n");
          }
        }

        if (early_abort_on_time(tc, last_iter_time, bestv, iter, evals_by_iter, num_last_iter_pv, last_iter_pv))
          break;
      }
    } /* end loop over all moves */

    ui64  et = read_clock();
    {
      if (logfile())
      {
        fstatus(logfile(), "ID(%i) eval=%i time=%.4fs PV=[%M]",
            d, (int)bestv, (et-bt)*1e-6, pv[0], pv_length[0]);
        fprintf(logfile(), "  (%'lluns/%'lluqns %'llune/%'lluqne)\n",
            nodes_searched, qnodes_searched, nodes_evaluated, qnodes_evaluated);
        const int maxd = max(max_ply_reached, max_qply_reached);
        for (i=0; i<=maxd; ++i)
          fprintf(logfile(), "\t[%2i]  %'12lluns %'12lluqns  %'12llune %'12lluqne\n",
              i, nodes_searched_depth[i], qnodes_searched_depth[i], nodes_evaluated_depth[i], qnodes_evaluated_depth[i]);
      }

      if (pv_length[0])
      {
        num_last_iter_pv = pv_length[0];
        memcpy(last_iter_pv, pv[0], pv_length[0]*sizeof(movet));
        evals_by_iter[iter] = bestv;
      }
      else
      {
        evals_by_iter[iter] = evals_by_iter[iter-1];
      }
    }

    {
      /* N.B. we scale the eval down to match hachu more closely */
      const evalt best = evals_by_iter[iter];
      const evalt eval = ((best > W_WIN_IN_N(MAXSEARCHDEPTH))||(best < B_WIN_IN_N(MAXSEARCHDEPTH))) ? best : best/3;
      send("%i %i %i %llu [%i/%i/%i] ",
          d/100, (int)eval, (int)((et-bt)/10000), (nodes_searched+qnodes_searched),
          d/100, max_ply_reached, max_qply_reached);
      if ((best > W_WIN_IN_N(MAXSEARCHDEPTH))||(best < B_WIN_IN_N(MAXSEARCHDEPTH)))
        send("(Mate-in-%i) ", (int)PLIES_TO_WIN(best));
      if (pv_length[0])
      {
        fstatus(stdout, "%Q\n", last_iter_pv, num_last_iter_pv);
        if (in_winboard && logfile())
          fstatus(logfile(), "%Q\n", last_iter_pv, num_last_iter_pv);
      }
      else
      {
        send(" []\n");
      }
    }
  }
#ifdef LOGGING
  if (1<=LOGGING && logfile())
  {
    fprintf(logfile(), "ID: **********\n# \t");
    for (i=0; i<n; ++i)
    {
      fstatus(logfile(), "\t%m(%i)  ", ml[i], (int)scorel[i]);
      if (!((i+1)%5) || i==n-1) fprintf(logfile(), "\n");
    }
    fprintf(logfile(), "\n");
  }
#endif

  *out_pv_length = num_last_iter_pv;
  memcpy(out_pv, last_iter_pv, sizeof(movet)*num_last_iter_pv);
  LOUT(1, " eval=%i pv=[ %M ]", (int)evals_by_iter[iter], out_pv, *out_pv_length);
  return bestv;
}

/* ************************************************************ */

evalt search(board* b, const search_settings* s, const evaluator* e, const time_control* tc, movet* out_pv, int* out_pv_length)
{
  settings = *s;
  return search_alphabeta(e, tc, b, out_pv, out_pv_length);
}

/* ************************************************************ */

int show_settings(FILE* f, const search_settings* ss)
{
  fprintf(f, "{");
  fprintf(f, "AB:");
  fprintf(f, "D(%i)", ss->depth);
  fprintf(f, "Q(%i)", ss->qdepth);
  if (ss->f_id) fprintf(f, ",ID(%i+%i)", ss->id_base, ss->id_step);
  if (ss->f_iid) fprintf(f, ",IID(%i+%i)", ss->iid_base, ss->iid_step);
  fprintf(f, ",ASP(%i)", ss->asp_width);
  fprintf(f, ",HH");
  fprintf(f, ",QHH");
  fprintf(f, ",TT");
  fprintf(f, ",QTT");
  if (ss->f_lmr) fprintf(f, ",LMR");
  if (ss->f_futility) fprintf(f, ",FUT");
  if (ss->f_extfutility) fprintf(f, ",EXTFUT");
  if (ss->f_razoring) fprintf(f, ",RAZ");
  if (ss->f_limrazoring) fprintf(f, ",LIMRAZ");
  if (ss->f_nmp) fprintf(f, ",NMP(%i/%i[%i])", ss->nmp_R1, ss->nmp_R2, ss->nmp_cutoff);
  fprintf(f, "}");
  return 0;
}

void make_simple_alphabeta_searcher(search_settings* ss, int depth)
{
  memset(ss, '\x00', sizeof(search_settings));
  ss->search_type = alphabeta_search;
  ss->depth = depth;
}

/* ************************************************************ */
