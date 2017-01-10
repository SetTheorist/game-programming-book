use std::fmt::{Debug};
use std::ops::{Neg, Not};

////////////////////////////////////////////////////////////

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
    fn moves(&self, ml: &mut [Self::Move]) -> usize;
    fn evaluate(&self, ply:usize) -> Self::Eval;
    fn state(&self) -> GameState;
}

////////////////////////////////////////////////////////////

type Hash = u64;

const TT_VALID : i32 = 1;
const TT_EXACT : i32 = 2;
const TT_UPPER : i32 = 4;
const TT_LOWER : i32 = 8;
const TTABLE_ENTRIES : usize = 1024 * 1024;

#[derive(Clone,Copy,Debug,Default)]
struct TTEntry<E:Clone+Copy+Debug+Default,M:Clone+Copy+Debug+Default> {
    hash: u32,
    depth: i32,
    flags: i32,
    score: E,
    best_move: M,
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
pub enum Side { X=1, O }
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
use std::fmt;
use std::mem;
use std::ops::{Neg,Not};
extern crate rand;

const NPIECE : usize = 10;
#[derive(Clone,Copy,Debug,Eq,Ord,PartialEq,PartialOrd)]
pub enum Piece {
    Pawn=1, Gold, Silver, Bishop, Rook, King,
    Tokin, PromotedSilver, Horse, Dragon,
}
impl Piece {
    pub fn to_char(self) -> char {
        match self {
            Piece::Pawn => 'P',
            Piece::Gold => 'G',
            Piece::Silver => 'S',
            Piece::Bishop => 'B',
            Piece::Rook => 'R',
            Piece::King => 'K',
            Piece::Tokin => 'T',
            Piece::PromotedSilver => 'N',
            Piece::Horse => 'H',
            Piece::Dragon => 'D',
        }
    }
}

static PROMOTES : [Option<Piece>; NPIECE+1] = [ None,
    Some(Piece::Tokin), None, Some(Piece::PromotedSilver), Some(Piece::Horse),
    Some(Piece::Dragon), None, None, None, None, None,
];

static UNPROMOTES : [Piece; NPIECE+1] = [ Piece::Pawn,
    Piece::Pawn, Piece::Gold, Piece::Silver, Piece::Bishop, Piece::Rook, Piece::King,
    Piece::Pawn, Piece::Silver, Piece::Bishop, Piece::Rook,
];
    
#[derive(Clone,Copy,Debug,Eq,Ord,PartialEq,PartialOrd)]
pub enum Color {
    White=1, Black,
}
impl Not for Color {
    type Output = Color;
    fn not(self) -> Color {
        match self { Color::White=>Color::Black, Color::Black=>Color::White }
    }
}

#[derive(Clone,Copy,Debug,Default,Eq,Ord,PartialEq,PartialOrd)]
pub struct Eval(pub i32);
impl Neg for Eval {
    type Output = Eval;
    fn neg(self) -> Eval { Eval(-self.0) }
}

const B_HAND       : usize = 25;
const W_HAND       : usize = 26;
const MF_CAP       : u8 = 0b00001;
const MF_PRO       : u8 = 0b00010;
const MF_NONPRO    : u8 = 0b00100;
const MF_DROP      : u8 = 0b01000;
const MF_WHITEMOVE : u8 = 0b10000;
#[derive(Clone,Copy,Default,Eq,PartialEq)]
pub struct Move(u32);
impl Move {
    pub fn new(f: usize, t: usize, p: usize, m: usize, flag: u8) -> Move {
        Move(
            ((f    as u32&0x3F)<<26)|
            ((t    as u32&0x3F)<<20)|
            ((p    as u32&0x3F)<<14)|
            ((m    as u32&0x3F)<< 8)|
            ((flag as u32&0xFF)<< 0) )
    }
    pub fn get_f(self)    -> u8 { ((self.0 & 0xFC000000) >> 26) as u8 }
    pub fn get_t(self)    -> u8 { ((self.0 & 0x03F00000) >> 20) as u8 }
    pub fn get_p(self)    -> u8 { ((self.0 & 0x000FC000) >> 14) as u8 }
    pub fn get_m(self)    -> u8 { ((self.0 & 0x00003F00) >>  8) as u8 }
    pub fn get_flag(self) -> u8 { ((self.0 & 0x000000FF) >>  0) as u8 }
}
impl fmt::Debug for Move {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "M[{:8x}|{}|{}|{}|{}|{}]", self.0,
            self.get_f(), self.get_t(), self.get_p(), self.get_m(), self.get_flag())
    }
}
impl fmt::Display for Move {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "M[{:8x}|{}|{}|{}|{}|{}]", self.0,
            self.get_f(), self.get_t(), self.get_p(), self.get_m(), self.get_flag())
    }
}

