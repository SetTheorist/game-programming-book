#ifndef INCLUDED_SEARCH_H_
#define INCLUDED_SEARCH_H_
/* $Id: search.h,v 1.6 2010/12/26 18:10:28 apollo Exp apollo $ */
#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

#include <stdio.h>
#include "board.h"

#define MAXSEARCHDEPTH  64

typedef struct search_settings {
  int    search_type;
  int    depth;    /* in %ge of ply */
  /* history heuristic */
  int    f_hh;
  /* transposition table */
  int    f_tt;
  /* iterative deepening */
  int    f_id;
  int    id_base;
  int    id_step;
  /* internal iterative deepening */
  int f_iid;
  int    iid_base;
  int    iid_step;
  /* minimal window search */
  int f_mws;
  /* aspiration search */
  int f_asp;
  int asp_width;
  /* quiescense search */
  int    f_quiesce;
  int    qdepth;    /* in %ge of ply */
  /* null move pruning */
  int    f_nmp;
  int    nmp_cutoff; /* use R1 reduction if depth>cutoff else R2 */
  int    nmp_R1;
  int    nmp_R2;
} search_settings;

typedef enum search_types {
  minimax_search, negamax_search, alphabeta_search,
  monte_carlo_naive_search, monte_carlo_uct_search,
} search_types;

void make_simple_minimax_searcher(search_settings* ss, int depth, int tt);
void make_simple_negamax_searcher(search_settings* ss, int depth, int tt);
void make_simple_alphabeta_searcher(search_settings* ss, int depth, int tt);

int show_settings(FILE* f, const search_settings* s);

#define TTNUM (8*1024*1024)
//#define TTNUM (256)

typedef enum tt_flags {
  tt_invalid=0, tt_valid=1,
  tt_upper=2|tt_valid, tt_lower=4|tt_valid,
  tt_exact=tt_upper|tt_lower|tt_valid
} tt_flags;

typedef struct ttentry {
  hasht lock;
  evalt value;
  movet best_move;
  ui16  depth;
  ui16  flags;
} ttentry;

extern int tt_hit;
extern int tt_false;
extern int tt_deep;
extern int tt_shallow;
extern int tt_used;
extern int tt_used_exact;
extern int tt_used_lower;
extern int tt_used_upper;

int ttable_init();

extern int nodes_searched;

enum make_move_flags {
  invalid_move=1, same_side_moves_again=2,
};

evalt search(board* b, const search_settings* s, const evaluator* e,
             movet* pv, int* pv_length);

#ifdef  __cplusplus
};
#endif/*__cplusplus*/
#endif /* INCLUDED_SEARCH_H_ */
