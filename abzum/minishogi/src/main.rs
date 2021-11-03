use abzu::hash::H;

////////////////////////////////////////

bitfield::bitfield! {
  #[derive(Clone,Copy,Eq,PartialEq)]
  pub struct Move(u32);
  //impl Debug;
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

impl std::fmt::Debug for Move {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    write!(f, "(f{} t{} p{} m{} {}{}{}{}{}{})",
      self.f(), self.t(), self.p(), self.m(),
      if self.cap() {'x'} else {'_'},
      if self.pro() {'+'} else {'_'},
      if self.nonpro() {'='} else {'_'},
      if self.drop() {'*'} else {'_'},
      if self.whitemove() {'W'} else {'_'},
      if self.nullmove() {'N'} else {'_'},
    )
  }
}

impl std::fmt::Display for Move {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    let c = if self.whitemove() {Color::White} else {Color::Black};
    if self.drop() {
      write!(f, "{}*", Piece::from(self.p()).name(c))?;
    } else {
      write!(f, "{}{}{}",
        Piece::from(self.m()).name(c),
        5 - (self.f()%5),
        ['e','d','c','b','a'][(self.f()/5) as usize])?;
    }
    write!(f, "{}", if self.cap() {'x'} else {'-'})?;
    if self.cap() {
      write!(f, "{}", Piece::from(self.p()).name(c.other()))?;
    }
    write!(f, "{}{}",
      5 - (self.t()%5),
      ['e','d','c','b','a'][(self.t()/5) as usize])?;
    if self.pro() { write!(f, "+")?; }
    if self.nonpro() { write!(f, "=")?; }
    if self.p() == (Piece::King as u8) { write!(f, "#")?; }
    write!(f, "")
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
  Black = 0,
  White = 1,
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

impl From<u8> for Color {
  fn from(x:u8) -> Self {
    unsafe{std::mem::transmute(x)}
  }
}

////////////////////////////////////////

#[derive(Clone,Copy,Debug,Eq,PartialEq)]
pub enum Piece {
  Pawn = 0,
  Gold = 1,
  Silver = 2,
  Bishop = 3,
  Rook = 4,
  King = 5,
  Tokin = 6,
  Nari = 7,
  Horse = 8,
  Dragon = 9,
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

impl From<u8> for Piece {
  fn from(x:u8) -> Self {
    unsafe{std::mem::transmute(x)}
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
  hash: H,
  play: State,
  drawhash: [H; MAX_GAME_LENGTH],
}

////////////////////////////////////////

const INITIAL_BOARD : [Option<(Piece,Color)>; 25] = [
  Some((Piece::King,Color::Black)),
    Some((Piece::Gold,Color::Black)),
    Some((Piece::Silver,Color::Black)),
    Some((Piece::Bishop,Color::Black)),
    Some((Piece::Rook,Color::Black)),
  Some((Piece::Pawn,Color::Black)), None, None, None, None,
  None, None, None, None, None,
  None, None, None, None, Some((Piece::Pawn,Color::White)),
  Some((Piece::Rook,Color::White)),
    Some((Piece::Bishop,Color::White)),
    Some((Piece::Silver,Color::White)),
    Some((Piece::Gold,Color::White)),
    Some((Piece::King,Color::White)),
];

impl Board {
  fn new() -> Self {
    let mut b = Board {
      cells: INITIAL_BOARD.clone(),
      side: Color::Black,
      xside: Color::White,
      nhand: [0;2],
      hand: [[0;5];2],
      ply: 1,
      hash: 0,
      play: State::Playing,
      drawhash: [0; MAX_GAME_LENGTH],
    };
    b.hash_board();
    b.drawhash[0] = b.hash;
    b
  }

