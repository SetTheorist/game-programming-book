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

/* ************************************************************ */

#define VALIDATE_HASH 1

#define LOGGING 0
#if LOGGING
static int log_ind=0;
#define LIN(i_, fmt_, ...) do{ if ((i_)<=LOGGING) {\
  fstatus(stderr, "%*s[%i:%s]>>>>" fmt_ "\n", 2*log_ind, "", __LINE__, __func__, ##__VA_ARGS__); \
  ++log_ind; \
}}while(0)
#define LOUT(i_, fmt_, ...) do{ if ((i_)<=LOGGING) {\
  --log_ind; \
  fstatus(stderr, "%*s[%i:%s]<--<" fmt_ "\n", 2*log_ind, "", __LINE__, __func__, ##__VA_ARGS__); \
}}while(0)
#define LOG(i_,fmt_, ...) do{ if ((i_)<=LOGGING) {\
  fstatus(stderr, "%*s[%i:%s]: " fmt_ "\n", 2*log_ind, "", __LINE__, __func__, ##__VA_ARGS__); \
}}while(0)
#else
#define LIN(i_, fmt_, ...)
#define LOUT(i_, fmt_, ...)
#define LOG(i_, fmt_, ...)
#endif


/* ************************************************************ */

static movet pv[MAXSEARCHDEPTH][MAXSEARCHDEPTH];
static int pv_length[MAXSEARCHDEPTH];
static int hist_heur[MAXBOARDI][MAXBOARDI];
static search_settings settings;
static ttentry ttable[TTNUM];
int tt_hit;
int tt_false;
int tt_deep;
int tt_shallow;
int tt_used;
int tt_used_exact;
int tt_used_upper;
int tt_used_lower;
int tt_cutoff;
int nodes_searched;

/* ************************************************************ */

static inline void get_best_move(int i, int n, movet* ml, evalt* scorel);
static void update_pv(int ply, movet m);

static evalt search_alphabeta_quiesce(
    const evaluator* e, board* b, int qdepth, evalt alpha, evalt beta, int ply);
static evalt search_alphabeta_general(
    const evaluator* e, board* b, int depth, evalt alpha, evalt beta, int ply);
static evalt search_alphabeta(
    const evaluator* e, board* b, movet* out_pv, int* out_pv_length);

static int ttable_retrieve(hasht hash, ttentry* tte);
static int ttable_store(hasht hash, evalt score, movet m, int flags, int depth);

