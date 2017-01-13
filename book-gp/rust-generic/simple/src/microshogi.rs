use std::fmt;
use std::mem;
use std::ops::{Neg,Not};

extern crate rand;

use search;

const NPIECE : usize = 10;
#[derive(Clone,Copy,Debug,Eq,Ord,PartialEq,PartialOrd)]
pub enum Piece { Empty=0,
    Pawn=1, Gold, Silver, Bishop, Rook, King,
    Tokin, PromotedSilver, Horse, Dragon,
}
impl Piece {
    pub fn to_char(self) -> char {
        match self {
            Piece::Empty => '?',
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
    pub fn promotes(self) -> Piece {
        match self {
            Piece::Empty => Piece::Empty,
            Piece::Pawn => Piece::Tokin,
            Piece::Gold => Piece::Empty,
            Piece::Silver => Piece::PromotedSilver,
            Piece::Bishop => Piece::Horse,
            Piece::Rook => Piece::Dragon,
            Piece::King => Piece::Empty,
            Piece::Tokin => Piece::Empty,
            Piece::PromotedSilver => Piece::Empty,
            Piece::Horse => Piece::Empty,
            Piece::Dragon => Piece::Empty,
        }
    }
    pub fn unpromotes(self) -> Piece {
        match self {
            Piece::Empty => Piece::Empty,
            Piece::Pawn => Piece::Pawn,
            Piece::Gold => Piece::Gold,
            Piece::Silver => Piece::Silver,
            Piece::Bishop => Piece::Bishop,
            Piece::Rook => Piece::Rook,
            Piece::King => Piece::King,
            Piece::Tokin => Piece::Pawn,
            Piece::PromotedSilver => Piece::Silver,
            Piece::Horse => Piece::Bishop,
            Piece::Dragon => Piece::Rook,
        }
    }
}
impl fmt::Display for Piece {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.to_char())
    }
}
    
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
    pub fn new(f: usize, t: usize, p: Piece, m: Piece, flag: u8) -> Move {
        Move(
            ((f    as u32&0x3F)<<26)|
            ((t    as u32&0x3F)<<20)|
            ((p    as u32&0x3F)<<14)|
            ((m    as u32&0x3F)<< 8)|
            ((flag as u32&0xFF)<< 0) )
    }
    pub fn f(self)    -> usize { ((self.0 & 0xFC000000) >> 26) as usize }
    pub fn t(self)    -> usize { ((self.0 & 0x03F00000) >> 20) as usize }
    pub fn p(self)    -> Piece { unsafe { mem::transmute(((self.0 & 0x000FC000) >> 14) as u8) } }
    pub fn m(self)    -> Piece { unsafe { mem::transmute(((self.0 & 0x00003F00) >>  8) as u8) } }
    pub fn flag(self) -> u8 { ((self.0 & 0x000000FF) >>  0) as u8 }
}
impl fmt::Debug for Move {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "M[{:8x}|{}|{}|{}|{}|{}]", self.0,
            self.f(), self.t(), self.p(), self.m(), self.flag())
    }
}
impl fmt::Display for Move {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fn sq(x: usize) -> (char,char) {
            ((('a' as u32) + ((x % 5) as u32)) as u8 as char,
             (('1' as u32) + ((x / 5) as u32)) as u8 as char)
        }
        write!(f, "{}", self.m())?;
        if self.flag()&MF_DROP==0{ let(a,b)=sq(self.f()); write!(f, "{}{}", a, b)?; }
        write!(f, "{}",
            (if self.flag()&MF_DROP!=0{"*"}else if self.flag()&MF_CAP!=0{":"}else{"-"}))?;
        let(a,b)=sq(self.t()); write!(f, "{}{}", a, b)?;
        write!(f, "{}",
            (if self.flag()&MF_PRO!=0{"+"}else if self.flag()&MF_NONPRO!=0{"="}else{""}))
    }
}

