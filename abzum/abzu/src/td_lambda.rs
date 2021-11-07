use crate::{Evaluator,Game};

#[derive(Clone,Debug)]
pub struct TDLambda {
  pub num_weights: usize,
  pub lambda: f32, /* temporal decay */
  pub alpha: f32,  /* learning speed */
  pub beta: f32,   /* transform scaling */
  pub h: f32,      /* bump for partial approximation */

  /* temporal coherence */
  pub tc: bool,
  pub net_change: Vec<f32>,
  pub abs_change: Vec<f32>,
  pub alpha_tc: Vec<f32>,
}

impl TDLambda {
  pub fn new(num_weights:usize, lambda:f32, alpha:f32, beta:f32, h:f32, tc:bool) -> Self {
    let tcn = if tc {num_weights} else {0};
    TDLambda {
      num_weights, lambda, alpha, beta, h,
      tc,
      net_change: vec![0.0;tcn],
      abs_change: vec![0.0;tcn],
      alpha_tc: vec![1.0;tcn],
    }
  }

  #[inline]
  fn eval_transform(&self, eval:f32) -> f32 {
    (self.beta * eval).tanh()
  }

  fn compute_weight_partial<G:Game>(&self, b:&G::B, e:&mut G::E, weights:&[f32], partials:&mut[f32]) {
    for j in 0..self.num_weights {
      let wj = weights[j];

      e.set_weight_f32(j, wj + self.h);
      let pev = e.evaluate_absolute(b, 0);
      let pv = self.eval_transform(pev.into());

      e.set_weight_f32(j, wj - self.h);
      let mev = e.evaluate_absolute(b, 0);
      let mv = self.eval_transform(mev.into());

      e.set_weight_f32(j, wj);
      partials[j] = (pv - mv) / ((wj+self.h)-(wj-self.h));
    }
  }
}
