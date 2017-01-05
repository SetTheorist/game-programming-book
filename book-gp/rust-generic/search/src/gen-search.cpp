/* $Id: search.c,v 1.7 2010/12/26 18:10:28 apollo Exp apollo $ */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "gen-search.h"

// TODO: fix same_side_moves_again cases!!!!!
//       (note that pruning, etc. assumptions may need to be
//        handled carefully, in addition to sign of recursion...)

void send(...) {
}

/* ************************************************************ */

//#define VALIDATE_HASH 1

/* NOT THREAD SAFE LOGGING! */
//#define LOGGING 1
//#define LOGGING 0
#ifdef LOGGING
static int log_ind=0;
#define LIN(i_, fmt_, ...) do{ if ((i_)<=LOGGING) {\
  fstatus<movet,evalt>(logfile(), "%*s[%i:%s]>>>>" fmt_ "\n", 2*log_ind, "", __LINE__, __func__, ##__VA_ARGS__); \
  ++log_ind; \
}}while(0)
#define LOUT(i_, fmt_, ...) do{ if ((i_)<=LOGGING) {\
  --log_ind; \
  fstatus<movet,evalt>(logfile(), "%*s[%i:%s]<--<" fmt_ "\n", 2*log_ind, "", __LINE__, __func__, ##__VA_ARGS__); \
}}while(0)
#define LOG(i_,fmt_, ...) do{ if ((i_)<=LOGGING) {\
  fstatus<movet,evalt>(logfile(), "%*s[%i:%s]: " fmt_ "\n", 2*log_ind, "", __LINE__, __func__, ##__VA_ARGS__); \
}}while(0)
#else
#define LIN(i_, fmt_, ...)
#define LOUT(i_, fmt_, ...)
#define LOG(i_, fmt_, ...)
#endif

/* ************************************************************ */

// These don't seem to help at all (in fact make it much worse ...)
// many variations tried...
#define F_QIID    0
#define F_IID     0

#define LMR_QMOVES  4
//#define LMR_MOVES   10
static const int LMR_MOVES[MAXSEARCHDEPTH] = {
 100, 30, 20, 10,  8,  7,  6,  5,
   4,  4,  4,  4,  4,  4,  4,  4,
   4,  4,  4,  4,  4,  4,  4,  4,
   4,  4,  4,  4,  4,  4,  4,  4,
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   2,  2,  2,  2,  2,  2,  2,  2,
   2,  2,  2,  2,  2,  2,  2,  2,
};

/* ************************************************************ */