  fn hash_board(&mut self) {
    let mut h = 0;
    unsafe {
      if self.side == Color::White {h ^= HASH_SIDE_WHITE;}
      for i in 0..25 {
        if let Some((p,c)) = self.cells[i] {
          h ^= HASH_PIECE[c as usize][p as usize][i];
        }
      }
      for i in 0..2 {
        for j in 0..5 {
          h ^= HASH_PIECE_HAND[i][j][self.hand[i][j]];
        }
      }
    }
    self.hash = h;
  }
}

////////////////////////////////////////

static mut HASH_PIECE : [[[H; 25]; 10]; 2]  = [[[0; 25]; 10]; 2];
static mut HASH_PIECE_HAND : [[[H; 3]; 5]; 2]  = [[[0; 3]; 5]; 2];
static mut HASH_SIDE_WHITE : H = 0;

unsafe fn gen_hash(hg:&mut abzu::hash::HashGen) {
  HASH_SIDE_WHITE = hg.next();
  for i in 0..2 {
    for j in 0..5 {
      for k in 1..3 {
        HASH_PIECE_HAND[i][j][k] = hg.next();
      }
    }
  }
  for i in 0..2 {
    for j in 0..10 {
      for k in 0..25 {
        HASH_PIECE[i][j][k] = hg.next();
      }
    }
  }
}

////////////////////////////////////////

const BOARD_TO_BOX : [isize; 25] = [
   8,  9, 10, 11, 12,
  15, 16, 17, 18, 19,
  22, 23, 24, 25, 26,
  29, 30, 31, 32, 33,
  36, 37, 38, 39, 40,
];

const BOX_TO_BOARD : [isize; 49] = [
  -1, -1, -1, -1, -1, -1, -1,
  -1,  0,  1,  2,  3,  4, -1,
  -1,  5,  6,  7,  8,  9, -1,
  -1, 10, 11, 12, 13, 14, -1,
  -1, 15, 16, 17, 18, 19, -1,
  -1, 20, 21, 22, 23, 24, -1,
  -1, -1, -1, -1, -1, -1, -1,
];

const MOVE_STEP_NUM : [usize; 10] = [
  1, 6, 5, 0, 0, 8, 6, 6, 4, 4
];

const MOVE_STEP_OFFSET : [[isize; 8]; 10] = [
  [  7,  0,  0,  0,  0,  0,  0,  0], /* pawn */
  [  6,  7,  8,  1, -7, -1,  0,  0], /* gold */
  [  6,  7,  8, -6, -8,  0,  0,  0], /* silver */
  [  0,  0,  0,  0,  0,  0,  0,  0], /* bishop */
  [  0,  0,  0,  0,  0,  0,  0,  0], /* rook */
  [  6,  7,  8,  1, -6, -7, -8, -1], /* king */
  [  6,  7,  8,  1, -7, -1,  0,  0], /* tokin */
  [  6,  7,  8,  1, -7, -1,  0,  0], /* nari */
  [  7,  1, -7, -1,  0,  0,  0,  0], /* horse */
  [  6,  8, -8, -6,  0,  0,  0,  0], /* dragon */
];

const MOVE_SLIDE_OFFSET : [[isize; 5]; 10] = [
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

const PROMOTION : [Option<Color>; 25] = [
  Some(Color::White), Some(Color::White), Some(Color::White), Some(Color::White), Some(Color::White),
  None, None, None, None, None,
  None, None, None, None, None,
  None, None, None, None, None,
  Some(Color::Black), Some(Color::Black), Some(Color::Black), Some(Color::Black), Some(Color::Black),
];

fn generate_moves<const CAPS_ONLY:bool>(b:&Board) -> Vec<Move> {
  if b.play != State::Playing { return Vec::new(); }
  let mult = if b.side == Color::Black {1} else {-1};
  let flg = if b.side == Color::Black {0} else {MF_WHITEMOVE};
  let mut ml = Vec::new();

  // generate drops
  //   drop onto empty squares
  //   can't drop pawns onto promotion squares
  //   TODO: disallow pawn drops giving checkmate
  //   TODO: disallow pawn drops with pawns
  if !CAPS_ONLY && b.nhand[b.side as usize]>0 {
    for i in 0..5 {
      if b.hand[b.side as usize][i] > 0 {
        let f = if b.side == Color::Black {B_HAND} else {W_HAND};
        if i == (Piece::Pawn as usize) {
          for t in 0..25 {
            if b.cells[t] == None && PROMOTION[t]!=Some(b.side) {
              ml.push(Move::new(f as u8, t as u8, i as u8, i as u8, flg|MF_DROP));
            }
          }
        } else {
          for t in 0..25 {
            if b.cells[t] == None {
              ml.push(Move::new(f as u8, t as u8, i as u8, i as u8, flg|MF_DROP));
            }
          }
        }
      }
    }
  }

  for f in 0..25 {
    if let Some((mvd,c)) = b.cells[f] {
      if c!=b.side {continue;}
      // generate step moves
      for i in 0..MOVE_STEP_NUM[mvd as usize] {
        let t = BOX_TO_BOARD[(BOARD_TO_BOX[f] + mult*MOVE_STEP_OFFSET[mvd as usize][i]) as usize];
        if t<0 {continue;}
        let t = t as usize;
        if b.cells[t].is_none() && CAPS_ONLY {continue;}
        if let Some((_,c)) = b.cells[t] { if c==b.side {continue;} }
        let flags;
        let p;
        if let Some((xp,_)) = b.cells[t] { flags=MF_CAP; p=xp as u8; }
        else { flags=0; p=0; }
        if (PROMOTION[f] == Some(b.side) || PROMOTION[t] == Some(b.side))
          &&  mvd.promotes().is_some()
        {
          if mvd != Piece::Pawn {
            ml.push(Move::new(f as u8, t as u8, p as u8, mvd as u8, flg|flags|MF_NONPRO));
          }
          ml.push(Move::new(f as u8, t as u8, p as u8, mvd as u8, flg|flags|MF_PRO));
        } else {
          ml.push(Move::new(f as u8, t as u8, p as u8, mvd as u8, flg|flags));
        }
      }
      // generate slide moves
      for &offset in &MOVE_SLIDE_OFFSET[mvd as usize] {
        if offset == 0 {break;}
        let mut t = f;
        loop {
          let pt = BOX_TO_BOARD[(BOARD_TO_BOX[t as usize] + offset) as usize];
          if pt<0 {break;}
          t = pt as usize;
          let fp = if (PROMOTION[f] == Some(b.side) || PROMOTION[t] == Some(b.side))
              && mvd.promotes().is_some() {MF_PRO} else {0};
          match b.cells[t as usize] {
            None => {
              if !CAPS_ONLY {
                ml.push(Move::new(f as u8, t as u8, 0, mvd as u8, flg|fp));
                if fp!=0 {
                  ml.push(Move::new(f as u8, t as u8, 0, mvd as u8, flg|MF_NONPRO));
                }
              }
            }
            Some((xp,c)) if c==b.xside => {
              ml.push(Move::new(f as u8, t as u8, xp as u8, mvd as u8, flg|fp|MF_CAP));
              if fp!=0 {
                ml.push(Move::new(f as u8, t as u8, xp as u8, mvd as u8, flg|MF_NONPRO|MF_CAP));
              }
              break;
            }
            _ => break,
          }
        }
      }
    }
  }

  return ml;
}

////////////////////////////////////////

fn main() {
  let mut hg = abzu::hash::HashGen::new(1);
  unsafe{gen_hash(&mut hg)};
  let mut b = Board::new();
  println!("{:?}", b);

  println!("{:?}", generate_moves::<false>(&b));
  for m in generate_moves::<false>(&b) { print!(" {}", m); } println!();

  println!("{:?}", generate_moves::<true>(&b));
  for m in generate_moves::<true>(&b) { print!(" {}", m); } println!();
}

