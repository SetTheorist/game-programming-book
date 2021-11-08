use crate::{Board,Evaluator,Game,GameResult};

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

  pub fn process_game<G:Game>(&mut self, e:&mut G::E, gr:&crate::record::GameRecord<G>) {
    let ns = gr.move_list.len() + 2;
    let nw = e.num_weights();
    let weights = e.get_all_weights_f32();
    let mut new_weights = vec![0.0; nw];
    let mut evals = vec![0.0; ns];
    let mut evals_xform = vec![0.0; ns];
    let mut diffs = vec![0.0; ns];
    let mut weight_partials = vec![vec![0.0; nw]; ns];
    evals_xform[ns-1] =
      match gr.result {
        Some(GameResult::FirstWin) => 1.0,
        Some(GameResult::SecondWin) => -1.0,
        _ => 0.0,
      };
    evals_xform[ns-2] = evals_xform[ns-1];

    let mut b = gr.initial_board.clone();
    for i in 0..(ns-2) {
      // get board at end of pv
      for &m in gr.move_list[i].pv.iter() {
        // validate move
        let ml = b.gen_moves();
        let mut has = false;
        for &mm in ml.iter() {if m==mm {has=true;break;}}
        if !has {
          eprintln!("*** MOVE {} ON PV NOT IN MOVELIST {}", m, gr.move_list[i]);
          eprintln!("{}", b);
          eprintln!("{}", gr);
          panic!("");
        }
        b.make_move(m);
      }

      evals[i] = e.evaluate_absolute(&b, 1).into();
      evals_xform[i] = self.eval_transform(evals[i]);

      // get partial-derivative wrt weights
      self.compute_weight_partial::<G>(&b, e, &weights, &mut weight_partials[i]);

      // undo all moves except the first
      for &m in gr.move_list[i].pv[1..].iter().rev() { b.unmake_move(m); }
    }

    for i in 0..(ns-2) {
      diffs[i] = evals_xform[i+2] - evals_xform[i];
    }
    //println!("{:?}", evals);
    //println!("{:?}", evals_xform);
    //println!("{:?}", diffs);

    // TODO: optimizer lambda-powers/tail-sums (cf chu-shogi)
    for j in 0..nw {
      let αj = self.alpha*(if self.tc {self.alpha_tc[j]} else {1.0});
      let mut sum = 0.0;
      let mut abs_sum = 0.0;
      for i in 0..(ns-2) {
        let mut λsum = 0.0;
        for m in (i..(ns-2)).step_by(2) {
          λsum += self.lambda.powi(((m-i)/2) as i32)*diffs[m];
        }
        sum += weight_partials[i][j] * λsum;
        if self.tc { abs_sum += (weight_partials[i][j] * λsum).abs(); }
        new_weights[j] = weights[j] + αj * sum;
        if self.tc {
          self.net_change[j] += sum;
          self.abs_change[j] += abs_sum;
          self.alpha_tc[j] =
            if self.abs_change[j]<1e-8 {1.0}
            else {self.net_change[j].abs()/self.abs_change[j]};
        }
      }
    }
    for j in 0..nw {
      e.set_weight_f32(j, new_weights[j]);
    }
    e.normalize_weights();
  }
}
