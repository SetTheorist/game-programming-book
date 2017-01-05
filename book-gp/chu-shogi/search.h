#ifndef INCLUDED_SEARCH_H_
#define INCLUDED_SEARCH_H_
/* $Id: search.h,v 1.6 2010/12/26 18:10:28 apollo Exp apollo $ */
#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

#include <stdio.h>
#include "board.h"
#include "evaluate.h"

typedef struct time_control {
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
} time_control;


#define MAXSEARCHDEPTH  64

typedef struct search_settings {
  int   search_type;
  int   depth;    /* in %ge of ply */
  /* iterative deepening */
  int   f_id;
  int   id_base;
  int   id_step;
  /* internal iterative deepening */
  int   f_iid;
  int   iid_base;
  int   iid_step;
  /* aspiration search */
  int   asp_width;
  /* quiescense search */
  int   qdepth;    /* in %ge of ply */
  /* null move pruning */
  int   f_nmp;
  int   nmp_cutoff; /* use R1 reduction if depth>cutoff else R2 */
  int   nmp_R1;
  int   nmp_R2;
  /* futility */
  int   f_futility;
  int   f_extfutility;
  /* razoring */
  int   f_razoring;
  int   f_limrazoring;
  /* late-move reduction */
  int   f_lmr;
} search_settings;

typedef enum search_types {
  alphabeta_search,
} search_types;

void make_simple_alphabeta_searcher(search_settings* ss, int depth);

int show_settings(FILE* f, const search_settings* s);

#define TTNUM (16*1024*1024)
#define TTMASK (TTNUM-1)
#define QTTNUM (16*1024*1024)
#define QTTMASK (QTTNUM-1)

typedef enum tt_flags {
  tt_invalid=0,
  tt_exact=1,
  tt_upper=2,
  tt_lower=4,
  tt_have_move=16,
} tt_flags;

typedef struct ttentry {
  hasht lock;
  evalt value;
  movet best_move;
  ui16  depth;
  ui16  flags;
} ttentry;

int ttable_init();

extern ui64 nodes_searched;
extern ui64 qnodes_searched;
extern ui64 nodes_evaluated;
extern ui64 qnodes_evaluated;

enum make_move_flags {
  ok_move=0, invalid_move=1, same_side_moves_again=2,
};

extern int stop_search;

evalt search(board* b, const search_settings* s, const evaluator* e, const time_control* tc, movet* pv, int* pv_length);

#ifdef  __cplusplus
};
#endif/*__cplusplus*/
#endif /* INCLUDED_SEARCH_H_ */
