use std::fmt::{Debug};
use std::ops::{Neg, Not};

////////////////////////////////////////////////////////////

#[derive(Clone,Copy,Debug,Eq,PartialEq)]
pub enum MoveResult {
    Error, OtherSide, SameSide
}

#[derive(Clone,Copy,Debug,Eq,PartialEq)]
pub enum GameState {
    Playing, WhiteWin, BlackWin, Draw, NoResult
}

pub trait Board : Debug {
    type Move : Clone+Copy+Debug+Default+Eq+PartialEq;
    type Eval : Clone+Copy+Debug+Default+Eq+Neg<Output=Self::Eval>+Ord+PartialEq+PartialOrd;

    fn new() -> Self;
    fn from_fen(s: &str) -> Self;
    fn to_fen(&self) -> String;
    fn init(&mut self);
    fn make_move(&mut self, m: Self::Move) -> MoveResult;
    fn unmake_move(&mut self, m: Self::Move) -> MoveResult;
    fn hash(&self) -> Hash;
    fn moves(&self, ml: &mut [Self::Move]) -> usize;
    fn evaluate(&self, ply:usize) -> Self::Eval;
    fn state(&self) -> GameState;
}

////////////////////////////////////////////////////////////

pub type Hash = u64;

const TT_VALID : i32 = 1;
const TT_EXACT : i32 = 2;
const TT_UPPER : i32 = 4;
const TT_LOWER : i32 = 8;
const TTABLE_ENTRIES : usize = 1024 * 1024;

#[derive(Clone,Copy,Debug,Default)]
pub struct TTEntry<E:Clone+Copy+Debug+Default,M:Clone+Copy+Debug+Default> {
    hash: u32,
    depth: i32,
    flags: i32,
    score: E,
    best_move: M,
}

pub struct TTable<B:Board> {
    pub tt_hits: u64,
    pub tt_false: u64,
    pub tt_deep: u64,
    pub tt_shallow: u64,
    pub tt_used: u64,
    pub ttable: Vec<TTEntry<B::Eval,B::Move>>,
}
impl<B:Board> TTable<B> {
    pub fn new() -> Self {
        TTable {
            tt_hits: 0,
            tt_false: 0,
            tt_deep: 0,
            tt_shallow: 0,
            tt_used: 0,
            ttable: vec![TTEntry::default(); TTABLE_ENTRIES],
        }
    }
    pub fn put(&mut self, hash: Hash, score: B::Eval, best_move: B::Move, flags: i32, depth: i32) {
        let loc = (hash as usize) % TTABLE_ENTRIES;
        self.ttable[loc] = TTEntry {
            hash: (hash>>32) as u32,
            score: score,
            flags: flags|TT_VALID,
            best_move: best_move,
            depth: depth,
        };
    }
    pub fn get(&mut self, hash: Hash) -> &TTEntry<B::Eval,B::Move> {
        let loc = (hash as usize) % TTABLE_ENTRIES;
        &self.ttable[loc]
    }
    pub fn get_mut(&mut self, hash: Hash) -> &mut TTEntry<B::Eval,B::Move> {
        let loc = (hash as usize) % TTABLE_ENTRIES;
        &mut self.ttable[loc]
    }
}

////////////////////////////////////////////////////////////

const MAXPV : usize = 64;

pub struct AlphaBetaSearcher<B:Board> {
    pub pv_length: [usize; MAXPV],
    pub pv: [[B::Move; MAXPV]; MAXPV],
    pub b: B,
    pub ttable: TTable<B>,
    pub use_tt: bool,
    pub nodes_searched: u64,
}
impl<B:Board> AlphaBetaSearcher<B> {
    pub fn new() -> AlphaBetaSearcher<B> {
        AlphaBetaSearcher {
            pv_length: [0;MAXPV],
            pv: [[B::Move::default();MAXPV];MAXPV],
            b: B::new(),
            ttable: TTable::new(),
            use_tt: true,
            nodes_searched: 0,
        }
    }
    pub fn alpha_beta(&mut self, ply:usize, depth:i32, alpha:B::Eval, beta:B::Eval, quiescence:i32)
        -> B::Eval
    {
        self.nodes_searched += 1;
        if self.b.state()!=GameState::Playing {
            self.pv_length[ply] = 0;
            return self.b.evaluate(ply);
        }
        if depth<=0 {
            self.pv_length[ply] = 0;
            // TODO: quiescence
            return self.b.evaluate(ply);
        }

        let h = self.b.hash();
        let mut tt_move : Option<B::Move> = None;
        if self.use_tt {
            let &tt = self.ttable.get(h);
            if tt.flags&TT_VALID != 0 {
                self.ttable.tt_hits += 1;
                if tt.hash != (h>>32) as u32 {
                    self.ttable.tt_false += 1;
                } else if tt.depth < depth {
                    self.ttable.tt_shallow += 1;
                    tt_move = Some(tt.best_move);
                } else if tt.depth > depth {
                    self.ttable.tt_deep += 1;
                    tt_move = Some(tt.best_move);
                } else if tt.flags&TT_EXACT != 0 {
                    self.ttable.tt_used += 1;
                    self.pv[ply][0] = tt.best_move;
                    self.pv_length[ply] = 1;
                    return tt.score;
                }
            }
        }

        let mut ml = [B::Move::default(); 256];
        let n = self.b.moves(&mut ml);
        if n <= 0 {
            println!("#No moves: ply={} depth={} alpha={:?} beta={:?} q={} fen={} | {:?}#\n",
                ply, depth, alpha, beta, quiescence, self.b.to_fen(), self.b);
            self.pv_length[ply] = 0;
            return self.b.evaluate(ply);
        }

        // if use_tt_move
        if self.use_tt {
            if let Some(mtt) = tt_move {
                if let Some(i) = ml[0..n].iter().position(|&m|m==mtt) {
                    ml.swap(0, i);
                }
            }
        }

        let mut alpha = alpha;
        let mut store_tt = false;
        self.pv[ply][0] = ml[0];
        for &m in ml[0..n].iter() {
            self.b.make_move(m);
            let v = -self.alpha_beta(ply+1, depth-100, -beta, -alpha, quiescence);
            self.b.unmake_move(m);

            if v > alpha {
                store_tt = true;
                alpha = v;
                if alpha >= beta {
                    return alpha;
                }
                {
                    let l = self.pv_length[ply+1];
                    let (x,y) = self.pv.split_at_mut(ply+1);
                    x[ply][1..(l+1)].clone_from_slice(&y[0][0..l]);
                }
                self.pv[ply][0] = m;
                self.pv_length[ply] = self.pv_length[ply+1]+1;
            }
        }

        if self.use_tt && store_tt && (alpha < beta) {
            self.ttable.put(h, alpha, self.pv[ply][0], TT_EXACT, depth);
        }
        return alpha;
    }
}