/* ************************************************************ */
int ttable_init() {
  memset(ttable, 0, sizeof(ttable));
  return 0;
}
static int ttable_retrieve(hasht hash, ttentry* tte) {
  //printf("<?:%016llX=%02llX>", hash, hash%TTNUM);
  if (ttable[hash % TTNUM].flags == tt_invalid) return 0;
  //printf("!");
  ++tt_hit;
  if (ttable[hash % TTNUM].lock != hash) { ++tt_false; return 0; }
  //printf("!");
  *tte = ttable[hash % TTNUM];
  return 1;
}
static int ttable_store(hasht hash, evalt score, movet m, int flags, int depth) {
  //printf("<!:%016llX=%02llX>\n", hash, hash%TTNUM);
  /* TODO: investigate other replacement schemes */
  /* most-recent replacement scheme: */
  ttable[hash % TTNUM].lock = hash;
  ttable[hash % TTNUM].value = score;
  ttable[hash % TTNUM].best_move = m;
  ttable[hash % TTNUM].depth = depth;
  ttable[hash % TTNUM].flags = flags;
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

static void update_pv(int ply, movet m)
{
  memcpy(&pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(movet)*pv_length[ply+1]);
  pv[ply][ply] = m;
  pv_length[ply] = pv_length[ply+1]+1;
}

/* ************************************************************ */

static evalt search_alphabeta_quiesce( const evaluator* e, board* b, int qdepth, evalt alpha, evalt beta, int ply )
{
  ++nodes_searched;
  pv_length[ply] = 0;

  if ((qdepth<=0) || terminal(b))
    return evaluate_relative(e, b, ply);

  movet ml[MAXNCAPMOVES];
  evalt scorel[MAXNCAPMOVES];
  evalt val;
  int   n, i;

  /* first do "stand-pat" */
  val = evaluate_relative(e, b, ply);
  if (val>alpha)
  {
    alpha = val;
    if (alpha>=beta)
      return alpha;
  }

  n = gen_moves_cap(b, ml);
  evaluate_moves_for_quiescence(e, b, ml, scorel, n);

  for (i=0; i<n; ++i)
  {
    get_best_move(i, n, ml, scorel);
    int mm = make_move(b, ml[i]);
    if (mm == invalid_move) continue;
    evalt val = (mm==same_side_moves_again)
              ?  search_alphabeta_quiesce(e, b, qdepth-100, alpha,   beta, ply+1)
              : -search_alphabeta_quiesce(e, b, qdepth-100, -beta, -alpha, ply+1);
    unmake_move(b);
    if (val>alpha)
    {
      alpha = val;
      if (alpha>=beta) break;
      update_pv(ply, ml[i]);
    }
  }
  return alpha;
}

static evalt search_alphabeta_general( const evaluator* e, board* b, int depth, evalt alpha, evalt beta, int ply )
{
  ++nodes_searched;

  if (terminal(b))
  {
    pv_length[ply] = 0;
    return evaluate_relative(e, b, ply);
  }

  if (depth<=0)
  {
    pv_length[ply] = 0;
    if (settings.f_quiesce)
      return search_alphabeta_quiesce(e, b, settings.qdepth, alpha, beta, ply);
    else
      return evaluate_relative(e, b, ply);
  }

  movet tt_goodm;
  int   have_tt_goodm = 0;
  int   n, i;

  /* transition table */
  if (settings.f_tt)
  {
    ttentry  tt;
    hasht h = board_hash(b);
    if (ttable_retrieve(h, &tt))
    {
      if (MOVE_IS_VALID(tt.best_move))
      {
        have_tt_goodm = 1;
        tt_goodm = tt.best_move;
      }
      if (tt.depth < depth)
      {
        ++tt_shallow;
        goto skip_tt;
      }
      if (tt.depth > depth) ++tt_deep;
      ++tt_used;
      switch (tt.flags&tt_exact)
      {
        case tt_exact:
          ++tt_used_exact;
          if (MOVE_IS_VALID(tt.best_move))
          {
            pv[ply][ply] = tt.best_move;
            pv_length[ply] = 1;
          }
          else
          {
            pv_length[ply] = 0;
          }
          ++tt_cutoff;
          return tt.value;
        case tt_upper:
          ++tt_used_upper;
          /*
          if (tt.value < beta) beta = tt.value;
          if (alpha >= beta) { ++tt_cutoff; return beta; }
          */
          if (tt.value <= alpha) { ++tt_cutoff; return tt.value; }
          break;
        case tt_lower:
          ++tt_used_lower;
          /*
          if (tt.value > alpha) alpha = tt.value;
          if (alpha >= beta) { ++tt_cutoff; return alpha; }
          */
          if (tt.value >= beta) { ++tt_cutoff; return tt.value; }
          break;
        default:
          break;
      }
    }
  }
skip_tt: {}

  movet ml[MAXNMOVES];
  evalt scorel[MAXNMOVES];

  n = gen_moves(b, ml);
  if (settings.f_hh)
  {
    /* use history_heuristic table for score initialization */
    for (i=0; i<n; ++i)
      scorel[i] = hist_heur[MOVE_FROM_I(ml[i])][MOVE_TO_I(ml[i])];
    /* TODO: generalize this! */
    /* bonus capture value */
    //for (i=0; i<n; ++i) if (ml[i].flags&cap) scorel[i] += 20;
  }
  else
  {
    evaluate_moves_for_search(e, b, ml, scorel, n);
  }
  if (have_tt_goodm)
  {
    /* TODO: */
    /* bonus score to transition-table move so it's used first */
    for (i=0; i<n; ++i) {
      if (*((int*)&tt_goodm)==*((int*)&(ml[i]))) {
        scorel[i] += 1000000;
        break;
      }
    }
  }
  /* null-move-pruning:
   *  - swap sides
   *  - search with reduced depth
   *  - if result is <alpha, then prune
   */
  if (settings.f_nmp)
  {
    const movet  nm = null_move();
    int nmr = (depth>settings.nmp_cutoff) ? settings.nmp_R1 : settings.nmp_R2;
    if (!MOVE_IS_NULL(last_move(b)) && (depth>(nmr+100)))
    {
      int mm = make_move(b,nm);
      if (mm != invalid_move)
      {
        evalt nmv = (mm == same_side_moves_again)
                  ?  search_alphabeta_general(e, b, depth-100-nmr, alpha,   beta, ply+1)
                  : -search_alphabeta_general(e, b, depth-100-nmr, -beta, -alpha, ply+1);
        unmake_move(b);
        if (nmv>=beta)
          return beta;
      }
    }
  }
  /* minimal-window search:
   *  after first move with full alpha-beta window,
   *  try minimal windows for following searches */
  if (settings.f_mws)
  {
    for (i=0; i<n; ++i)
    {
      evalt val;
      get_best_move(i, n, ml, scorel);
      int mm = make_move(b, ml[i]);
      if (mm == invalid_move) continue;
      if (i==0)
      {
        val = (mm == same_side_moves_again)
            ?  search_alphabeta_general(e, b, depth-100, alpha,   beta, ply+1)
            : -search_alphabeta_general(e, b, depth-100, -beta, -alpha, ply+1);
      }
      else
      {
        val = (mm == same_side_moves_again)
            ? -search_alphabeta_general(e, b, depth-100,      alpha, alpha+1, ply+1)
            : -search_alphabeta_general(e, b, depth-100, -(alpha+1),  -alpha, ply+1);
        /* fail-high means we have to try again with full window */
        if (val>alpha)
        {
          val = (mm == same_side_moves_again)
              ?  search_alphabeta_general(e, b, depth-100, alpha, beta, ply+1)
              : -search_alphabeta_general(e, b, depth-100, -beta, -alpha, ply+1);
        }
      }
      unmake_move(b);
      if (val>alpha)
      {
        alpha = val;
        if (alpha>=beta)
        {
          /* history-heuristic: increment cutoff move */
          if (settings.f_hh)
            hist_heur[MOVE_FROM_I(ml[i])][MOVE_TO_I(ml[i])] += 1<<((depth-100)/100);
          break;
        }
        update_pv(ply, ml[i]);
      }
    }
  }
  else
  {
    int idepth;
    /* straight-forward alpha-beta and maybe internal-iterative-deepening */
    for (idepth=(settings.f_iid&&(depth>=500)) ? (depth-400) : depth;
         idepth <= depth;
         idepth += (settings.f_iid&&(depth>=500)) ? 200 : 100)
    {
      for (i=0; i<n; ++i)
      {
        get_best_move(i, n, ml, scorel);
        int mm = make_move(b, ml[i]);
        if (mm == invalid_move) continue;
        evalt val = (mm == same_side_moves_again)
                  ?  search_alphabeta_general(e, b, idepth-100, alpha,   beta, ply+1)
                  : -search_alphabeta_general(e, b, idepth-100, -beta, -alpha, ply+1);
        unmake_move(b);
        if (settings.f_iid) scorel[i] = val;
        if (val>alpha)
        {
          alpha = val;
          if (alpha>=beta)
          {
            /* increment cutoff move */
            if (settings.f_hh)
              hist_heur[MOVE_FROM_I(ml[i])][MOVE_TO_I(ml[i])] += 1<<((idepth-100)/100);
            break;
          }
          update_pv(ply, ml[i]);
        }
      }
    }
  }
  /* TODO: fix tt_upper/lower/exact flags! */
  if (settings.f_tt)
  {
    if (alpha<beta)
    {
      ttable_store(board_hash(b), alpha, pv[ply][ply], tt_exact, depth);
      /* increment best move */
      if (settings.f_hh)
        hist_heur[MOVE_FROM_I(pv[ply][ply])][MOVE_TO_I(pv[ply][ply])] += 1<<((depth-100)/100);
    }
  }
  return alpha;
}

static evalt search_alphabeta( const evaluator* e, board* b, movet* out_pv, int* out_pv_length )
{
  LIN(2, "");
  movet ml[MAXNMOVES];
  evalt scorel[MAXNMOVES];
  movet old_ml[MAXNMOVES];
  evalt old_scorel[MAXNMOVES];
  evalt lower_bound=B_INF, upper_bound;
  int   i, n, d;
  tt_hit = tt_false = tt_deep = tt_shallow = tt_used = tt_cutoff
      = tt_used_exact = tt_used_upper = tt_used_lower = 0;
  nodes_searched = 1;
  n = gen_moves(b, ml);
  if (n==0)
  {
    /* TODO: stalemate is loss (?!) */
    *pv_length = 0;
    LOUT(2, "");
    return B_INF;
  }
  evaluate_moves_for_search(e, b, ml, scorel, n);

  if (settings.f_hh)
    memset(hist_heur, 0, sizeof(hist_heur));

  /* iterative deepening */
  for (d=settings.f_id ? settings.id_base : settings.depth;
       d<=settings.depth;
       d+=settings.f_id ? settings.id_step : 100)
  {
    LOG(2,"ID");
    int doing_asp=0;
    evalt asp_midv=0, asp_low=0, asp_high=0;
    if (settings.f_asp && settings.f_id && (d>settings.id_base))
    {
      /* aspiration search: after first iteration
       * we try a small window around previous iteration's result */
      LOG(2,"aspiration search");
      doing_asp = 1;
      asp_midv = lower_bound;
      lower_bound = asp_low = asp_midv - settings.asp_width;
      upper_bound = asp_high = asp_midv + settings.asp_width;
      memcpy(old_scorel, scorel, sizeof(scorel));
      memcpy(old_ml, ml, sizeof(ml));
    }
    else
    {
      doing_asp = 0;
      lower_bound = B_INF;
      upper_bound = W_INF;
    }
    for (i=0; i<n; ++i)
    {
      LOG(2, "ml[i]=%m", ml[i]);
      get_best_move(i, n, ml, scorel);
      int mm = make_move(b, ml[i]);
      if (mm == invalid_move) continue;
      evalt v = (mm == same_side_moves_again)
              ?  search_alphabeta_general(e, b, d-100,  lower_bound,  upper_bound, 1)
              : -search_alphabeta_general(e, b, d-100, -upper_bound, -lower_bound, 1);
      unmake_move(b);
      if (v>lower_bound)
      {
        lower_bound = v;
        update_pv(0, ml[i]);
      }
      /* save value for sorting next iteration */
      scorel[i] = v;

      if (doing_asp && ((lower_bound>=asp_high) || (lower_bound<=asp_low)))
      {
        /* fell outside aspiration window, restart with full window
         * TODO: (actually: we can use the results up to this failing move...)
         * (and shouldn't we reorder this troublesome move earlier?)
         */
        LOG(2,"aspiration restart");
        lower_bound = B_INF;
        upper_bound = W_INF;
        memcpy(scorel, old_scorel, sizeof(scorel));
        memcpy(ml, old_ml, sizeof(ml));
        i = -1;
        doing_asp = 0;
      }
      //status("<%m %i>", ml[i], scorel[i]);
    }
    if (settings.f_id)
      fstatus(stdout, "ID(%i) eval=%f PV=[%M]\n", d, (double)lower_bound, pv[0], pv_length[0]);
  }

  *out_pv_length = pv_length[0];
  memcpy(out_pv, pv[0], sizeof(movet)*pv_length[0]);
  LOUT(2, " eval=%f pv=[%M]", (double)lower_bound, out_pv, *out_pv_length);
  return lower_bound;
}

/* ************************************************************ */

static evalt search_negamax_general(
    const evaluator* e, board* b, int depth, int ply) {
  LIN(3, "depth=%i", depth);
  ++nodes_searched;
  if (terminal(b) || (depth<=0)) {
    pv_length[ply] = 0;
    LOUT(3, "");
    return evaluate_relative(e, b, ply);
  }
  movet ml[MAXNMOVES];
  evalt best_v = B_INF;
  int   n, i;
  /* transition table */
  if (settings.f_tt) {
    ttentry  tt;
    hasht h = board_hash(b);
    if (ttable_retrieve(h, &tt)) {
      if (tt.depth < depth) {
        ++tt_shallow;
        goto skip_tt;
      }
      if (tt.depth > depth) {
        ++tt_deep;
        //goto skip_tt;
      }
      ++tt_used;
      pv[ply][ply] = tt.best_move;
      pv_length[ply] = 1;
      LOUT(3, " TT=%i [%m]", tt.value, tt.best_move);
      return tt.value;
    }
  }
skip_tt:
  n = gen_moves(b, ml);
  for (i=0; i<n; ++i) {
    LOG(3, "ml[i]=%m", ml[i]);
#if VALIDATE_HASH
    hasht h_pre = board_hash(b);
#endif
    int mm = make_move(b, ml[i]);
    if (mm == invalid_move) continue;
    evalt val = (mm == same_side_moves_again)
              ?  search_negamax_general(e, b, depth-100, ply+1)
              : -search_negamax_general(e, b, depth-100, ply+1);
    unmake_move(b);
#if VALIDATE_HASH
    hasht h_post = board_hash(b);
    if (h_pre!=h_post) {
      fprintf(stderr, "((%16llX!=%16llX {", h_pre, h_post);
      show_move(stderr, ml[i]);
      fprintf(stderr, "} ))");
    }
#endif
    if (val>best_v) {
      best_v = val;
      update_pv(ply, ml[i]);
    }
  }
  if (settings.f_tt)
    ttable_store(board_hash(b), best_v, pv[ply][ply], tt_exact, depth);
  LOUT(3, "");
  return best_v;
}

static evalt search_negamax(
    const evaluator* e, board* b, movet* out_pv, int* out_pv_length) {
  LIN(2, "");
  movet ml[MAXNMOVES];
  evalt best_val=B_INF;
  int   i, n, made_valid_move = 0;
  tt_hit = tt_false = tt_deep = tt_shallow = tt_used = tt_cutoff
      = tt_used_exact = tt_used_upper = tt_used_lower = 0;
  nodes_searched = 1;
  n = gen_moves(b, ml);
  if (n==0) {
    /* TODO: stalemate is loss (?!) */
    *pv_length = 0;
    LOUT(2, "");
    return B_INF;
  }
  for (i=0; i<n; ++i) {
    LOG(2, "ml[i]=%m", ml[i]);
    int mm = make_move(b, ml[i]);
    if (mm == invalid_move) continue;
    made_valid_move = 1;
    evalt v = (mm == same_side_moves_again)
            ?  search_negamax_general(e, b, settings.depth-100, 1)
            : -search_negamax_general(e, b, settings.depth-100, 1);
    unmake_move(b);
    if (v>best_val) {
      best_val = v;
      update_pv(0, ml[i]);
    }
  }
  if (!made_valid_move) {
    /* TODO: stalemate is loss (?!) */
    *pv_length = 0;
    LOUT(2, "");
    return B_INF;
  }
  *out_pv_length = pv_length[0];
  memcpy(out_pv, pv[0], sizeof(movet)*pv_length[0]);
  LOUT(2, " eval=%i pv=[%M]", best_val, out_pv, *out_pv_length);
  return best_val;
}

/* ************************************************************ */

evalt search(board* b, const search_settings* s, const evaluator* e,
             movet* out_pv, int* out_pv_length) {
  settings = *s;
  switch (s->search_type) {
    case minimax_search:
      //return search_minimax(e, b, out_pv, out_pv_length);
    case negamax_search:
      return search_negamax(e, b, out_pv, out_pv_length);
    case alphabeta_search:
      return search_alphabeta(e, b, out_pv, out_pv_length);
    case monte_carlo_naive_search:
    case monte_carlo_uct_search:
    default:
      *pv_length = 0;
      return (evalt)0;
  }
}

/* ************************************************************ */
int show_settings(FILE* f, const search_settings* ss) {
  fprintf(f, "{");
  switch(ss->search_type) {
    case minimax_search: fprintf(f, "Minimax:"); break;
    case negamax_search: fprintf(f, "Negamax:"); break;
    case alphabeta_search: fprintf(f, "Alphabeta:"); break;
    case monte_carlo_naive_search: fprintf(f, "MonteCarlo(Naive):"); break;
    case monte_carlo_uct_search: fprintf(f, "MonteCarlo(UCT):"); break;
    default: fprintf(f, "Unknown(%i):", ss->search_type);
  }
  fprintf(f, "D(%i)", ss->depth);
  if (ss->f_quiesce) fprintf(f, ",Q(%i)", ss->qdepth);
  if (ss->f_id) fprintf(f, ",ID(%i+%i)", ss->id_base, ss->id_step);
  if (ss->f_iid) fprintf(f, ",IID(%i+%i)", ss->iid_base, ss->iid_step);
  if (ss->f_asp) fprintf(f, ",ASP(%i)", ss->asp_width);
  if (ss->f_hh) fprintf(f, ",HH");
  if (ss->f_tt) fprintf(f, ",TT");
  if (ss->f_mws) fprintf(f, ",MWS");
  if (ss->f_nmp) fprintf(f, ",NMP(%i/%i[%i])", ss->nmp_R1, ss->nmp_R2, ss->nmp_cutoff);
  fprintf(f, "}");
  return 0;
}

void make_simple_minimax_searcher(search_settings* ss, int depth, int tt) {
  memset(ss, '\x00', sizeof(search_settings));
  ss->search_type = minimax_search;
  ss->depth = depth;
  ss->f_tt = tt;
}
void make_simple_negamax_searcher(search_settings* ss, int depth, int tt) {
  memset(ss, '\x00', sizeof(search_settings));
  ss->search_type = negamax_search;
  ss->depth = depth;
  ss->f_tt = tt;
}
void make_simple_alphabeta_searcher(search_settings* ss, int depth, int tt) {
  memset(ss, '\x00', sizeof(search_settings));
  ss->search_type = alphabeta_search;
  ss->depth = depth;
  ss->f_tt = tt;
}

/* ************************************************************ */