/* just bring the best score from ml[i]..ml[n-1] to position i (swap it) */
template<typename Game>
static inline void get_best_move(int i, int n, typename Game::movet* ml, typename Game::evalt* scorel)
{
  typedef typename Game::evalt evalt;
  typedef typename Game::movet movet;

  evalt  best_score = scorel[i];
  int  besti = i;
  for (int j=i+1; j<n; ++j)
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

/* move ml[i] to the front without disturbing order of the rest */
template<typename Game>
static inline void
move_to_front(typename Game::movet* ml, typename Game::evalt* scorel, int i)
{
  typedef typename Game::evalt evalt;
  typedef typename Game::movet movet;

  if (!i) return;
  const movet m = ml[i];
  const evalt v = scorel[i];
  memmove(&ml[1], &ml[0], sizeof(*ml)*i);
  memmove(&scorel[1], &scorel[0], sizeof(*scorel)*i);
  ml[0] = m;
  scorel[0] = v;
}

/* ************************************************************ */

template<typename Game>
template<typename Evaluator>
typename Game::evalt
searcher<Game>::search_alphabeta_quiesce(
    typename Game::boardt* b,
    const Evaluator* e,
    int qdepth,
    typename Game::evalt alpha,
    typename Game::evalt beta,
    int ply,
    node_type type
    )
{
  ++qnodes_searched;
  ++qnodes_searched_depth[ply];
  if (ply > max_qply_reached) max_qply_reached = ply;

  if (!(qnodes_searched & 0x003FFF)) periodic_check();

  pv_length[ply] = 0;
  if ((qdepth <= 0) || b->terminal() || (ply >= MAXSEARCHDEPTH-1))
  {
    ++qnodes_evaluated;
    ++qnodes_evaluated_depth[ply];
    return e->evaluate_relative(b, ply, alpha, beta);
  }

  /* ?!
  alpha = max(alpha, evalt::B_WIN_IN_N(ply));
  beta = min(beta, evalt::W_WIN_IN_N(ply+1));
  if (alpha >= beta) return alpha;
  */

  movet tt_goodm; tt_goodm.i = 0;
  int   have_tt_goodm = 0;

  /* transposition table */
  {
    ttentry<movet,evalt>  tte;
    hasht h = b->hash();
    if (qtt.retrieve(h, &tte))
    {
      tt_goodm = tte.best_move;
      have_tt_goodm = tte.flags & tt_have_move;
      if (tte.depth < qdepth)
        goto skip_tt;
      if (tte.flags & tt_exact)
      {
        if (have_tt_goodm && tte.best_move.valid())
        {
          pv[ply][ply] = tte.best_move;
          pv_length[ply] = 1;
        }
        return tte.value;
      }
      else if ((tte.flags & tt_upper) && (type != pv_node))
      {
        if (tte.value <= alpha)
        {
          if (have_tt_goodm && tte.best_move.valid())
          {
            pv[ply][ply] = tte.best_move;
            pv_length[ply] = 1;
          }
          return tte.value;
        }
      }
      else if ((tte.flags & tt_lower) && (type != pv_node))
      {
        if (tte.value >= beta)
        {
          if (have_tt_goodm && tte.best_move.valid())
          {
            pv[ply][ply] = tte.best_move;
            pv_length[ply] = 1;
          }
          return tte.value;
        }
      }
    }
  }
skip_tt: { }

  evalt best_val = evalt::B_INF;
  /* first do "stand-pat" (if not in check, e.g.) */
  if (e->allow_stand_pat_quiescence(b))
  {
    ++qnodes_evaluated;
    ++qnodes_evaluated_depth[ply];
    best_val = e->evaluate_relative(b, ply, alpha, beta);
    if (best_val > alpha)
    {
      alpha = best_val;
      if (alpha >= beta)
        return alpha;
    }
  }

  movet ml[boardt::MAXNMOVES];
  evalt scorel[boardt::MAXNMOVES];

  int n = b->gen_moves_quiescence(ml);

  e->evaluate_moves_for_search(b, ml, scorel, n);
#if HISTORY_HEURISTIC
  for (int i=0; i<n; ++i)
    scorel[i] += 10*qhistory.retrieve(ml[i]);
#endif

  if (F_QIID && !have_tt_goodm && (qdepth > 200) && (type==pv_node || type==cut_node))
  {
    /* internal iterative-deepening to find the best move */
    (void)search_alphabeta_quiesce(b, e, min(qdepth-200, qdepth/2), alpha, beta, ply, type);
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
    for (int i=0; i<n; ++i)
    {
      if (tt_goodm.i == ml[i].i)
      {
        const movet m = ml[0];
        ml[0] = ml[i];
        ml[i] = m;
        const evalt s = scorel[0];
        scorel[0] = scorel[i] + 1000000000;
        scorel[i] = s;
        break;
      }
    }
  }
  else
  {
    get_best_move<Game>(0, n, ml, scorel);
  }

  int global_extensions = e->global_search_extensions(b, ply, alpha, beta, type, true);

  int store_tt = 0;
  int num_valid_moves = 0;
  for (int i=0; i<n && !stop_search; ++i)
  {
    evalt val;
    if (i) get_best_move<Game>(i, n, ml, scorel); /* best move is already at front ... */

    make_move_flags mmf = b->make_move(ml[i]);
    if (mmf == invalid_move) continue;
    ++num_valid_moves;

    int extensions = global_extensions
      + e->local_search_extensions(b, ply, alpha, beta, type, true);

    if (settings.lmr.f
        && !store_tt
        && !extensions
        && (type != pv_node)
        && (num_valid_moves > LMR_QMOVES)
        && (qdepth > (settings.lmr.reduction+100))
        )
      extensions -= settings.lmr.reduction;

    /*
    if (mmf == same_side_moves_again)
      val = search_alphabeta_quiesce(b, e, (qdepth-100+extensions), alpha, beta, ply+1,
          (!store_tt ? ((node_type)-type) : ((type==cut_node) ? all_node : cut_node)));
    else
    */
      val = -search_alphabeta_quiesce(b, e, (qdepth-100+extensions), -beta, -alpha, ply+1,
          (!store_tt ? ((node_type)-type) : ((type==cut_node) ? all_node : cut_node)));

    if ((val > best_val) && (extensions < 0))
    {
      /* re-search seemingly good move at full depth... */
      /*
      if (mmf == same_side_moves_again)
        val = search_alphabeta_quiesce(b, e, (qdepth-100), alpha, beta, ply+1,
            (!store_tt ? ((node_type)-type) : ((type==cut_node) ? all_node : cut_node)));
      else
      */
        val = -search_alphabeta_quiesce(b, e, (qdepth-100), -beta, -alpha, ply+1,
            (!store_tt ? ((node_type)-type) : ((type==cut_node) ? all_node : cut_node)));
    }

    /* TODO: put in the pvs search here in q-search... */
    b->unmake_move();

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
#if HISTORY_HEURISTIC
          qhistory.store(ml[i], qdepth);
#endif
          qtt.store(b->hash(), alpha, ml[i], tt_lower|tt_have_move, qdepth);
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
#if HISTORY_HEURISTIC
      qhistory.store(pv[ply][ply], qdepth);
#endif
      qtt.store(b->hash(), alpha, pv[ply][ply], tt_exact|tt_have_move, qdepth);
    }
    else
    {
      movet m; m.i = 0;
      qtt.store(b->hash(), best_val, m, tt_upper, qdepth);
    }
  }
  return best_val;
}