#[derive(Clone,Copy,Debug)]
pub struct Hasher {
    side : super::Hash,
    piece : [[[super::Hash; 25]; NPIECE+1]; 2+1],
    hand : [[[super::Hash; 3]; 5+1]; 2+1],
}
impl Hasher {
    pub fn new<R:rand::Rng>(rng: &mut R) -> Self {
        let mut hasher : Hasher = unsafe { mem::uninitialized() };

        hasher.side = rng.gen::<super::Hash>();
        for x in &mut hasher.piece.iter_mut() {
            for x in &mut x.iter_mut() {
                for x in &mut x.iter_mut() {
                    *x = rng.gen::<super::Hash>(); } } }
        for x in &mut hasher.hand.iter_mut() {
            for x in &mut x.iter_mut() {
                for x in &mut x.iter_mut() {
                    *x = rng.gen::<super::Hash>(); } } }
        hasher
    }
}

static INITIAL_BOARD : [Option<(Piece,Color)>; 27] = [
    Some((Piece::King,Color::Black)), Some((Piece::Gold,Color::Black)), Some((Piece::Silver,Color::Black)), Some((Piece::Bishop,Color::Black)), Some((Piece::Rook,Color::Black)),
    Some((Piece::Pawn,Color::Black)), None, None, None, None,
    None, None, None, None, None,
    None, None, None, None, Some((Piece::Pawn,Color::White)),
    Some((Piece::Rook,Color::White)), Some((Piece::Bishop,Color::White)), Some((Piece::Silver,Color::White)), Some((Piece::Gold,Color::White)), Some((Piece::King,Color::White)),
    Some((Piece::King,Color::Black)), Some((Piece::King,Color::White)),
];

static board_to_box : [isize; 25] = [
   8,  9, 10, 11, 12,
  15, 16, 17, 18, 19,
  22, 23, 24, 25, 26,
  29, 30, 31, 32, 33,
  36, 37, 38, 39, 40,
];

static box_to_board : [isize; 49] = [
  -1, -1, -1, -1, -1, -1, -1,
  -1,  0,  1,  2,  3,  4, -1,
  -1,  5,  6,  7,  8,  9, -1,
  -1, 10, 11, 12, 13, 14, -1,
  -1, 15, 16, 17, 18, 19, -1,
  -1, 20, 21, 22, 23, 24, -1,
  -1, -1, -1, -1, -1, -1, -1,
];

