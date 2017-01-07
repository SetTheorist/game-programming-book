use std::fmt::{Debug};
use std::ops::{Neg, Not};

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
    type Eval : Clone+Copy+Debug+Default+Eq+Neg<Output=Self::Eval>+Ord+PartialEq+PartialOrd;

    fn new() -> Self;
    fn from_fen(s: &str) -> Self;
    fn to_fen(&self) -> String;
    fn init(&mut self);
    fn make_move(&mut self, m: Self::Move) -> MoveResult;
    fn unmake_move(&mut self, m: Self::Move) -> MoveResult;
    fn hash(&self) -> Hash;
    fn moves(&self, &mut [Self::Move]) -> usize;
    fn evaluate(&self, ply:usize) -> Self::Eval;
    fn state(&self) -> GameState;
}

////////////////////////////////////////////////////////////

struct AlphaBetaSearcher<B:Board> {
    pv_length: [usize; 64],
    pv: [[B::Move; 64]; 64],
    b: B,
}
impl<B:Board> AlphaBetaSearcher<B> {
    fn new() -> AlphaBetaSearcher<B> {
        AlphaBetaSearcher { pv_length:[0;64], pv:[[B::Move::default();64];64], b:B::new() }
    }
    fn alpha_beta(&mut self, ply:usize, depth:i32, alpha:B::Eval, beta:B::Eval, quiescence:i32)
        -> B::Eval
    {
        let mut alpha = alpha;
        if self.b.state()!=GameState::Playing {
            self.pv_length[ply] = 0;
            return self.b.evaluate(ply);
        }
        if depth<=0 {
            self.pv_length[ply] = 0;
            // TODO: quiescence
            return self.b.evaluate(ply);
        }
        // TODO: ttlookup
        let mut ml = [B::Move::default(); 256];
        let n = self.b.moves(&mut ml);
        if n <= 0 {
            println!("#No moves: ply={} depth={} alpha={:?} beta={:?} q={} fen={} {:?}#\n",
                ply, depth, alpha, beta, quiescence, self.b.to_fen(), self.b.state());
            self.pv_length[ply] = 0;
            return self.b.evaluate(ply);
        }
        // TODO: ttmove ordering
        let mut store_tt = false;
        self.pv[ply][0] = ml[0];
        for &m in ml.iter() {
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

        if store_tt && (alpha < beta) {
            // put_ttable...
        }
        return alpha;
    }
}

////////////////////////////////////////////////////////////

#[derive(Clone,Copy,Debug,Default,Eq,PartialEq)]
struct MoveTTT(u8);
#[derive(Clone,Copy,Debug,Default,Eq,Ord,PartialEq,PartialOrd)]
struct EvalTTT(i32);
impl Neg for EvalTTT {
    type Output = EvalTTT;
    fn neg(self) -> EvalTTT { EvalTTT(-self.0) }
}
#[derive(Clone,Copy,Debug,Eq,Ord,PartialEq,PartialOrd)]
enum SideTTT { X, O }
impl Not for SideTTT {
    type Output = SideTTT;
    fn not(self) -> SideTTT {
        match self { SideTTT::X=>SideTTT::O, SideTTT::O=>SideTTT::X }
    }
}
#[derive(Debug)]
struct BoardTTT {
    state: GameState,
    to_play: SideTTT,
    board: [Option<SideTTT>; 9],
}
impl BoardTTT {
    pub fn show(&self) {
        for i in 0..9 {
            print!("{}",
                match self.board[i] {
                    None => '.',
                    Some(SideTTT::X) => 'X',
                    Some(SideTTT::O) => 'O',
                }
            );
            if (i+1)%3==0 { println!(""); }
        }
    }
    pub fn state(&self) -> GameState {
        for &(a,b,c) in &[(0,1,2),(3,4,5),(6,7,8),(0,3,6),(1,4,7),(2,5,8),(0,4,8),(2,4,6)] {
            if self.board[a]==self.board[b] && self.board[b]==self.board[c] {
                match self.board[a] {
                    Some(SideTTT::X) => { return GameState::WhiteWin }
                    Some(SideTTT::O) => { return GameState::BlackWin }
                    None => {}
                }
            }
        }
        if self.board.iter().all(|x|x.is_some()) {
            return GameState::Draw;
        } else {
            return GameState::Playing;
        }
    }
}
impl Board for BoardTTT {
    type Move = MoveTTT;
    type Eval = EvalTTT;
    fn new() -> Self {
        BoardTTT {
            state:GameState::Playing,
            to_play:SideTTT::X,
            board:[None;9],
        }
    }
    fn from_fen(_s: &str) -> Self {
        Self::new()
    }
    fn to_fen(&self) -> String {
        let mut s = String::new();
        for &t in self.board.iter() {
            s.push(
                match t {
                    None => '.',
                    Some(SideTTT::X) => 'X',
                    Some(SideTTT::O) => 'O',
                }
            );
        }
        s
    }
    fn init(&mut self) {
        *self = Self::new();
    }
    fn make_move(&mut self, m: Self::Move) -> MoveResult {
        self.board[m.0 as usize] = Some(self.to_play);
        self.to_play = !self.to_play;
        self.state = self.state();
        MoveResult::OtherSide
    }
    fn unmake_move(&mut self, m: Self::Move) -> MoveResult {
        self.board[m.0 as usize] = None;
        self.to_play = !self.to_play;
        self.state = self.state();
        //self.state = GameState::Playing;
        MoveResult::OtherSide
    }
    fn hash(&self) -> Hash {
        let mut h = 0;
        for &t in self.board.iter() {
            match t {
                None => { h = h*13; }
                Some(SideTTT::X) => { h = h*13+3; }
                Some(SideTTT::O) => { h = h*13+7; }
            }
        }
        h
    }
    fn moves(&self, ml: &mut [Self::Move]) -> usize {
        let mut n = 0;
        for i in 0..9 {
            if self.board[i].is_none() {
                ml[n] = MoveTTT(i as u8);
                n += 1;
            }
        }
        n
    }
    fn evaluate(&self, ply:usize) -> Self::Eval {
        let v = match self.state {
            GameState::WhiteWin =>  EvalTTT(1000000),
            GameState::BlackWin => -EvalTTT(1000000),
            _ => EvalTTT(0),
        };
        if self.to_play==SideTTT::X {v} else {-v}
    }
    fn state(&self) -> GameState {
        GameState::Playing
    }
}


////////////////////////////////////////////////////////////

fn main() {
    let mut ab = AlphaBetaSearcher::<BoardTTT>::new();
    println!("{:?}", ab.b);
    while ab.b.state == GameState::Playing {
        println!("====================");
        ab.b.show();
        println!("{:?}", ab.b.to_fen());
        let mut ml = [MoveTTT::default(); 10];
        let n = ab.b.moves(&mut ml);
        println!("{:?} {:?}", n, &ml[0..n]);

        let v = ab.alpha_beta(0, 300, EvalTTT(-1000000), EvalTTT(1000000), 0);
        println!("{:?} {:?}", v, &ab.pv[0][0..(ab.pv_length[0])]);
        ab.b.show();
        let m = ml[(19+n/2)%n];
        println!("{:?} ==> {:?}", ab.b.to_play, m);
        ab.b.make_move(m);
    }
    println!("--------------------");
    ab.b.show();
    println!("{:?}", ab.b);
}