#[derive(Clone,Copy,Debug)]
pub struct Hasher {
    side : search::Hash,
    piece : [[[search::Hash; 25]; NPIECE+1]; 2+1],
    hand : [[[search::Hash; 3]; 5+1]; 2+1],
}
impl Hasher {
    pub fn new<R:rand::Rng>(rng: &mut R) -> Self {
        let mut hasher : Hasher = unsafe { mem::uninitialized() };

        hasher.side = rng.gen::<search::Hash>();
        for x in &mut hasher.piece.iter_mut() {
            for x in &mut x.iter_mut() {
                for x in &mut x.iter_mut() {
                    *x = rng.gen::<search::Hash>(); } } }
        for x in &mut hasher.hand.iter_mut() {
            for x in &mut x.iter_mut() {
                for x in &mut x.iter_mut() {
                    *x = rng.gen::<search::Hash>(); } } }
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
fn move_step_offset(p: Piece) -> &'static [isize; 8] {
    match p {
        Piece::Empty =>           { static v : [isize; 8] = [0,0,0,0,0,0,0,0]; &v }
        Piece::Pawn =>            { static v : [isize; 8] = [7,0,0,0,0,0,0,0]; &v }
        Piece::Gold =>            { static v : [isize; 8] = [6,7,8,1,-7,-1,0,0]; &v }
        Piece::Silver =>          { static v : [isize; 8] = [6,7,8,-6,-8,0,0,0]; &v }
        Piece::Bishop =>          { static v : [isize; 8] = [0,0,0,0,0,0,0,0]; &v }
        Piece::Rook =>            { static v : [isize; 8] = [0,0,0,0,0,0,0,0]; &v }
        Piece::King =>            { static v : [isize; 8] = [6,7,8,1,-6,-7,-8,-1]; &v }
        Piece::Tokin =>           { static v : [isize; 8] = [6,7,8,1,-7,-1,0,0]; &v }
        Piece::PromotedSilver =>  { static v : [isize; 8] = [6,7,8,1,-7,-1,0,0]; &v }
        Piece::Horse =>           { static v : [isize; 8] = [7,1,-7,-1,0,0,0,0]; &v }
        Piece::Dragon =>          { static v : [isize; 8] = [6,8,-6,-8,0,0,0,0]; &v }
    }
}
fn move_slide_offset(p: Piece) -> &'static [isize; 4] {
    match p {
        Piece::Empty =>           { static v : [isize; 4] = [0,0,0,0]; &v }
        Piece::Pawn =>            { static v : [isize; 4] = [0,0,0,0]; &v }
        Piece::Gold =>            { static v : [isize; 4] = [0,0,0,0]; &v }
        Piece::Silver =>          { static v : [isize; 4] = [0,0,0,0]; &v }
        Piece::Bishop =>          { static v : [isize; 4] = [6,8,-6,-8]; &v }
        Piece::Rook =>            { static v : [isize; 4] = [7,1,-7,-1]; &v }
        Piece::King =>            { static v : [isize; 4] = [0,0,0,0]; &v }
        Piece::Tokin =>           { static v : [isize; 4] = [0,0,0,0]; &v }
        Piece::PromotedSilver =>  { static v : [isize; 4] = [0,0,0,0]; &v }
        Piece::Horse =>           { static v : [isize; 4] = [6,8,-6,-8]; &v }
        Piece::Dragon =>          { static v : [isize; 4] = [7,1,-7,-1]; &v }
    }
}

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
    hand : [[usize; 5+1]; 2+1],
    nhand : [usize; 2+1],
    ply : i32,
    hash : search::Hash,
    play : search::GameState,
    hasher : Hasher,
}