/*dummy, pawn, gold, silver, bishop, rook, king, tokin, promoted_silver, horse, dragon*/
static move_step_num : [isize; NPIECE+1] = [0, 1, 6, 5, 0, 0, 8, 6, 6, 4, 4];
static move_step_offset : [[isize; 8]; NPIECE+1] = [
  [  0,  0,  0,  0,  0,  0,  0,  0], /* dummy */
  [  7,  0,  0,  0,  0,  0,  0,  0], /* pawn */
  [  6,  7,  8,  1, -7, -1,  0,  0], /* gold */
  [  6,  7,  8, -6, -8,  0,  0,  0], /* silver */
  [  0,  0,  0,  0,  0,  0,  0,  0], /* bishop */
  [  0,  0,  0,  0,  0,  0,  0,  0], /* rook */
  [  6,  7,  8,  1, -6, -7, -8, -1], /* king */
  [  6,  7,  8,  1, -7, -1,  0,  0], /* tokin */
  [  6,  7,  8,  1, -7, -1,  0,  0], /* promoted_silver */
  [  7,  1, -7, -1,  0,  0,  0,  0], /* horse */
  [  6,  8, -8, -6,  0,  0,  0,  0], /* dragon */
];
static move_slide_offset : [[isize; 5]; NPIECE+1] = [
  [  0,  0,  0,  0,  0], /* dummy */
  [  0,  0,  0,  0,  0],
  [  0,  0,  0,  0,  0],
  [  0,  0,  0,  0,  0],
  [  6,  8, -6, -8,  0], /* bishop */
  [  7,  1, -7, -1,  0], /* rook */
  [  0,  0,  0,  0,  0],
  [  0,  0,  0,  0,  0],
  [  0,  0,  0,  0,  0],
  [  6,  8, -6, -8,  0], /* horse */
  [  7,  1, -7, -1,  0], /* dragon */
];

static promotion : [Option<Color>; 25] = [
  Some(Color::White), Some(Color::White), Some(Color::White), Some(Color::White), Some(Color::White),
  None, None, None, None, None,
  None, None, None, None, None,
  None, None, None, None, None,
  Some(Color::Black), Some(Color::Black), Some(Color::Black), Some(Color::Black), Some(Color::Black),
];


#[derive(Clone,Copy,Debug)]
pub struct Board {
    square : [Option<(Piece,Color)>; 5*5],
    side : Color,
    xside : Color,
    hand : [[usize; 5+1]; 2],
    nhand : [usize; 2],
    ply : i32,
    hash : super::Hash,
    play : super::GameState,
    hasher : Hasher,
}

