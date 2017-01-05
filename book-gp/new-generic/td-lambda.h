#ifndef INCLUDED__TD_LAMBDA_H_
#define INCLUDED__TD_LAMBDA_H_

#include "player.h"
#include "util.h"

/* ************************************************************ */
/* TD(lambda) learning */
/* ************************************************************ */

template<typename Game>
struct TD_Lambda
{
  typedef typename Game::boardt     boardt;
  typedef typename Game::evalt      evalt;
  typedef typename Game::evaluatort evaluatort;

  int     num_weights;

  double  lambda; /* temporal decay */
  double  alpha;  /* learning speed */
  double  beta;   /* transform scaling */
  double  h;      /* bump for partial approximationn */

  /* temporal coherence */
  int     tc;
  double* net_change;
  double* abs_change;
  double* alpha_tc;

  TD_Lambda(int num_weights_, double lambda_, double alpha_=1.0, double beta_=0.50/100, double h_=1.00, int tc_=1)
      : num_weights(num_weights_),
        lambda(lambda_), alpha(alpha_), beta(beta_), h(h_), tc(tc_),
        net_change(tc ? new double[num_weights] : 0),
        abs_change(tc ? new double[num_weights] : 0),
        alpha_tc(tc ? new double[num_weights] : 0)
  {
    if (tc) {
      memset(net_change, '\x00', num_weights*sizeof(double));
      memset(abs_change, '\x00', num_weights*sizeof(double));
      for (int i=0; i<num_weights; ++i)
        alpha_tc[i] = 1.0;
    }
  }
  ~TD_Lambda()
  {
    if (tc) {
      delete[] net_change;
      delete[] abs_change;
      delete[] alpha_tc;
    }
  }
  TD_Lambda(const TD_Lambda& tdl)
      : num_weights(tdl.num_weights),
        lambda(tdl.lambda), alpha(tdl.alpha), beta(tdl.beta), h(tdl.h), tc(tdl.tc),
        net_change(tc ? new double[num_weights] : 0),
        abs_change(tc ? new double[num_weights] : 0),
        alpha_tc(tc ? new double[num_weights] : 0)
  {
    if (tc) {
      memcpy(net_change, tdl.net_change, num_weights*sizeof(double));
      memcpy(abs_change, tdl.abs_change, num_weights*sizeof(double));
      memcpy(alpha_tc, tdl.alpha_tc, num_weights*sizeof(double));
    }
  }
  const TD_Lambda& operator = (const TD_Lambda& tdl)
  {
    if (this == &tdl) return *this;
    if (tc) {
      delete[] net_change;
      delete[] abs_change;
      delete[] alpha_tc;
    }
    num_weights = tdl.num_weight;
    lambda = tdl.lambda;
    alpha = tdl.alpha;
    beta = tdl.beta;
    h = tdl.h;
    tc = tdl.tc;
    net_change = tc ? new double[num_weights] : 0;
    abs_change = tc ? new double[num_weights] : 0;
    alpha_tc = tc ? new double[num_weights] : 0;
    if (tc) {
      memcpy(net_change, tdl.net_change, num_weights*sizeof(double));
      memcpy(abs_change, tdl.abs_change, num_weights*sizeof(double));
      memcpy(alpha_tc, tdl.alpha_tc, num_weights*sizeof(double));
    }
    return *this;
  }

  inline double eval_transform(double eval)
  {
    return tanh(beta * eval);
  }

  void compute_weight_partials(const boardt* b, int num_weights, const double weights[], double partials[], evaluatort* evaluator)
  {
    for (int j=0; j<num_weights; ++j)
    {
      const double wj = weights[j];

      evaluator->set_weight_double(j, wj + h);
      evalt pev = evaluator->compute_evaluation_absolute(b, 1, evalt::B_INF, evalt::W_INF);
      double pv = eval_transform(pev);

      evaluator->set_weight_double(j, wj - h);
      evalt nev = evaluator->compute_evaluation_absolute(b, 1, evalt::B_INF, evalt::W_INF);
      double nv = eval_transform(nev);

      evaluator->set_weight_double(j, wj);

      partials[j] = (pv - nv) / (2*h);
    }
  }

