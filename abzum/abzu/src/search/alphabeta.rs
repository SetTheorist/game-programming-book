use super::*;
use crate::{*};
use crate::tt;
use crate::pv::PV;
use crate::record::Ply;
use crate::time;

pub fn search_alphabeta<G:Game>(
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
  let val = search_alphabeta_general::<G>(b, e,
    G::V::MIN, G::V::MAX, settings.depth, 0,
    pv, tt, settings, stats);
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
    annotation:"alphabeta".into(),
  }
}

fn search_alphabeta_general<G:Game>(
  b:&mut G::B, e:&G::E,
  α:G::V, β:G::V,
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

  let ml = b.gen_moves();

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
        // validate "best move" from tt
        for &mm in &ml {
          if mm==ttv.best_move {
            tt.s.used += 1;
            pv.pv[ply][ply] = ttv.best_move;
            pv.length[ply] = 1;
            return ttv.value;
          }
        }
      }
    }
  }

  let mut α = α;
  let mut made_valid_move = false;
  let mut best_val = G::V::MIN;
  for &m in &ml {
    let val : G::V =
      match b.make_move(m) {
        MoveResult::InvalidMove => { continue; }
        MoveResult::SameSideMovesAgain => {
          search_alphabeta_general::<G>(b, e, α, β, depth-100, ply+1, pv, tt, settings, stats)
        } 
        MoveResult::OtherSideMoves => {
          -search_alphabeta_general::<G>(b, e, -β, -α, depth-100, ply+1, pv, tt, settings, stats)
        }
      };
    b.unmake_move(m);
    made_valid_move = true;
    if val > best_val {
      best_val = val;
    }
    if best_val > α {
      α = best_val;
      pv.update(ply, m);
      if α >= β {
        if let Some(TT{}) = settings.tt {
          tt.store(b.hash(), α, m, tt::F_LOWER, depth);
        }
        return α;
      }
    }
  }
  if !made_valid_move {
    // stalemate: no valid moves
    pv.length[ply] = 0;
    return e.stalemate_relative(b, ply);
  } else if let Some(TT{}) = settings.tt {
    tt.store(b.hash(), best_val, pv.pv[ply][ply], tt::F_EXACT, depth);
  }
  return α;
}

fn search_alphabeta_quiescence<G:Game>(
  b:&mut G::B, e:&G::E,
  α:G::V, β:G::V,
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

  let ml = b.gen_moves();

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
        // validate "best move" from tt
        for &mm in &ml {
          if mm==ttv.best_move {
            tt.s.used += 1;
            pv.pv[ply][ply] = ttv.best_move;
            pv.length[ply] = 1;
            return ttv.value;
          }
        }
      }
    }
  }

  let mut α = α;
  let mut made_valid_move = false;
  let mut best_val = G::V::MIN;
  for &m in &ml {
    let val : G::V =
      match b.make_move(m) {
        MoveResult::InvalidMove => { continue; }
        MoveResult::SameSideMovesAgain => {
          search_alphabeta_general::<G>(b, e, α, β, depth-100, ply+1, pv, tt, settings, stats)
        } 
        MoveResult::OtherSideMoves => {
          -search_alphabeta_general::<G>(b, e, -β, -α, depth-100, ply+1, pv, tt, settings, stats)
        }
      };
    b.unmake_move(m);
    made_valid_move = true;
    if val > best_val {
      best_val = val;
    }
    if best_val > α {
      α = best_val;
      pv.update(ply, m);
      if α >= β {
        if let Some(TT{}) = settings.tt {
          tt.store(b.hash(), α, m, tt::F_LOWER, depth);
        }
        return α;
      }
    }
  }
  if !made_valid_move {
    // stalemate: no valid moves
    pv.length[ply] = 0;
    return e.stalemate_relative(b, ply);
  } else if let Some(TT{}) = settings.tt {
    tt.store(b.hash(), best_val, pv.pv[ply][ply], tt::F_EXACT, depth);
  }
  return α;
}