impl super::Board for Board {
    type Move = Move;
    type Eval = Eval;
    fn new() -> Self {
        let mut b = Board {
            square : {let mut x=[None; 25]; x.clone_from_slice(&INITIAL_BOARD[0..25]); x},
            side : Color::Black,
            xside : Color::White,
            hand : [[0; 5+1]; 2],
            nhand : [0; 2],
            ply : 0,
            hash : 0,
            play : super::GameState::Playing,
            hasher : Hasher::new(&mut rand::thread_rng()),
        };
        b.hash = b.hash();
        b
    }
    fn from_fen(s: &str) -> Self {
        Board::new()
    }
    fn to_fen(&self) -> String {
        let mut s = String::new();
        let mut n = 0;
        for (i,&x) in self.square.iter().enumerate() {
            if i!=0 && (i%5)==0 {
                if n>0 { s.push_str(&n.to_string()); n=0; }
                s.push('/');
            }
            match x {
                None => { n += 1; }
                Some((p,c)) => {
                    if n>0 { s.push_str(&n.to_string()); n=0; }
                    if c==Color::Black { s.push(p.to_char()); }
                    else { s.push(p.to_char().to_lowercase().nth(0).unwrap()); }
                }
            }
        }
        if n>0 { s.push_str(&n.to_string()); n=0; }
        s.push(':');
        s.push(if self.side==Color::Black{'B'}else{'W'});
        s.push(':');
        s.push(':');
        s
    }
    fn init(&mut self) {
        *self = Self::new();
    }
    fn make_move(&mut self, m: Self::Move) -> super::MoveResult {
        let flag = m.get_flag();
        if flag & MF_DROP != 0 {
            self.square[m.get_t() as usize] = Some((
                unsafe{mem::transmute(m.get_p())},
                unsafe{mem::transmute(self.side)}));
            self.nhand[self.side as usize - 1] -= 1;
            self.hand[self.side as usize - 1][m.get_p() as usize] -= 1;
        } else {
            if flag & MF_CAP != 0 {
                if m.get_p() == Piece::King as u8 {
                    self.play =
                        if self.side==Color::Black { super::GameState::BlackWin }
                        else { super::GameState::WhiteWin };
                } else {
                    self.nhand[self.side as usize] += 1;
                    self.hand[self.side as usize][UNPROMOTES[m.get_p() as usize] as usize] += 1;
                }
            }
            let fsq = self.square[m.get_f() as usize];
            self.square[m.get_t() as usize] = fsq;
            self.square[m.get_f() as usize] = None;
            if flag & MF_PRO != 0 {
                if let Some((p,c)) = self.square[m.get_t() as usize] {
                    self.square[m.get_t() as usize] = Some((PROMOTES[p as usize].unwrap(), c));
                }
            }
        }

        self.side = !self.side;
        self.xside = !self.xside;

        self.hash = self.hash();
        super::MoveResult::SameSide
    }
    fn unmake_move(&mut self, m: Self::Move) -> super::MoveResult {
        self.hash = self.hash();
        super::MoveResult::SameSide
    }
    fn hash(&self) -> super::Hash {
        let mut h = if self.side==Color::White {self.hasher.side} else {0};
        for i in 0..25 {
            if let Some((p,c)) = self.square[i] {
                h ^= self.hasher.piece[c as usize][p as usize][i];
            }
        }
        for i in 0..2 {
            for j in 0..5 {
                h ^= self.hasher.hand[i][j][self.hand[i][j] as usize];
            }
        }
        h
    }
    fn moves(&self, ml: &mut [Self::Move]) -> usize {
        let mut n = 0;
        let mut add_move = |f,t,p,m,fl,n:&mut usize| { ml[*n]=Move::new(f, t, p, m, fl); *n+=1; };
        let mult = if self.side==Color::Black{1}else{-1};
        let flg = if self.side==Color::Black{0}else{MF_WHITEMOVE};
        // drops:
        //     - empty square
        //     - not promotion
        //     - TODO: disallow checkmate pawn drops
        for i in 0..5 {
        }
        for f in 0..25 {
            if let Some((p,c)) = self.square[f] {
                if c!=self.side { continue; }
                let mvd = p as usize;
                // step moves
                for i in 0..move_step_num[mvd] {
                    let t = box_to_board[(board_to_box[f] + mult*move_step_offset[mvd][i as usize]) as usize];
                    if t<0 { continue; }
                    let t = t as usize;
                    if self.square[t]==None || self.square[t].unwrap().1==self.xside {
                        let mut flags = 0;
                        let mut x = 0;
                        if self.square[t]!=None {
                            flags |= MF_CAP;
                            x = self.square[t].unwrap().0 as i32;
                        }
                        if (promotion[t]==Some(self.side) || promotion[f]==Some(self.side))
                            && PROMOTES[p as usize]!=None {
                            add_move(f, t, x as usize, p as usize, flg|flags|MF_PRO,&mut n);
                            if p!=Piece::Pawn {
                                add_move(f, t, x as usize, p as usize, flg|flags|MF_NONPRO,&mut n);
                            }
                        } else {
                            add_move(f, t, x as usize, p as usize, flg|flags,&mut n);
                        }
                    }
                }
                // slide moves
            }
        }
        n
    }
    fn evaluate(&self, ply:usize) -> Self::Eval {
        Self::Eval::default()
    }
    fn state(&self) -> super::GameState {
        super::GameState::Playing
    }
}

}


////////////////////////////////////////////////////////////
extern crate rand;
use rand::Rng;

fn main() {
    let mut ab = AlphaBetaSearcher::<MicroShogi::Board>::new();
    println!("{:?}", ab.b.to_fen());
    let mut ml = [MicroShogi::Move::default(); 99];
    let n = ab.b.moves(&mut ml);
    println!("{} : {:?}", n, &ml[0..n]);
    //println!("{:?}", ab.b);
    return;


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


    println!("{}", std::mem::size_of::<TTEntry<TTT::Move,TTT::Eval>>());
    println!("{}", std::mem::size_of::<TTable<TTT::Board>>());
    println!("{}", std::mem::size_of::<Option<(MicroShogi::Piece,MicroShogi::Color)>>());
    let mut rng = rand::thread_rng();
    println!("{}", rng.gen::<Hash>());
}

