use super::*;
use crate::{*};
use crate::tt;
use crate::pv::PV;
use crate::record::Ply;
use crate::time;

/*
pub struct Ply<G:Game> {
  pub side: G::B::C,
  pub evaluation: G::V,
  pub m: G::M,
  pub pv: Vec<G::M>,
  pub time_spent_cs: u64,
  pub nodes_searched: u64,
  pub qnodes_searched: u64,
  pub nodes_evaluated: u64,
  pub qnodes_evaluated: u64,
  // pub sm: special_move,
  pub annotation: String,
  }
*/

pub fn search_negamax<G:Game>(
  b:&mut G::B, e:&G::E,
  pv:&mut PV<G::M>,
  tt:&mut tt::Table<G::V,G::M>,
  settings:&Settings<G::V>,
  stats:&mut Stats
  )
  -> Ply<G>
{
  pv.init();
  stats.init();
  tt.init();

  let t0 = time::now();
  let val = search_negamax_general::<G>(b, e, settings.depth, 0, pv, tt, settings, stats);
  let since = time::since_cs(t0);

  Ply {
    side: b.to_move(),
    evaluation: val,
    m: pv.pv[0][0],
    pv: pv.pv[0][0..pv.length[0]].to_vec(),
    time_spent_cs: since,
    nodes_searched: stats.nodes_searched,
    qnodes_searched: stats.qnodes_searched,
    nodes_evaluated: 0,
    qnodes_evaluated: 0,
    annotation:"negamax".into(),
  }
}

fn search_negamax_general<G:Game>(
  b:&mut G::B, e:&G::E,
  depth:i16, ply:usize,
  pv:&mut PV<G::M>,
  tt:&mut tt::Table<G::V,G::M>,
  settings:&Settings<G::V>,
  stats:&mut Stats
  )
  -> G::V
{
  stats.nodes_searched += 1;

  // Stop search
  if b.terminal() || depth <= 0 {
    pv.length[ply] = 0;
    return e.evaluate_relative(b, ply);
  }

  // Transition table
  if let Some(TT{}) = settings.tt {
    if let Some(ttv) = tt.retrieve(b.hash()) {
      let skip;
      if ttv.depth < depth {
        tt.s.shallow += 1;
        skip = true;
      } else if ttv.depth > depth {
        tt.s.deep += 1;
        skip = false;
      } else {
        skip = false;
      }
      if !skip {
        tt.s.used += 1;
        pv.pv[ply][ply] = ttv.best_move;
        pv.length[ply] = 1;
        return ttv.value;
      }
    }
  }

  let mut made_valid_move = false;
  let mut best_val = G::V::MIN;
  let ml = b.gen_moves();
  for &m in &ml {
    let val : G::V =
      match b.make_move(m) {
        MoveResult::InvalidMove => { continue; }
        MoveResult::SameSideMovesAgain => {
          search_negamax_general::<G>(b, e, depth-100, ply+1, pv, tt, settings, stats)
        } 
        MoveResult::OtherSideMoves => {
          -search_negamax_general::<G>(b, e, depth-100, ply+1, pv, tt, settings, stats)
        }
      };
    b.unmake_move(m);
    made_valid_move = true;
    if val > best_val {
      best_val = val;
      pv.update(ply, m);
    }
  }
  if !made_valid_move {
    // stalemate: no valid moves
    pv.length[ply] = 0;
    return e.stalemate_relative(b, ply);
  } else if let Some(TT{}) = settings.tt {
    tt.store(b.hash(), best_val, pv.pv[ply][ply], tt::F_EXACT, depth);
  }
  return best_val;
}


