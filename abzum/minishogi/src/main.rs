
////////////////////////////////////////

bitfield::bitfield! {
  #[derive(Clone,Copy,Eq,PartialEq)]
  pub struct Move(u32);
  impl Debug;
  u8,f,_    : 5,0; // from
  u8,t,_    : 11,6; // to
  u8,p,_    : 17,12; // captured/dropped piece
  u8,m,_    : 23,18; // moved piece
  cap,_       : 24;
  pro,_       : 25;
  nonpro,_    : 26;
  drop,_      : 27;
  whitemove,_ : 28;
  nullmove,_  : 29;
}

const MF_CAP : u8 = 1;
const MF_PRO : u8 = 2;
const MF_NONPRO : u8 = 4;
const MF_DROP : u8 = 8;
const MF_WHITEMOVE : u8 = 16;
const MF_NULLMOVE : u8 = 32;

impl Move {
  #[inline]
  fn new(f:u8, t:u8, p:u8, m:u8, flags:u8) -> Self {
    Move((f as u32)
      | ((t as u32)<<6)
      | ((p as u32)<<12)
      | ((m as u32)<<18)
      | ((flags as u32)<<24)
      )
  }
}

const B_HAND : usize = 25;
const W_HAND : usize = 26;

////////////////////////////////////////

const PV_LENGTH : usize = 64;
const MAX_GAME_LENGTH : usize = 256;

////////////////////////////////////////

#[derive(Clone,Copy,Debug,Default,PartialEq,PartialOrd)]
pub struct FVal(f64);

impl std::ops::Add for FVal {
  type Output = Self;
  #[inline] fn add(self, rhs:Self) -> Self { FVal(self.0 + rhs.0) }
}
impl std::ops::Sub for FVal {
  type Output = Self;
  #[inline] fn sub(self, rhs:Self) -> Self { FVal(self.0 - rhs.0) }
}
impl std::ops::Neg for FVal {
  type Output = Self;
  #[inline] fn neg(self) -> Self { FVal(-self.0) }
}

impl abzu::Value for FVal {
  const MIN  : FVal = FVal(-100_000_000_f64);
  const MAX  : FVal = FVal( 100_000_000_f64);
  const DRAW : FVal = FVal(0_f64);
  #[inline]
  fn mate_in_n(n:usize) -> Self {
    Self::MAX - FVal(n as f64)
  }
}

////////////////////////////////////////

#[derive(Clone,Copy,Debug,Eq,PartialEq)]
pub enum Color {
  Black, White,
}

impl Color {
  #[inline]
  fn other(self) -> Self {
    match self {
      Color::Black => Color::White,
      Color::White => Color::Black,
    }
  }
}

////////////////////////////////////////

#[derive(Clone,Copy,Debug,Eq,PartialEq)]
pub enum Piece {
  Pawn,
  Gold,
  Silver,
  Bishop,
  Rook,
  King,
  Tokin,
  Nari,
  Horse,
  Dragon,
}

impl Piece {
  #[inline]
  pub fn promotes(self) -> Option<Self> {
    use Piece::*;
    match self {
      Pawn => Some(Tokin),
      Silver => Some(Nari),
      Bishop => Some(Horse),
      Rook => Some(Dragon),
      _ => None,
    }
  }

  #[inline]
  pub fn unpromotes(self) -> Option<Self> {
    use Piece::*;
    match self {
      Tokin => Some(Pawn),
      Nari => Some(Silver),
      Horse => Some(Bishop),
      Dragon => Some(Rook),
      _ => None,
    }
  }

  pub fn name(self, c:Color) -> char {
    use Color::*;
    use Piece::*;
    match (self,c) {
      (Pawn,Black) => 'P',
      (Gold,Black) => 'G',
      (Silver,Black) => 'S',
      (Bishop,Black) => 'B',
      (Rook,Black) => 'R',
      (King,Black) => 'K',
      (Tokin,Black) => 'T',
      (Nari,Black) => 'N',
      (Horse,Black) => 'H',
      (Dragon,Black) => 'D',
      (Pawn,White) => 'p',
      (Gold,White) => 'g',
      (Silver,White) => 's',
      (Bishop,White) => 'b',
      (Rook,White) => 'r',
      (King,White) => 'k',
      (Tokin,White) => 't',
      (Nari,White) => 'n',
      (Horse,White) => 'h',
      (Dragon,White) => 'd',
    }
  }
}

////////////////////////////////////////

#[derive(Clone,Copy,Debug,Eq,PartialEq)]
pub enum State {
  Playing,
  BlackWin,
  WhiteWin,
  Draw,
}

////////////////////////////////////////

#[derive(Clone,Debug)]
pub struct Board {
  cells: [Option<(Piece,Color)>; 25],
  side: Color,
  xside: Color,
  nhand: [usize;2],
  hand: [[usize;5];2],
  ply: usize,
  hash: abzu::hash::H,
  play: State,
  drawhash: [abzu::hash::H; MAX_GAME_LENGTH],
}

////////////////////////////////////////

fn main() {
    println!("Hello, world!");
}
