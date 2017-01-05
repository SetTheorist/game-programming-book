#include <ctype.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "board.h"
#include "evaluate.h"
#include "player.h"
#include "td-lambda.h"

/* ************************************************************ */
/* TD(lambda) learning */
/* ************************************************************ */

static double eval_transform(double eval, double beta)
{
  return tanh(beta * eval);
}
static void compute_weight_partials(const board* b, const evaluator* e, double beta, double h, ui8 tune_flag[], double partials[])
{
  int i;
  for (i=0; i<NUM_WEIGHTS; ++i)
  {
    if (!tune_flag[i]) continue;
    const double  save_weight = e->weights[i];
    double  pv, nv;

    ((evaluator*)e)->weights[i] = save_weight + h;
    pv = eval_transform(evaluate(e, b, 1, B_INF, W_INF), beta);

    ((evaluator*)e)->weights[i] = save_weight - h;
    nv = eval_transform(evaluate(e, b, 1, B_INF, W_INF), beta);

    ((evaluator*)e)->weights[i] = save_weight;

    partials[i] = (pv - nv) / (2*h);
  }
}

int td_lambda_process_game( const game_record* g, td_lambda* tdl, const evaluator* e, evaluator* new_e )
{
  int i, j;
  int num_states = g->ply + 2;

  double  evals[num_states];
  double  evals_xform[num_states];
  double  diffs[num_states];
  double  *weight_partials = (double*)malloc(num_states*NUM_WEIGHTS*sizeof(double));
  memset(weight_partials, '\x00', num_states*NUM_WEIGHTS*sizeof(double));

  memcpy(new_e, e, sizeof(evaluator));

  switch (g->result)
  {
    case white: evals_xform[num_states-1] = evals_xform[num_states-2] = +1.0; break;
    case black: evals_xform[num_states-1] = evals_xform[num_states-2] = -1.0; break;
    default:    evals_xform[num_states-1] = evals_xform[num_states-2] =  0.0; break;
  }

  board b;
  init_board(&b);
  for (i=0; i<num_states-2; ++i)
  {
    int j;
    const int num_moves = g->plies[i].pv_length;

    /* get board at end of pv */
    for (j=0; j<num_moves; ++j) make_move(&b, g->pv_heap[g->plies[i].pv_index+j]);

    /* evaluate board at end-of-pv */
    evals[i] = evaluate(e, &b, 1, B_INF, W_INF);
    evals_xform[i] = eval_transform(evals[i], tdl->beta);

    /* get partial-derivative wrt weights */
    compute_weight_partials(&b, e, tdl->beta, tdl->h, tdl->tune_flag, weight_partials+i*NUM_WEIGHTS);

    /* undo all moves except the first */
    for (j=num_moves-1; j>0; --j) unmake_move(&b);
  }

  for (i=0; i<num_states-2; ++i)
    diffs[i] = evals_xform[i+2] - evals_xform[i];

  /* lam_pow[i] = lambda^i */
  double lam_pow[num_states-2];
  for (i=0; i<num_states-2; ++i)
    lam_pow[i] = pow(tdl->lambda, i);

  /* lam_pow_diffs[i] = lambda^i * diffs[i] */
  double lam_pow_diffs[num_states-2];
  for (i=0; i<num_states-2; ++i)
    lam_pow_diffs[i] = lam_pow[i] * diffs[i];

  /* tail_sum[i] = \sum_{m=i (by 2)}^{num_states-2} lam_pow_diffs[m] = \sum_{m=i (by 2)}^{num_states-2} lambda^m * diffs[m] */
  double tail_sum[num_states];
  tail_sum[num_states-1] = tail_sum[num_states-2] = 0.0;
  for (i=num_states-3; i>=0; --i)
    tail_sum[i] = tail_sum[i+2] + lam_pow_diffs[i];

  for (j=0; j<NUM_WEIGHTS; ++j)
  {
    if (!tdl->tune_flag[j]) {
      new_e->weights[j] = e->weights[j];
    } else {
      const double  alpha = tdl->use_temporal_coherence ? (tdl->alpha*tdl->alpha_tc[j]) : tdl->alpha;
      double  sum = 0.0, abs_sum = 0.0;
      for (i=0; i<num_states-2; ++i)
      {
        double lam_sum = 0.0;
        //int m;
        //for (m=i; m<num_states-2; m+=2)
        //  lam_sum += lam_pow[m-i]*diffs[m];
        lam_sum = tail_sum[i] / lam_pow[i/2];
        sum += weight_partials[i*NUM_WEIGHTS+j] * lam_sum;
        abs_sum += fabs(weight_partials[i*NUM_WEIGHTS+j] * lam_sum);
      }
      new_e->weights[j] = e->weights[j] + alpha * sum;
      tdl->net_change[j] += sum;
      tdl->abs_change[j] += abs_sum;
      tdl->alpha_tc[j] = (fabs(tdl->abs_change[j]) < 1e-12) ? 1.0 : (fabs(tdl->net_change[j])/tdl->abs_change[j]);
    }
  }

  free(weight_partials);
  return 0;
}

int init_td_lambda(td_lambda* tdl, double lambda, double beta, double alpha, double h, int use_temporal_coherence)
{
  memset(tdl, '\x00', sizeof(td_lambda));
  tdl->lambda = lambda;
  tdl->beta = beta;
  tdl->alpha = alpha;
  tdl->h = h;
  memset(tdl->tune_flag, '\x01', sizeof(tdl->tune_flag));

  tdl->use_temporal_coherence = use_temporal_coherence;
  int i;
  if (use_temporal_coherence)
    for (i=0; i<NUM_WEIGHTS; ++i)
      tdl->alpha_tc[i] = 1.0;
  return 0;
}

int show_td_lambda(FILE* f, const td_lambda* tdl)
{
  int n = 0;
  n += fprintf(f, "{lambda=%.4f;", tdl->lambda);
  n += fprintf(f, "beta=%.4f;", tdl->beta);
  n += fprintf(f, "alpha=%.4f;", tdl->alpha);
  n += fprintf(f, "TC=%i;", tdl->use_temporal_coherence);
  {
    int i;
    n += fprintf(f, "\n\t");
    for (i=0; i<NUM_WEIGHTS; ++i)
    {
      n += fprintf(f, "%.2f|%.2f|%.2f  ", tdl->net_change[i], tdl->abs_change[i], tdl->alpha_tc[i]);
      if (!((i+1)%10) && i!=NUM_WEIGHTS-1) n += fprintf(f, "\n\t");
    }
  }
  n += fprintf(f, "}\n");
  return n;
}