impl search::Board for Board {
    type Move = Move;
    type Eval = Eval;
    fn new() -> Self {
        let mut b = Board {
            square : {let mut x=[None; 25]; x.clone_from_slice(&INITIAL_BOARD[0..25]); x},
            side : Color::Black,
            hand : [[0; 5+1]; 2+1],
            nhand : [0; 2+1],
            ply : 0,
            hash : 0,
            play : search::GameState::Playing,
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
    fn make_move(&mut self, m: Self::Move) -> search::MoveResult {
        let flag = m.flag();
        if flag & MF_DROP != 0 {
            self.square[m.t()] = Some((m.p(), unsafe{mem::transmute(self.side)}));
            self.nhand[self.side as usize] -= 1;
            self.hand[self.side as usize][m.p() as usize] -= 1;
        } else {
            if flag & MF_CAP != 0 {
                if m.p() == Piece::King {
                    self.play =
                        if self.side==Color::Black { search::GameState::BlackWin }
                        else { search::GameState::WhiteWin };
                } else {
                    self.nhand[self.side as usize] += 1;
                    self.hand[self.side as usize][m.p().unpromotes() as usize] += 1;
                }
            }
            self.square[m.t()] = self.square[m.f()];
            self.square[m.f()] = None;
            if flag & MF_PRO != 0 {
                if let Some((p,c)) = self.square[m.t()] {
                    self.square[m.t()] = Some((p.promotes(), c));
                }
            }
        }
        self.side = !self.side;
        self.hash = self.hash();
        search::MoveResult::SameSide
    }
    fn unmake_move(&mut self, m: Self::Move) -> search::MoveResult {
        self.hash = self.hash();
        search::MoveResult::SameSide
    }
    fn hash(&self) -> search::Hash { self.hash() }
    fn moves(&self, ml: &mut [Self::Move]) -> usize {
        let mut n = 0;
        let mut add_move = |f,t,p,m,fl,n:&mut usize| { ml[*n]=Move::new(f, t, p, m, fl); *n+=1; };
        let mult = if self.side==Color::Black{1}else{-1};
        let flg = if self.side==Color::Black{0}else{MF_WHITEMOVE};
        // drops:
        //     - empty square
        //     - not promotion
        //     - TODO: disallow checkmate pawn drops
        if self.nhand[self.side as usize] > 0 {
            for i in 0..5 {
                if self.hand[self.side as usize][i] > 0 {
                    let f = if self.side==Color::Black{B_HAND}else{W_HAND};
                    for t in 0..25 {
                        if self.square[t]==None
                            && (i!=(Piece::Pawn as usize)||promotion[t]!=Some(self.side)) {
                            let p = unsafe { mem::transmute(i as u8) };
                            add_move(f, t, p, p, flg|MF_DROP, &mut n);
                        }
                    }
                }
            }
        }
        for f in 0..25 {
            if let Some((p,c)) = self.square[f] {
                if c!=self.side { continue; }
                // step moves
                for &off in move_step_offset(p).iter().filter(|&&off|off!=0) {
                    let t = box_to_board[(board_to_box[f] + mult*off) as usize];
                    if t<0 { continue; }
                    let t = t as usize;
                    if self.square[t]==None || self.square[t].unwrap().1==!self.side {
                        let mut flags = 0;
                        let mut x = Piece::Empty;
                        if self.square[t]!=None {
                            flags |= MF_CAP;
                            x = self.square[t].unwrap().0;
                        }
                        if (promotion[t]==Some(self.side) || promotion[f]==Some(self.side))
                            && p.promotes()!=Piece::Empty {
                            add_move(f, t, x, p, flg|flags|MF_PRO,&mut n);
                            if p!=Piece::Pawn {
                                add_move(f, t, x, p, flg|flags|MF_NONPRO,&mut n);
                            }
                        } else {
                            add_move(f, t, x, p, flg|flags,&mut n);
                        }
                    }
                }
                // slide moves
                for &off in move_slide_offset(p).iter().filter(|&&off|off!=0) {
                    let mut t = f;
                    loop {
                        t = box_to_board[board_to_box[t] + off];
                        if t<0 { break; }

                    }
                }
            }
        }
        n
    }
    fn evaluate(&self, ply:usize) -> Self::Eval {
        Self::Eval::default()
    }
    fn state(&self) -> search::GameState {
        search::GameState::Playing
    }
}

impl Board {
    pub fn show(&self) {
        println!("{:016x}", self.hash());
        for y in (0..5).rev() {
            print!("{}|", ('1' as u32 + y as u32) as u8 as char);
            for x in 0..5 {
                let x = self.square[x + y*5];
                match x {
                    None => { print!("."); }
                    Some((p,c)) => {
                        print!("{}", 
                            if c==Color::Black { p.to_char() }
                            else { p.to_char().to_lowercase().nth(0).unwrap() }
                        );
                    }
                }
            }
            print!("\n");
        }
        print!(" +-----\n  abcde\n");
    }
    fn hash(&self) -> search::Hash {
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
}