  int process_game(const GameRecord<Game>* gr, evaluatort* evaluator, int /*side*/)
  {
    int num_states = gr->move_list.size() + 2;
    int num_weights = evaluator->num_weights();

    double  weights[num_weights];
    double  new_weights[num_weights];
    double  evals[num_states];
    double  evals_xform[num_states];
    double  diffs[num_states];
    double  weight_partials[num_states][num_weights]; // NB this can blow the stack for long games / many weights...

    evaluator->get_weights_double(weights);

    memset(new_weights,     '\x00', sizeof(new_weights));
    memset(evals,           '\x00', sizeof(evals));
    memset(evals_xform,     '\x00', sizeof(evals_xform));
    memset(diffs,           '\x00', sizeof(diffs));
    memset(weight_partials, '\x00', sizeof(weight_partials));

    switch (gr->result)
    {
      case Game::first:  evals_xform[num_states-1] = evals_xform[num_states-2] = +1.0; break;
      case Game::second: evals_xform[num_states-1] = evals_xform[num_states-2] = -1.0; break;
      default:           evals_xform[num_states-1] = evals_xform[num_states-2] =  0.0; break;
    }

    boardt b = gr->initial_board;

    for (int i=0; i<num_states-2; ++i) {
      /* get board at end of pv */
      b.make_moves(gr->move_list[i].pv_length, gr->move_list[i].pv);

      /* evaluate board at end-of-pv */
      evals[i] = evaluator->compute_evaluation_absolute(&b, 1, evalt::B_INF, evalt::W_INF);
      evals_xform[i] = eval_transform(evals[i]);

      /* get partial-derivative wrt weights */
      compute_weight_partials(&b, num_weights, weights, weight_partials[i], evaluator);

      /* undo all moves except the first */
      b.unmake_moves(gr->move_list[i].pv_length-1);
    }

    for (int i=0; i<num_states-2; ++i) {
      diffs[i] = evals_xform[i+2] - evals_xform[i];
    }

    // TODO: optimize lambda-powers / tail-sums... (see chu-shogi)
    for (int j=0; j<num_weights; ++j) {
      const double alpha_j = alpha*(tc ? alpha_tc[j] : 1.0);
      double  sum = 0.0, abs_sum = 0.0;;
      for (int i=0; i<num_states-2; ++i) {
        double lam_sum = 0.0;
        for (int m=i; m<num_states-2; m+=2)
          lam_sum += pow(lambda, (m-i)/2)*diffs[m];
        sum += weight_partials[i][j] * lam_sum;
        if (tc) abs_sum += fabs(weight_partials[i][j] * lam_sum);
      }
      new_weights[j] = weights[j] + alpha_j * sum;
      if (tc) {
        net_change[j] += sum;
        abs_change[j] += abs_sum;
        alpha_tc[j] = (abs_change[j]<1e-12) ? 1.0 : fabs(net_change[j])/abs_change[j];
      }
    }
    evaluator->set_weights_double(new_weights);

    return 0;
  }

  int showf(FILE* ff) const
  {
    int n = 0;
    n += fprintf(ff, "# alpha=%g beta=%g lambda=%g h=%g\n", alpha, beta, lambda, h);
    if (tc) {
      for (int i=0; i<num_weights; ++i) {
        n += fprintf(ff, "%g %g %g", net_change[i], abs_change[i], alpha_tc[i]);
        if (i<num_weights-1) n += fprintf(ff, "   ");
        if (i%5==4) n += fprintf(ff, "\n");
      }
      if (num_weights%5) n += fprintf(ff, "\n");
    }
    return n;
  }
};

#endif/*INCLUDED__TD_LAMBDA_H_*/