/* ************************************************************ */

template<typename Game>
template<typename Evaluator>
typename Game::evalt
searcher<Game>::search_alphabeta_general(
    typename Game::boardt* b,
    const Evaluator* e,
    int depth,
    typename Game::evalt alpha,
    typename Game::evalt beta,
    int ply,
    node_type type
    )
{
  ++nodes_searched;
  ++nodes_searched_depth[ply];
  if (ply > max_ply_reached) max_ply_reached = ply;
  pv_length[ply] = 0;

  if (b->terminal() || (ply >= MAXSEARCHDEPTH-1))
  {
    ++nodes_evaluated;
    ++nodes_evaluated_depth[ply];
    return e->evaluate_relative(b, ply, alpha, beta);
  }

  if (depth <= 0)
    return search_alphabeta_quiesce(b, e, settings.qdepth, alpha, beta, ply, type);

  /* ?!
  alpha = max(alpha, evalt::B_WIN_IN_N(ply));
  beta = min(beta, evalt::W_WIN_IN_N(ply+1));
  if (alpha >= beta) return alpha;
  */

  movet tt_goodm; tt_goodm.i = 0;
  int   have_tt_goodm = 0;

  /* transposition table */
  {
    ttentry<movet,evalt>  tte;
    hasht h = b->hash();
    if (tt.retrieve(h, &tte))
    {
      tt_goodm = tte.best_move;
      have_tt_goodm = tte.flags & tt_have_move;
      if (tte.depth < depth)
      {
        ++tt.stats.shallow;
        goto skip_tt;
      }
      if (tte.depth > depth) ++tt.stats.deep;
      ++tt.stats.used;
      //if (tte.flags & tt_exact)
      if ((tte.flags & tt_exact) && (type != pv_node))
      {
        ++tt.stats.used_exact;
        if (have_tt_goodm && tte.best_move.valid())
        {
          pv[ply][ply] = tte.best_move;
          pv_length[ply] = 1;
        }
        ++tt.stats.cutoff;
        return tte.value;
      }
      else if ((tte.flags & tt_upper) && (type != pv_node))
      {
        ++tt.stats.used_upper;
        if (tte.value <= alpha) { ++tt.stats.cutoff; return tte.value; }
      }
      else if ((tte.flags & tt_lower) && (type != pv_node))
      {
        ++tt.stats.used_lower;
        if (tte.value >= beta) { ++tt.stats.cutoff; return tte.value; }
      }
    }
  }
skip_tt: { }


  /* null-move-pruning:
   *  - swap sides
   *  - search with reduced depth
   *  - if result is <alpha, then prune
   */
  if (settings.nmp.f && (type != pv_node) && (beta < evalt::W_WIN_IN_N(ply+1)))
  {
    int nmr = (depth>settings.nmp.cutoff) ? settings.nmp.R1 : settings.nmp.R2;
    if ((depth > (nmr+100)) && !b->last_move().null() && !b->in_check(b->to_move))
    {
      if (b->make_move(movet::nullmove) != invalid_move)
      {
        evalt nmv = -search_alphabeta_general(b, e, depth-100-nmr, -beta, -beta+1, ply+1, ((type == cut_node) ? all_node : cut_node));
        b->unmake_move();
        if (nmv>=beta && !stop_search)
          return beta;
      }
    }
  }

  movet ml[boardt::MAXNMOVES];
  evalt scorel[boardt::MAXNMOVES];
  int n = b->gen_moves(ml);
  if (n==0) return evalt::B_WIN_IN_N(ply);

  e->evaluate_moves_for_search(b, ml, scorel, n);
#if HISTORY_HEURISTIC
  for (int i=0; i<n; ++i)
    scorel[i] += 10*history.retrieve(ml[i]);
#endif

  if (have_tt_goodm)
  {
    /* bonus score to transition-table move, and move to front */
    for (int i=0; i<n; ++i) {
      if (tt_goodm.i == ml[i].i) {
        {
          const movet m = ml[0];
          ml[0] = ml[i];
          ml[i] = m;
          const evalt s = scorel[0];
          scorel[0] = scorel[i] + 1000000000;
          scorel[i] = s;
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
      int besti = 0;
      evalt bestval = evalt::B_INF;
      for (int i=0; i<n; ++i)
      {
        if (b->make_move(ml[i]) == invalid_move) continue;
        evalt val = -search_alphabeta_general(b, e, 0, -beta, -max(bestval,alpha), ply+1, (node_type)-type);
        b->unmake_move();
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
      get_best_move<Game>(0, n, ml, scorel);
    }
  }

  int global_extensions = e->global_search_extensions(b, ply, alpha, beta, type, false);

  /* loop over all moves */
  int num_valid_moves = 0;
  int store_tt = 0;
  evalt best_val = evalt::B_INF;
  for (int i=0; i<n && !stop_search; ++i)
  {
    evalt val;
    if (i) get_best_move<Game>(i, n, ml, scorel);
    if (b->make_move(ml[i]) == invalid_move) continue;
    ++num_valid_moves;

    int extensions = global_extensions;
    extensions += e->local_search_extensions(b, ply, alpha, beta, type, false);

    if (settings.lmr.f
        && !extensions
        && !store_tt
        && (type != pv_node)
        && (depth > (settings.lmr.reduction+100))
        && (num_valid_moves > LMR_MOVES[ply])
        )
      extensions -= settings.lmr.reduction;

    {
      if (!store_tt)
      {
        val = -search_alphabeta_general(b, e, (depth-100+extensions), -beta, -alpha, ply+1, (node_type)-type);
        if ((val > best_val) && (extensions < 0))
        {
          /* Re-search full-depth */
          val = -search_alphabeta_general(b, e, (depth-100), -beta, -alpha, ply+1, (node_type)-type);
        }
      }
      else
      {
        val = -search_alphabeta_general(b, e, (depth-100+extensions), -(alpha+1),  -alpha, ply+1, ((type==cut_node) ? all_node : cut_node));
        if ((val >= (alpha+1)) && (val < beta))
        {
#if HISTORY_HEURISTIC
          history.store(ml[i], depth);
#endif
          if (extensions < 0)
            extensions = 0;
          val = -search_alphabeta_general(b, e, (depth-100+extensions), -beta, -alpha, ply+1, type);
        }
      }
    }
    b->unmake_move();

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
#if HISTORY_HEURISTIC
          history.store(ml[i], depth+extensions);
#endif
          tt.store(b->hash(), alpha, ml[i], tt_lower|tt_have_move, depth);
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
      return evalt::STALEMATE_RELATIVE_IN_N(ply);

    if (store_tt)
    {
      tt.store(b->hash(), alpha, pv[ply][ply], tt_exact|tt_have_move, depth);
#if HISTORY_HEURISTIC
      history.store(pv[ply][ply], (depth+global_extensions));
#endif
    }
    else
    {
      movet m; m.i = 0;
      tt.store(b->hash(), best_val, m, tt_upper, depth);
    }
  }
  return best_val;
}

/* ************************************************************ */

template<typename Game>
int searcher<Game>::early_abort_on_time(
  ui64 last_iter_time,
  typename Game::evalt best_val,
  int iter,
  typename Game::evalt evals_by_iter[],
  int num_last_iter_pv,
  typename Game::movet last_iter_pv[]
  )
{
  if (!tc.active || !num_last_iter_pv || ((int)(last_iter_time/10000) <= tc.allocated_cs))
    return 0;

  const evalt last_iter_eval = evals_by_iter[iter - 1];
  if ((pv[0][0].i == last_iter_pv[0].i) && ((double)(best_val - last_iter_eval)/(double)max(abs(last_iter_eval),100) >= -0.10))
  {
    /* if we have used our allocated time but move seems ok still (hasn't dropped by more than 10%), abort early */
#ifdef  LOGGING
    fstatus<movet,evalt>(logfile(), "Early abort on time, unchanged best move [%m], eval [%q was %q]\n",
        pv[0][0], best_val, last_iter_eval);
#endif
    return 1;
  }
  else if ((pv[0][0].i == last_iter_pv[0].i) && ((double)(best_val - last_iter_eval)/max(abs(last_iter_eval),100) >= -0.25))
  {
    /* if we've exceeded our panic time and move we still have in hand is not _too_ bad, abort early */
#ifdef  LOGGING
    fstatus<movet,evalt>(logfile(), "Early abort on panic time, best move [%m was %m], eval [%q was %q]\n",
        pv[0][0], last_iter_pv[0], best_val, last_iter_eval);
#endif
    return 1;
  }
  else
  {
#ifdef  LOGGING
    fstatus<movet,evalt>(logfile(), "No early abort on time, best move [%m was %m], eval [%q was %q]\n",
        pv[0][0], last_iter_pv[0], best_val, last_iter_eval);
#endif
  }

  return 0;
}

/* ************************************************************ */

template<typename Game>
int searcher<Game>::another_iteration(
  ui64 last_iter_time,
  int iter,
  typename searcher<Game>::evalt evals_by_iter[]
  )
{
  if (iter<=1)
    return 1;

  if (iter >= MAXSEARCHDEPTH)
    return 0;

  int est_time_cs = 5 * last_iter_time / 10000;
#ifdef  LOGGING
  fstatus<movet,evalt>(logfile(), "Estimated time: %.2f (last: %.2f)\n", est_time_cs/100.0, last_iter_time/1e6);
#endif

  if (est_time_cs*6 > tc.panic_cs)
    return 0;

  if (est_time_cs > tc.allocated_cs)
  {
    if (iter <= 1)
      return 0;

    /* don't break (yet) if evaluation dropped by a lot */
    if (evals_by_iter[iter-1] >= evals_by_iter[iter-2] - 300)
      return 0;
#ifdef  LOGGING
    fstatus<movet,evalt>(logfile(), "Extending into panic time on eval drop: %q --> %q\n",
        evals_by_iter[iter-2], evals_by_iter[iter-1]);
#endif
  }

  return 1;
}

/* ************************************************************ */

template<typename Game>
template<typename Evaluator>
typename searcher<Game>::evalt
searcher<Game>::search_alphabeta(
    typename searcher<Game>::boardt* b,
    const Evaluator* e,
    typename searcher<Game>::movet* out_pv,
    int* out_pv_length
    )
{
  LIN(1, "search_alphabeta()");

  stop_search = 0;

  pv_length[0] = 0;
  tt.reset_stats();
  qtt.reset_stats();
  nodes_searched = 1;
  qnodes_searched = 0;
  nodes_evaluated = 0;
  qnodes_evaluated = 0;

#if HISTORY_HEURISTIC
  /* TODO: preserve between calls? */
  history.reset();
  qhistory.reset();
#endif

  /* generate moves */
  movet ml[boardt::MAXNMOVES];
  int n = b->gen_moves(ml);

  /* prune invalid moves */
  for (int i=0; i<n; ++i) {
    if (b->make_move(ml[i]) == invalid_move)
      ml[i--] = ml[n-- -1];
    else
      b->unmake_move();
  }

  /* stalemate */
  if (n==0)
  {
    LOUT(1, " (stalemate) eval=%q", evalt::STALEMATE_RELATIVE);
#if (LOGGING>=1)
    b->showf(logfile());
#endif
    return evalt::STALEMATE_RELATIVE;
  }

  /* evaluate and sort moves */
  evalt scorel[boardt::MAXNMOVES];
  e->evaluate_moves_for_search(b, ml, scorel, n);
  for (int i=0; i<n; ++i) get_best_move<Game>(i, n, ml, scorel);

  /* try transposition table move first, if we've got one */
  {
    ttentry<movet,evalt>  tte;
    hasht h = b->hash();
    if (tt.retrieve(h, &tte))
      if (tte.flags & tt_have_move)
        for (int i=0; i<n; ++i)
          if (ml[i].i == tte.best_move.i)
          {
            move_to_front<Game>(ml, scorel, i);
            break;
          }
  }

  const evalt static_eval = e->evaluate_relative(b, 0, evalt::B_INF, evalt::W_INF);

  ui64 last_iter_time = 0;
  int   num_last_iter_pv = 0;
  movet last_iter_pv[MAXSEARCHDEPTH];
  int   iter = 1;
  evalt evals_by_iter[MAXSEARCHDEPTH] = {static_eval};

  evalt bestv = evalt::B_INF;

  /* iterative deepening */
  int d;
  const int start_depth = settings.id.f ? settings.id.base : settings.depth;
  const int depth_increment = settings.id.f ? settings.id.step : 100;
  ui64 bt = read_clock();
  search_start_time = bt;
  search_stop_time = bt + (tc.active ? (tc.force_stop_cs * 10000LL) : 1000000000000LL);
  int num_best_moves = 0;
  for (d=start_depth; d<=settings.depth; d+=depth_increment, ++iter)
  {
    /* can't get shorter mate by searching deeper... */
    if ((evalt::W_WIN_IN_N(d/100) < evals_by_iter[iter-1])
      || (evalt::B_WIN_IN_N(d/100) > evals_by_iter[iter-1]))
      break;

    /* do another iteration? */
    if (tc.active && !another_iteration(last_iter_time, iter, evals_by_iter))
      break;

    /* shuffle previous iteration's good moves to the front */
    if (num_best_moves)
    {
      for (int i=1; i<=num_best_moves; ++i)
      {
        for (int j=0; j<n; ++j)
        {
          if (scorel[j] == i)
          {
            move_to_front<Game>(ml, scorel, j);
            break;
          }
        }
      }
      num_best_moves = 0;
    }
    memset(scorel, '\x00', sizeof(scorel));

#ifdef LOGGING
    if (LOGGING>=1)
    {
      fprintf(logfile(), "ID: DEPTH = %i\n# \t", d);
      for (int i=0; i<n; ++i)
      {
#if HISTORY_HEURISTIC
        fstatus<movet,evalt>(logfile(), "%m(%.2f)H:%u ", ml[i], scorel[i], history.retrieve(ml[i]));
#else
        fstatus<movet,evalt>(logfile(), "%m(%.2f) ", ml[i], scorel[i]);
#endif
        if (!((i+1)%4) || i==n-1) fprintf(logfile(), "\n\t");
      }
      fprintf(logfile(), "\n");
    }
#endif

    max_ply_reached = 0;
    max_qply_reached = 0;

    memset(nodes_searched_depth, '\x00', sizeof(nodes_searched_depth));
    memset(qnodes_searched_depth, '\x00', sizeof(qnodes_searched_depth));
    memset(nodes_evaluated_depth, '\x00', sizeof(nodes_evaluated_depth));
    memset(qnodes_evaluated_depth, '\x00', sizeof(qnodes_evaluated_depth));

    /* Loop over all moves */
    bestv = evalt::B_INF;
    pv_length[0] = 0;
    for (int i=0; i<n && !stop_search; ++i)
    {
#ifdef  LOGGING
      const ui64 pre_nodes = (nodes_searched+qnodes_searched);
#endif

      evalt v;
      if (b->make_move(ml[i]) == invalid_move)
      {
        /* this shouldn't happen! */
#ifdef  LOGGING
        fstatus<movet,evalt>(logfile(), "Unexpected invalid_move %m\n", ml[i]);
#endif
        if (i!=n-1) memmove(&ml[i], &ml[i+1], sizeof(movet)*(n-i-1));
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
          evals_by_iter[iter-1] + settings.asp.width,
          evals_by_iter[iter-1] + 4*settings.asp.width,
          evalt::W_INF
        };
        evalt lower_bounds[3] = {
          evals_by_iter[iter-1] - settings.asp.width,
          evals_by_iter[iter-1] - 4*settings.asp.width,
          evalt::B_INF
        };
        int upper_level = 0;
        int lower_level = 0;
        evalt upper_bound, lower_bound;
        do
        {
          upper_bound = upper_bounds[upper_level];
          lower_bound = lower_bounds[lower_level];
          v = -search_alphabeta_general(b, e, d-100, -upper_bound, -lower_bound, 1, pv_node);
#ifdef  LOGGING
          const ui64 post_nodes=(nodes_searched+qnodes_searched);
          LOG(1, "ml[%i]=%m window=(%q,%q) --> %q  [%'llu]", i, ml[i], lower_bound, upper_bound, v,
              (post_nodes - pre_nodes));
#endif
          if (v <= lower_bound)
            ++lower_level;
          else if (v >= upper_bound)
            ++upper_level;
        } while (((v <= lower_bound) || (v >= upper_bound)) && (upper_level<3 && lower_level<3));
      }
      else
      {
        /* try minimal window search on best val so-far */
        v = -search_alphabeta_general(b, e, d-100, -(bestv+1),  -bestv, 1, cut_node);
        if (v >= bestv + 1)
        {
          LOG(1, "ml[%i]=%m   MWS re-search (got %q)", i, ml[i], v);
          v = -search_alphabeta_general(b, e, d-100, -(evalt::W_INF), -bestv, 1, pv_node);
        }
      }
      b->unmake_move();
      ui64  et = read_clock();
      last_iter_time = et - bt;

      const ui64 post_nodes=(nodes_searched+qnodes_searched);

#ifdef LOGGING
      LOG(1, "ml[%i]=%m  %q  [%'llu]", i, ml[i], v, (post_nodes - pre_nodes));
#endif
      if (v>bestv && !stop_search)
      {
        LOG(1, "New best move: %m (#%i) @ %q", ml[i], i, v);
        bestv = v;
        update_pv(0, ml[i]);
#if HISTORY_HEURISTIC
        history.store(ml[i], d);
#endif
        scorel[i] = ++num_best_moves;

        {
          /* N.B. we scale the eval down to match hachu more closely */
          evalt wb_eval = ((bestv > evalt::W_WIN_IN_N(MAXSEARCHDEPTH))||(bestv < evalt::B_WIN_IN_N(MAXSEARCHDEPTH))) ? bestv : bestv/3;
          send("%i %g %i %llu [%i/%i/%i] ",
              d/100, (double)wb_eval, (int)(last_iter_time/10000), post_nodes,
              d/100, max_ply_reached, max_qply_reached);
          if ((bestv > evalt::W_WIN_IN_N(MAXSEARCHDEPTH))||(bestv < evalt::B_WIN_IN_N(MAXSEARCHDEPTH)))
            send("(Mate-in-%i) ", (int)bestv.PLIES_TO_WIN());
          if (pv_length[0])
          {
#ifdef  LOGGING
            fstatus<movet,evalt>(stdout, "%M\n", pv[0], pv_length[0]);
#endif
            //if (in_winboard)
            //  fstatus<movet,evalt>(logfile(), "%M\n", pv[0], pv_length[0]);
          }
          else
          {
            send("[]\n");
          }
        }

        if (early_abort_on_time(last_iter_time, bestv, iter, evals_by_iter, num_last_iter_pv, last_iter_pv))
          break;
      }
    } /* end loop over all moves */

    ui64  et = read_clock();
    {
#ifdef  LOGGING
      fstatus<movet,evalt>(logfile(), "ID(%i) eval=%q time=%.4fs PV=[%M]",
          d, bestv, (et-bt)*1e-6, pv[0], pv_length[0]);
      fprintf(logfile(), "  (%'lluns/%'lluqns %'llune/%'lluqne)\n",
          nodes_searched, qnodes_searched, nodes_evaluated, qnodes_evaluated);
#endif

#ifdef  LOGGING
      {
        const int maxd = max(max_ply_reached, max_qply_reached);
        for (int i=0; i<=maxd; ++i)
          fprintf(logfile(), "\t[%2i]  %'12lluns %'12lluqns  %'12llune %'12lluqne\n",
              i, nodes_searched_depth[i], qnodes_searched_depth[i], nodes_evaluated_depth[i], qnodes_evaluated_depth[i]);
      }
#endif

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
      const evalt eval = ((best > evalt::W_WIN_IN_N(MAXSEARCHDEPTH))||(best < evalt::B_WIN_IN_N(MAXSEARCHDEPTH))) ? best : best/3;
      send("%i %g %i %llu [%i/%i/%i] ",
          d/100, (double)eval, (int)((et-bt)/10000), (nodes_searched+qnodes_searched),
          d/100, max_ply_reached, max_qply_reached);
      if ((best > evalt::W_WIN_IN_N(MAXSEARCHDEPTH))||(best < evalt::B_WIN_IN_N(MAXSEARCHDEPTH)))
        send("(Mate-in-%i) ", (int)best.PLIES_TO_WIN());
      if (pv_length[0])
      {
#ifdef  LOGGING
        fstatus<movet,evalt>(stdout, "%M\n", last_iter_pv, num_last_iter_pv);
#endif
        //if (in_winboard)
        //  fstatus<movet,evalt>(logfile(), "%M\n", last_iter_pv, num_last_iter_pv);
      }
      else
      {
        send(" []\n");
      }
    }
  }
#ifdef LOGGING
  if (1<=LOGGING)
  {
    fprintf(logfile(), "ID: **********\n# \t");
    for (int i=0; i<n; ++i)
    {
      fstatus<movet,evalt>(logfile(), "%m(%g)  ", ml[i], (double)scorel[i]);
      if (!((i+1)%5) || i==n-1) fprintf(logfile(), "\n\t");
    }
    fprintf(logfile(), "\n");
  }
#endif

  *out_pv_length = num_last_iter_pv;
  memcpy(out_pv, last_iter_pv, sizeof(movet)*num_last_iter_pv);
  LOUT(1, " eval=%q pv=[ %M ]", evals_by_iter[iter], out_pv, *out_pv_length);
  return bestv;
}

/* ************************************************************ */

template<typename Game>
template<typename Evaluator>
typename Game::evalt
searcher<Game>::search(
    typename Game::boardt* b,
    const Evaluator* e,
    typename Game::movet* out_pv,
    int* out_pv_length
    )
{
  b->set_piece_vals(e->get_piece_vals());
  b->set_piece_square_vals(e->get_piece_square_vals());
  return search_alphabeta(b, e, out_pv, out_pv_length);
}

