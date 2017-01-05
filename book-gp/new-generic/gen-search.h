#ifndef INCLUDED_GEN_SEARCH_H_
#define INCLUDED_GEN_SEARCH_H_
/* $Id: search.h,v 1.6 2010/12/26 18:10:28 apollo Exp apollo $ */
#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#define sendlog printf

struct time_control {
  int active;           /* TODO: confirm that this is handled correctly... */

  /* Initial time-control */
  int max_depth;        /* 0 = unlimited */
  ui64 max_nodes;       /* 0 = unlimited */
  int increment_cs;     /* increment-per-move, in centi-seconds */
  int byoyomi_cs;       /* time per move (not accumulated), in centi-seconds */
  int starting_cs;      /* initial time on clock, in centi-seconds */
  int moves_per;        /* moves per control period */

  /* Current state at start of move */
  int moves_remaining;  /* 0 = sudden death */
  int remaining_cs;     /* current time on clock, in centi-seconds */

  /* Working details */
  int allocated_cs;     /* time allocated for move, in centi-seconds */
  int panic_cs;         /* extra time allocated for move, in centi-seconds */
  int force_stop_cs;    /* absolute max time to use */
  ui64 start_clock;     /* starting clock time (microseconds from epoch) */

  int showf(FILE* ff) const {
    int n = 0;
    n += fprintf(ff, "<%i", active);
    n += fprintf(ff, "|%i,%llu,%i,%i,%i,%i", max_depth, max_nodes, increment_cs, byoyomi_cs, starting_cs, moves_per);
    n += fprintf(ff, "|%i,%i", moves_remaining, remaining_cs);
    n += fprintf(ff, "|%i,%i,%i,%llu", allocated_cs, panic_cs, force_stop_cs, start_clock);
    n += fprintf(ff, ">");
    return n;
  }
};


#define MAXSEARCHDEPTH  64

struct search_settings {
  int   depth;    /* in %ge of ply */
  /* quiescence search */
  int   qdepth;    /* in %ge of ply */
  void set_depth(int depth_, int qdepth_) {
    depth = depth_; qdepth = qdepth_;
  }
  /* iterative deepening */
  struct {
    int   f;
    int   base;
    int   step;
  } id;
  void set_id(int f_=1, int base_=200, int step_=100) {
    id.f = f_; id.base = base_; id.step = step_;
  }
  /* internal iterative deepening */
  struct {
    int   f;
    int   base;
    int   step;
  } iid;
  void set_iid(int f_=0, int base_=200, int step_=100) {
    iid.f = f_; iid.base = base_; iid.step = step_;
  }
  /* aspiration search */
  struct {
    int   f;
    int   width;
  } asp;
  void set_asp(int f_=0, int width_=100) {
    asp.f = f_; asp.width = width_;
  }
  /* null move pruning */
  struct {
    int   f;
    int   cutoff; /* use R1 reduction if depth>cutoff else R2 */
    int   R1;
    int   R2;
  } nmp;
  void set_nmp(int f_=0, int cutoff_=600, int R1_=300, int R2_=200) {
    nmp.f = f_; nmp.cutoff = cutoff_; nmp.R1 = R1_; nmp.R2 = R2_;
  }
  /* futility */
  struct {
    int   f;
    int   f_ext;
  } futility;
  void set_futility(int f_=0, int f_ext_=200) {
    futility.f = f_; futility.f_ext = f_ext_;
  }
  /* razoring */
  struct {
    int   f;
    int   f_lim;
  } razoring;
  void set_razoring(int f_=0, int f_lim_=100) {
    razoring.f = f_; razoring.f_lim = f_lim_;
  }
  /* late-move reduction */
  struct {
    int   f;
    int   reduction;
  } lmr;
  void set_lmr(int f_=0, int reduction_=200) {
    lmr.f = f_; lmr.reduction = reduction_;
  }

