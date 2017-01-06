use std::fmt::{Debug};
use std::op::{Neg};

////////////////////////////////////////////////////////////

type Hash = u64;

#[derive(Clone,Copy,Debug,Eq,PartialEq)]
enum MoveResult {
    Error, OtherSide, SameSide
}

#[derive(Clone,Copy,Debug,Eq,PartialEq)]
enum GameState {
    Playing, WhiteWin, BlackWin, Draw, NoResult
}


trait Board {
    type Move : Clone+Copy+Debug+Default+Eq+PartialEq;
    type MoveIter : Iterator<Item=Self::Move>;
    type Eval : Clone+Copy+Debug+Default+Eq+Neg+Ord+PartialEq+PartialOrd;

    fn new() -> Self;
    fn init(&mut self);
    fn make_move(&mut self, m: Self::Move) -> MoveResult;
    fn unmake_move(&mut self, m: Self::Move) -> MoveResult;
    fn hash(&self) -> Hash;
    fn moves(&self) -> Self::MoveIter;
    fn evaluate(&self, ply:i32) -> Self::Eval;
    fn state(&self) -> GameState;
}

////////////////////////////////////////////////////////////

struct AlphaBetaSearcher<B:Board> {
    pv_length: [i32; 64],
    pv: [[B::Move; 64]; 64],
    b: B,
}
impl<B:Board> AlphaBetaSearcher<B> {
    fn new() -> AlphaBetaSearcher<B> {
        AlphaBetaSearcher { pv_length:[0;64], pv:[[B::Move::default();64];64], b:B::new() }
    }
    fn alpha_beta(&mut self, ply:i32, depth:i32, alpha:B::Eval, beta:B::Eval, quiescence:i32)
    {
        let mut b = &mut self.b;
        if b.state()!=GameState::Playing {
            self.pv_length[ply] = 0;
            return b.evaluate(ply);
        }
        if depth<=0 {
            self.pv_length[ply] = 0;
            // TODO: quiescence
            return b.evaluate(ply);
        }
        // TODO: ttable
        let mut ms = b.moves();
    }
}

////////////////////////////////////////////////////////////

fn main() {
    println!("Hello, world!");
}

