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

trait Board : Debug {
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

const TT_VALID : i32 = 1;
const TT_EXACT : i32 = 2;
const TT_UPPER : i32 = 4;
const TT_LOWER : i32 = 8;
const TTABLE_ENTRIES : usize = 1024 * 256;

#[derive(Clone,Copy,Debug,Default)]
struct TTEntry<E:Clone+Copy+Debug+Default,M:Clone+Copy+Debug+Default> {
    hash: u32,
    depth: i32,
    score: E,
    best_move: M,
    flags: i32,
}

struct TTable<B:Board> {
    tt_hits: u64,
    tt_false: u64,
    tt_deep: u64,
    tt_shallow: u64,
    tt_used: u64,
    ttable: Vec<TTEntry<B::Eval,B::Move>>,
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

struct AlphaBetaSearcher<B:Board> {
    pv_length: [usize; MAXPV],
    pv: [[B::Move; MAXPV]; MAXPV],
    b: B,
    ttable: TTable<B>,
    use_tt: bool,
    nodes_searched: u64,
}
impl<B:Board> AlphaBetaSearcher<B> {
    fn new() -> AlphaBetaSearcher<B> {
        AlphaBetaSearcher {
            pv_length: [0;MAXPV],
            pv: [[B::Move::default();MAXPV];MAXPV],
            b: B::new(),
            ttable: TTable::new(),
            use_tt: true,
            nodes_searched: 0,
        }
    }
    fn alpha_beta(&mut self, ply:usize, depth:i32, alpha:B::Eval, beta:B::Eval, quiescence:i32)
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

////////////////////////////////////////////////////////////

mod TTT {
use std::ops::{Neg,Not};

#[derive(Clone,Copy,Debug,Default,Eq,PartialEq)]
pub struct Move(u8);
#[derive(Clone,Copy,Debug,Default,Eq,Ord,PartialEq,PartialOrd)]
pub struct Eval(pub i32);
impl Neg for Eval {
    type Output = Eval;
    fn neg(self) -> Eval { Eval(-self.0) }
}
#[derive(Clone,Copy,Debug,Eq,Ord,PartialEq,PartialOrd)]
pub enum Side { X, O }
impl Not for Side {
    type Output = Side;
    fn not(self) -> Side {
        match self { Side::X=>Side::O, Side::O=>Side::X }
    }
}
#[derive(Clone,Copy,Debug)]
pub struct Board {
    game_state: super::GameState,
    to_play: Side,
    board: [Option<Side>; 9],
    n: usize,
}
impl Board {
    pub fn show(&self) {
        for i in 0..9 {
            print!("{}",
                match self.board[i] {
                    None => '.',
                    Some(Side::X) => 'X',
                    Some(Side::O) => 'O',
                }
            );
            if (i+1)%3==0 { println!(""); }
        }
    }
}
impl super::Board for Board {
    type Move = Move;
    type Eval = Eval;
    fn new() -> Self {
        Board {
            game_state:super::GameState::Playing,
            to_play:Side::X,
            board:[None;9],
            n:0,
        }
    }
    fn from_fen(_s: &str) -> Self {
        Self::new()
    }
    fn to_fen(&self) -> String {
        let mut s = String::new();
        s.push(match self.to_play {
                    Side::X => 'X',
                    Side::O => 'O',
                }
            );
        s.push('|');
        for &t in self.board.iter() {
            s.push(
                match t {
                    None => '.',
                    Some(Side::X) => 'X',
                    Some(Side::O) => 'O',
                }
            );
        }
        s
    }
    fn init(&mut self) {
        *self = Self::new();
    }
    fn make_move(&mut self, m: Self::Move) -> super::MoveResult {
        self.board[m.0 as usize] = Some(self.to_play);
        self.to_play = !self.to_play;
        self.n += 1;
        self.game_state = self.state();
        super::MoveResult::OtherSide
    }
    fn unmake_move(&mut self, m: Self::Move) -> super::MoveResult {
        self.board[m.0 as usize] = None;
        self.to_play = !self.to_play;
        self.n -= 1;
        self.game_state = super::GameState::Playing;
        super::MoveResult::OtherSide
    }
    fn hash(&self) -> super::Hash {
        let mut h = 0;
        for &t in self.board.iter() {
            match t {
                None => { h = h*13; }
                Some(Side::X) => { h = h*13+3; }
                Some(Side::O) => { h = h*13+7; }
            }
        }
        h
    }
    fn moves(&self, ml: &mut [Self::Move]) -> usize {
        let mut n = 0;
        for i in 0..9 {
            if self.board[i].is_none() {
                ml[n] = Move(i as u8);
                n += 1;
            }
        }
        n
    }
    fn evaluate(&self, ply:usize) -> Self::Eval {
        let v = match self.game_state {
            super::GameState::WhiteWin =>  Eval(1000000 - (ply as i32)),
            super::GameState::BlackWin => -Eval(1000000 - (ply as i32)),
            _ => Eval(0),
        };
        if self.to_play==Side::X {v} else {-v}
    }
    fn state(&self) -> super::GameState {
        for &(a,b,c) in &[(0,1,2),(3,4,5),(6,7,8),(0,3,6),(1,4,7),(2,5,8),(0,4,8),(2,4,6)] {
            if self.board[a]==self.board[b] && self.board[b]==self.board[c] {
                match self.board[a] {
                    Some(Side::X) => { return super::GameState::WhiteWin }
                    Some(Side::O) => { return super::GameState::BlackWin }
                    None => {}
                }
            }
        }
        if self.board.iter().all(|&x|x.is_some()) {
            return super::GameState::Draw;
        } else {
            return super::GameState::Playing;
        }
    }
}
}

////////////////////////////////////////////////////////////

mod MicroShogi {

pub enum Piece {
    Pawn=1, Gold, Silver, Bishop, Rook, King,
    Tokin, PromotedSilver, Horse, Dragon,
}

}

////////////////////////////////////////////////////////////

fn main() {
    let mut ab = AlphaBetaSearcher::<TTT::Board>::new();
    println!("{:?}", ab.b);
    while ab.b.state() == GameState::Playing {
        println!("====================");
        ab.b.show();
        println!("FEN: {:?}", ab.b.to_fen());
        println!("Hash: {:?}", ab.b.hash());
        let mut ml = [TTT::Move::default(); 10];
        let n = ab.b.moves(&mut ml);
        println!("{:?} {:?}", n, &ml[0..n]);

        let v = ab.alpha_beta(0, 900, TTT::Eval(-1000001), TTT::Eval(1000001), 0);
        println!("AB: {:?} {:?}", v, &ab.pv[0][0..(ab.pv_length[0])]);
        let m = ml[(19+n/2)%n];
        println!("Move ==> {:?}", m);
        ab.b.make_move(m);
    }
    println!("--------------------");
    ab.b.show();
    println!("{:?}", ab.b);
    println!("{}", ab.nodes_searched);
    println!("{} {} {} {} {}",
        ab.ttable.tt_hits, ab.ttable.tt_false,
        ab.ttable.tt_deep, ab.ttable.tt_shallow,
        ab.ttable.tt_used);


    println!("{} {}", std::mem::size_of::<TTT::Move>(), std::mem::size_of::<TTT::Eval>());
    println!("{} {}", std::mem::size_of::<TTT::Side>(), std::mem::size_of::<Option<TTT::Side>>());
    println!("{} {}", std::mem::size_of::<MicroShogi::Piece>(), std::mem::size_of::<Option<MicroShogi::Piece>>());
    println!("{} {}", MicroShogi::Piece::Pawn as i32, MicroShogi::Piece::Dragon as i32);
}