  int show(FILE* f) const {
    fprintf(f, "{");
    fprintf(f, "AB:");
    fprintf(f, "D(%i)", depth);
    fprintf(f, "Q(%i)", qdepth);
    if (id.f) fprintf(f, ",ID(%i+%i)", id.base, id.step);
    if (iid.f) fprintf(f, ",IID(%i+%i)", iid.base, iid.step);
    fprintf(f, ",ASP(%i)", asp.width);
    fprintf(f, ",HH");
    fprintf(f, ",QHH");
    fprintf(f, ",TT");
    fprintf(f, ",QTT");
    if (lmr.f) fprintf(f, ",LMR(%i)", lmr.reduction);
    if (futility.f) fprintf(f, ",FUT");
    if (futility.f_ext) fprintf(f, ",EXTFUT");
    if (razoring.f) fprintf(f, ",RAZ");
    if (razoring.f_lim) fprintf(f, ",LIMRAZ");
    if (nmp.f) fprintf(f, ",NMP(%i/%i[%i])", nmp.R1, nmp.R2, nmp.cutoff);
    fprintf(f, "}");
    return 0;
  }
};

#define TTNUM (16*1024*1024)
#define TTMASK (TTNUM-1)
#define QTTNUM (16*1024*1024)
#define QTTMASK (QTTNUM-1)

enum tt_flags {
  tt_invalid=0,
  tt_exact=1,
  tt_upper=2,
  tt_lower=4,
  tt_have_move=16,
};

template <typename movet, typename evalt>
struct ttentry {
  hasht lock;
  evalt value;
  movet best_move;
  ui16  depth;
  ui16  flags;
};

template<typename Game>
struct transposition_table {
  typedef typename Game::evalt evalt;
  typedef typename Game::movet movet;
  typedef ttentry<movet,evalt> entry;

  struct {
    int hit;
    int false_hit;
    int deep;
    int shallow;
    int used;
    int used_exact;
    int used_upper;
    int used_lower;
    int cutoff;
  } stats;
  int num;
  entry* entries;

  transposition_table(int num_) {
    num = num_;
    entries = (entry*)calloc(num, sizeof(*entries));
  }
  ~transposition_table() {
    free(entries);
  }
  transposition_table(const transposition_table<Game>& tt) {
    num = tt.num;
    entries = (entry*)calloc(num, sizeof(*entries));
    memcpy(entries, tt.entries, num*sizeof(*entries));
  }
  const transposition_table<Game>& operator = (const transposition_table<Game>& tt) {
    num = tt.num;
    entries = (entry*)realloc(entries, num*sizeof(*entries));
    memcpy(entries, tt.entries, num*sizeof(*entries));
    return *this;
  }

  void reset_stats() {
    memset((void*)&stats, '\x00', sizeof(stats));
  }
  inline int TTLOC(hasht h) { return h % num; }
  //inline int TTLOC(hasht h) { return h & mask; }
  inline int retrieve(hasht hash, entry* tte)
  {
    if (entries[TTLOC(hash)].flags == tt_invalid) return 0;
    ++stats.hit;
    if (entries[TTLOC(hash)].lock != hash) { ++stats.false_hit; return 0; }
    *tte = entries[TTLOC(hash)];
    return 1;
  }
  inline int store(hasht hash, evalt score, movet m, int flags, int depth)
  {
    /* TODO: investigate other replacement schemes */
    /* most-recent replacement scheme: */
    entries[TTLOC(hash)].lock = hash;
    entries[TTLOC(hash)].value = score;
    entries[TTLOC(hash)].best_move = m;
    entries[TTLOC(hash)].depth = depth;
    entries[TTLOC(hash)].flags = flags;
    return 0;
  }
};


#if HISTORY_HEURISTIC
// The alternative (color/piece/to) doesn't seem any better (in fact, a bit worse)
#define HH_TO_FROM

template <typename Game>
struct history_table {
  typedef typename Game::evalt evalt;
  typedef typename Game::movet movet;

#ifdef  HH_TO_FROM
  ui32 hist_heur[Game::MAXBOARDI][Game::MAXBOARDI];
#else
  ui32 hist_heur[2][Game::num_piece][Game::MAXBOARDI];
#endif

  void reset()
  {
    memset((void*)hist_heur, '\x00', sizeof(hist_heur));
  }

  void store(movet m, int depth)
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

  ui32 retrieve(movet m)
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
};
#endif

enum node_type {
  pv_node   =  0,
  all_node  = -1,
  cut_node  = +1
};

template<typename Game>
struct searcher {
  typedef typename Game::evalt evalt;
  typedef typename Game::movet movet;
  typedef typename Game::boardt boardt;

  search_settings settings;

  time_control tc;

