use super::*;
use crate::{*};
use crate::tt;

pub fn search_negamax<G:Game>(
  b:&mut G::B, e:&G::E, pv:&mut PV<G::M>,
  tt:&mut tt::Table<G::V,G::M>,
  settings:&Settings<G::V>,
  stats:&mut Stats
  )
  -> (G::V, Vec<G::M>)
{
  tt.init();
  stats.nodes_searched = 1;
  let ml = b.gen_moves();
  if ml.len() == 0 {
    // TODO: stalemate is loss ?!
    return (G::V::MIN, vec![]);
  }

  let mut best_val = G::V::MIN;
  let mut made_valid_move = false;
  for &m in &ml {
    let v : G::V =
      match b.make_move(m) {
        MoveResult::InvalidMove => { continue; }
        MoveResult::SameSideMovesAgain => {
          search_negamax_general::<G>(b, e, settings.depth-100, 1, pv, tt, settings, stats)
        } 
        MoveResult::OtherSideMoves => {
          -search_negamax_general::<G>(b, e, settings.depth-100, 1, pv, tt, settings, stats)
        }
      };
    b.unmake_move(m);
    made_valid_move = true;
    if v > best_val {
      best_val = v;
      pv.update(0, m);
    }
  }

  if !made_valid_move {
    // TODO: stalemate is loss ?!
    return (G::V::MIN, vec![]);
  }

  (best_val, pv.pv[0][0..pv.length[0]].to_vec())
}

pub fn search_negamax_general<G:Game>(
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
        stats.tt.shallow += 1;
        skip = true;
      } else if ttv.depth > depth {
        stats.tt.deep += 1;
        skip = false;
      } else {
        skip = false;
      }
      if !skip {
        stats.tt.used += 1;
        pv.pv[ply][ply] = ttv.best_move;
        pv.length[ply] = 1;
        return ttv.value;
      }
    }
  }
  let mut best_val = G::V::MIN;
  let ml = b.gen_moves();
  for &m in &ml {
    let val : G::V =
      match b.make_move(m) {
        MoveResult::InvalidMove => { continue; }
        MoveResult::SameSideMovesAgain => {
          search_negamax_general::<G>(b, e, settings.depth-100, ply+1, pv, tt, settings, stats)
        } 
        MoveResult::OtherSideMoves => {
          -search_negamax_general::<G>(b, e, settings.depth-100, ply+1, pv, tt, settings, stats)
        }
      };
    b.unmake_move(m);
    if val > best_val {
      best_val = val;
      pv.update(ply, m);
    }
  }
  if let Some(TT{}) = settings.tt {
    tt.store(b.hash(), best_val, pv.pv[ply][ply], tt::F_EXACT, depth);
  }
  return best_val;
}



