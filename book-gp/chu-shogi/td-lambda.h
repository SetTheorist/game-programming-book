#ifndef    INCLUDED_TD_LAMBDA_H_
#define    INCLUDED_TD_LAMBDA_H_
/* $Id: $ */
#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

#include "board.h"
#include "evaluate.h"
#include "player.h"


typedef struct td_lambda {
  double  lambda; /* decay rate */
  double  alpha;  /* learn rate */

  double  beta;   /* eval transform constant */

  double  h;      /* bump-size for partials approximation */

  ui8     tune_flag[NUM_WEIGHTS]; /* whether to tune individual weights */

  int     use_temporal_coherence;
  double  net_change[NUM_WEIGHTS];
  double  abs_change[NUM_WEIGHTS];
  double  alpha_tc[NUM_WEIGHTS];
} td_lambda;

int init_td_lambda(td_lambda* tdl, double lambda, double beta, double alpha, double h, int use_temporal_coherence);

int td_lambda_process_game( const game_record* g, td_lambda* tdl, const evaluator* e, evaluator* new_e );

int show_td_lambda(FILE* f, const td_lambda* tdl);

#ifdef  __cplusplus
};
#endif/*__cplusplus*/

#endif /* INCLUDED_TD_LAMBDA_H_ */