  /*volatile*/ int stop_search;
  ui64 search_start_time;
  ui64 search_stop_time;
  void periodic_check()
  {
    if (tc.active)
    {
      const ui64 t = read_clock();
      if (t >= search_stop_time)
      {
        sendlog("*** Enforcing hard stop on time [%'.2fs]***\n", (t - search_start_time)*1e-6);
        stop_search = 1;
      }
    }
  }

  ui64 nodes_searched;
  ui64 nodes_searched_depth[MAXSEARCHDEPTH+1];
  ui64 nodes_evaluated;
  ui64 nodes_evaluated_depth[MAXSEARCHDEPTH+1];

  ui64 qnodes_searched;
  ui64 qnodes_searched_depth[MAXSEARCHDEPTH+1];
  ui64 qnodes_evaluated;
  ui64 qnodes_evaluated_depth[MAXSEARCHDEPTH+1];

  int max_ply_reached;
  int max_qply_reached;

  transposition_table<Game> tt;
  transposition_table<Game> qtt;

  int pv_length[MAXSEARCHDEPTH];
  movet pv[MAXSEARCHDEPTH][MAXSEARCHDEPTH];

  //movet qkillers[MAXSEARCHDEPTH][2];
#if HISTORY_HEURISTIC
  history_table history;
  history_table qhistory;
#endif

  searcher(int depth, int qdepth, int ttnum, int qttnum) : tt(ttnum), qtt(qttnum) {
    memset((void*)&tc, '\x00', sizeof(tc));
    memset((void*)&settings, '\x00', sizeof(settings));
    settings.depth = depth;
    settings.qdepth = qdepth;
  }
  ~searcher()
  { }
  // default copy constructor & assignment, but they are expensive operations!

  template <typename evaluator>
  evalt search(boardt* b, const evaluator* e, movet* pv, int* pv_length);

  FILE* logfile() { return stderr; }

protected:
  template <typename evaluator>
  evalt
  search_alphabeta_quiesce(
     boardt* b,
     const evaluator* e,
     int qdepth,
     evalt alpha,
     evalt beta,
     int ply,
     node_type type
     );

  template <typename evaluator>
  evalt
  search_alphabeta_general(
      boardt* b,
      const evaluator* e,
      int depth,
      evalt alpha,
      evalt beta,
      int ply,
      node_type type
      );

  template <typename evaluator>
  evalt
  search_alphabeta(
      boardt* b,
      const evaluator* e,
      movet* out_pv,
      int* out_pv_length
      );

  inline void update_pv(int ply, movet m) {
    memcpy(&pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(*pv[ply])*pv_length[ply+1]);
    pv[ply][ply] = m;
    pv_length[ply] = pv_length[ply+1]+1;
  }

  int early_abort_on_time(ui64 last_iter_time,
      evalt best_val, int iter, evalt evals_by_iter[],
      int num_last_iter_pv, movet last_iter_pv[]);

  int another_iteration(
      ui64 last_iter_time,
      int iter,
      evalt evals_by_iter[]);
};


enum make_move_flags {
  ok_move=0, invalid_move=1, same_side_moves_again=2,
};

/* API assumed for game, boardt, movet, evalt, evaluator:
 *
 * game
 *  type boardt
 *  type movet
 *  type evalt
 *
 * game::boardt:
 *  int gen_moves(movet ml[]);
 *  make_move_flags make_move(movet m);
 *  unmake_move();
 *  hasht hash();
 *  static int MAXNMOVES;
 *  bool terminal();
 *  bool in_check(side);
 *
 * movet:
 *  // value type union with int .i
 *  bool valid();
 *  bool null();
 *  static movet nullmove;
 *
 * evalt:
 *  // value type, orderable (either integer or floating-point typically)
 *  static evalt W_INF;
 *  static evalt B_INF;
 *  static evalt STALEMATE_RELATIVE;
 *  static evalt W_WIN_IN_N(int n);
 *  static evalt B_WIN_IN_N(int n);
 *  int PLIES_TO_WIN(evalt);
 *
 * evaluator:
 *  evalt evaluate_relative(boardt* b, int ply, evalt alpha, evalt beta);
 *
 */

/* ******************************************************************************** */
/* IMPLEMENTATION OF TEMPLATES, ETC */
/* ******************************************************************************** */

#include "gen-search.cpp"

/* ******************************************************************************** */
/* ******************************************************************************** */

#endif /* INCLUDED_GEN_SEARCH_H_ */
