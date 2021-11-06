#![feature(destructuring_assignment)]

use abzu::hash::H;

//王将
//角行
//金将
//銀将
//歩兵

////////////////////////////////////////

bitfield::bitfield! {
  #[derive(Clone,Copy,Default,Eq,PartialEq)]
  pub struct Move(u32);
  //impl Debug;
  u8,f,_    : 5,0; // from
  u8,t,_    : 11,6; // to
  u8,p,_    : 17,12; // captured/dropped piece
  u8,into Piece, piece,_    : 17,12; // captured/dropped piece
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

  fn null() -> Self {
    Move((MF_NULLMOVE as u32) << 24)
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
      write!(f, "{}", if self.cap() {'x'} else {'-'})?;
    }
    if self.cap() {
      write!(f, "{}", self.piece().name(c.other()))?;
    }
    write!(f, "{}{}",
      5 - (self.t()%5),
      ['e','d','c','b','a'][(self.t()/5) as usize])?;
    if self.pro() { write!(f, "+")?; }
    if self.nonpro() { write!(f, "=")?; }
    if self.piece() == Piece::King { write!(f, "#")?; }
    write!(f, "")
  }
}

const B_HAND : usize = 25;
const W_HAND : usize = 26;

////////////////////////////////////////

const PV_LENGTH : usize = 64;
const MAX_GAME_LENGTH : usize = 256;

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
  pub fn can_promote(self) -> bool {
    use Piece::*;
    match self {
      Pawn => true,
      Silver => true,
      Bishop => true,
      Rook => true,
      _ => false,
    }
  }
  pub fn promotes(self) -> Self {
    use Piece::*;
    match self {
      Pawn => Tokin,
      Silver => Nari,
      Bishop => Horse,
      Rook => Dragon,
      x => x,
    }
  }

  #[inline]
  pub fn unpromotes(self) -> Self {
    use Piece::*;
    match self {
      Tokin => Pawn,
      Nari => Silver,
      Horse => Bishop,
      Dragon => Rook,
      x => x,
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

impl std::fmt::Display for Board {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    write!(f, "   5  4  3  2  1  \n")?;
    write!(f, " +---------------+ [{:016X}]\n", self.hash)?;
    for r in (0..5).rev() {
      write!(f, " |")?;
      for c in 0..5 {
        let rc = r*5 + c;
        if let Some((p,c)) = self.cells[rc] {
          write!(f, " {} ", Piece::from(p).name(c))?;
        } else {
          write!(f, " . ")?;
        }
      }
      write!(f, "| ({})", ['e','d','c','b','a'][r])?;
      match r {
        0 => {
          write!(f, " {}.{}\n", (self.ply+1)/2, (if self.side==Color::Black{""}else{".."}))?;
        }
        1 => {
          write!(f, " Black({})[ ", self.nhand[Color::Black as usize])?;
          for i in 0..5 {
            if self.hand[Color::Black as usize][i] > 0 {
              for _ in 0..self.hand[Color::Black as usize][i] {
                write!(f, " {}", Piece::from(i as u8).name(Color::Black))?;
              }
            }
          }
          write!(f, "]\n")?;
        }
        2 => {
          write!(f, " White({})[ ", self.nhand[Color::White as usize])?;
          for i in 0..5 {
            if self.hand[Color::White as usize][i] > 0 {
              for _ in 0..self.hand[Color::White as usize][i] {
                write!(f, " {}", Piece::from(i as u8).name(Color::White))?;
              }
            }
          }
          write!(f, "]\n")?;
        }
        3 => {
          write!(f, " \"{}\"\n", self.to_fen())?;
        }
        4 => {
          let eval = <MatEval as abzu::Evaluator<Board,Move,IValue>>
            ::evaluate_absolute(&MatEval,self, 0).0;
          write!(f, " {}{{{}}}\n", if self.in_check() {"!"} else {""}, eval)?;
        }
        _ => {}
      }
    }
    write!(f, " +---------------+\n")?;
    write!(f, "")
  }
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

#[cfg(debug_assertions)]
const CHECK_INCREMENTAL_HASH : bool = true;
#[cfg(not(debug_assertions))]
const CHECK_INCREMENTAL_HASH : bool = false;

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

  // TODO
  fn from_fen(s:&str) -> Self {
    let mut b = Board::new();
    let mut cs = s.chars();
    todo!();
    b
  }

  fn to_fen(&self) -> String {
    let mut s = String::new();
    for r in (0..5).rev() {
      let mut b = 0;
      for c in 0..5 {
        let rc = r*5 + c;
        match self.cells[rc] {
          None => { b += 1; }
          Some((p,c)) => {
            if b != 0 {
              s += &format!("{}", b);
              b = 0;
            }
            s.push(p.name(c));
          }
        }
      }
      if b != 0 {
        s += &format!("{}", b);
      }
      if r != 0 {
        s.push('/');
      }
    }
    s += &format!(" {} ", if self.side==Color::Black {'b'} else {'w'});
    if self.nhand[0] + self.nhand[1] == 0 {
      s.push('-');
    } else {
      for i in 0..5 {
        for j in 0..self.hand[0][i] {
          s.push(Piece::from(i as u8).name(Color::Black));
        }
      }
      for i in 0..5 {
        for j in 0..self.hand[1][i] {
          s.push(Piece::from(i as u8).name(Color::White));
        }
      }
    }
    s
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

  #[inline]
  fn make_null_move(&mut self) -> abzu::MoveResult {
    self.ply += 1;
    (self.side, self.xside) = (self.xside, self.side);
    self.hash ^= unsafe{HASH_SIDE_WHITE};
    self.drawhash[self.ply-1] = self.hash;
    abzu::MoveResult::OtherSideMoves
  }

  #[inline]
  fn unmake_null_move(&mut self) -> abzu::MoveResult {
    self.ply -= 1;
    (self.side, self.xside) = (self.xside, self.side);
    self.hash = self.drawhash[self.ply-1];
    abzu::MoveResult::OtherSideMoves
  }

  fn make_move(&mut self, m:Move) -> abzu::MoveResult {
    if m.nullmove() { return self.make_null_move() };

    let mut h = self.hash ^ unsafe{HASH_SIDE_WHITE};

    let s = self.side as usize;
    let f = m.f() as usize;
    let t = m.t() as usize;
    let p = m.p() as usize;
    if m.drop() {
      unsafe {
        h ^= HASH_PIECE[s][p][t];
        h ^= HASH_PIECE_HAND[s][p][self.hand[s][p]];
        h ^= HASH_PIECE_HAND[s][p][self.hand[s][p]-1];
      }
      self.cells[t] = Some((m.piece(), self.side));
      self.nhand[s] -= 1;
      self.hand[s][p] -= 1;
    } else {
      let pu = m.piece().unpromotes() as usize;
      if m.cap() {
        if Piece::from(m.p()) == Piece::King {
          self.play = if self.side==Color::Black {State::BlackWin} else {State::WhiteWin};
        } else {
          self.nhand[s] += 1;
          self.hand[s][pu] += 1;
        }
      }
      self.cells[f] = None;
      if m.pro() {
        self.cells[t] = Some((Piece::from(m.m()).promotes(), self.side));
      } else {
        self.cells[t] = Some((Piece::from(m.m()), self.side));
      }
      unsafe {
        let mm = m.m() as usize;
        if m.cap() {
          h ^= HASH_PIECE[self.xside as usize][p][t];
          if m.piece() != Piece::King {
            h ^= HASH_PIECE_HAND[s][pu][self.hand[s][pu]];
            h ^= HASH_PIECE_HAND[s][pu][self.hand[s][pu]-1];
          }
        }
        h ^= HASH_PIECE[s][mm][f];
        if m.pro() {
          h ^= HASH_PIECE[s][Piece::from(m.m()).promotes() as usize][t];
        } else {
          h ^= HASH_PIECE[s][mm][t];
        }
      }
    }
    (self.side, self.xside) = (self.xside, self.side);
    self.ply += 1;
    self.hash = h;
    if CHECK_INCREMENTAL_HASH {
      self.hash_board();
      if h != self.hash {
        eprintln!("* Error incremental hash: move = {} *", m);
      }
    }

    // draw by repetition check
    // TODO: lame slow approach
    // (replace with hash for speed)
    self.drawhash[self.ply-1] = self.hash;
    if false /* the_settings.check_draws */ {
      if self.drawhash[0..(self.ply-1)].iter().any(|&xh|xh==h) {
        self.play = State::Draw;
      }
    }
    abzu::MoveResult::OtherSideMoves
  }

  fn unmake_move(&mut self, m:Move) -> abzu::MoveResult {
    if m.nullmove() { return self.unmake_null_move() };

    self.play = State::Playing;
    let f = m.f() as usize;
    let t = m.t() as usize;
    if f < 25 {
      if m.pro() {
        self.cells[f] = Some((Piece::from(m.m()).unpromotes(), self.xside));
      } else {
        self.cells[f] = Some((Piece::from(m.m()), self.xside));
      }
    }
    if m.drop() {
      self.cells[t] = None;
       self.nhand[self.xside as usize] += 1;
       self.hand[self.xside as usize][m.p() as usize] += 1;
    } else if m.cap() {
      self.cells[t] = Some((m.piece(), self.side));
      if m.piece() != Piece::King {
       self.nhand[self.xside as usize] -= 1;
       self.hand[self.xside as usize][m.piece().unpromotes() as usize] -= 1;
      }
    } else {
      self.cells[t] = None;
    }
    (self.side, self.xside) = (self.xside, self.side);
    self.ply -= 1;
    self.hash = self.drawhash[self.ply - 1];

    if CHECK_INCREMENTAL_HASH {
      let h = self.hash;
      self.hash_board();
      if h != self.hash {
        eprintln!("* Error incremental hash: move = {} *", m);
      }
    }
    abzu::MoveResult::OtherSideMoves
  }

  fn generate_moves<const CAPS_ONLY:bool>(&self) -> Vec<Move> {
    if self.play != State::Playing { return Vec::new(); }
    let mult = if self.side == Color::Black {1} else {-1};
    let flg = if self.side == Color::Black {0} else {MF_WHITEMOVE};
    let mut ml = Vec::new();

    // generate drops
    //   drop onto empty squares
    //   can't drop pawns onto promotion squares
    //   TODO: disallow pawn drops giving checkmate
    //   TODO: disallow pawn drops with pawns
    if !CAPS_ONLY && self.nhand[self.side as usize]>0 {
      for i in 0..5 {
        if self.hand[self.side as usize][i] > 0 {
          let f = if self.side == Color::Black {B_HAND} else {W_HAND};
          if i == (Piece::Pawn as usize) {
            for t in 0..25 {
              if self.cells[t] == None && PROMOTION[t]!=Some(self.side) {
                ml.push(Move::new(f as u8, t as u8, i as u8, i as u8, flg|MF_DROP));
              }
            }
          } else {
            for t in 0..25 {
              if self.cells[t] == None {
                ml.push(Move::new(f as u8, t as u8, i as u8, i as u8, flg|MF_DROP));
              }
            }
          }
        }
      }
    }

    for f in 0..25 {
      if let Some((mvd,c)) = self.cells[f] {
        if c!=self.side {continue;}
        // generate step moves
        for i in 0..MOVE_STEP_NUM[mvd as usize] {
          let t = BOX_TO_BOARD[(BOARD_TO_BOX[f] + mult*MOVE_STEP_OFFSET[mvd as usize][i]) as usize];
          if t<0 {continue;}
          let t = t as usize;
          if self.cells[t].is_none() && CAPS_ONLY {continue;}
          if let Some((_,c)) = self.cells[t] { if c==self.side {continue;} }
          let flags;
          let p;
          if let Some((xp,_)) = self.cells[t] { flags=MF_CAP; p=xp as u8; }
          else { flags=0; p=0; }
          if (PROMOTION[f] == Some(self.side) || PROMOTION[t] == Some(self.side))
            &&  mvd.can_promote()
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
            let fp = if (PROMOTION[f] == Some(self.side) || PROMOTION[t] == Some(self.side))
                && mvd.can_promote() {MF_PRO} else {0};
            match self.cells[t as usize] {
              None => {
                if !CAPS_ONLY {
                  ml.push(Move::new(f as u8, t as u8, 0, mvd as u8, flg|fp));
                  if fp!=0 {
                    ml.push(Move::new(f as u8, t as u8, 0, mvd as u8, flg|MF_NONPRO));
                  }
                }
              }
              Some((xp,c)) if c==self.xside => {
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

  // TODO: might be more efficient to look at moves _from_ king...
  fn in_check(&self) -> bool {
    if self.play != State::Playing { return false; }
    let mult = if self.xside == Color::Black {1} else {-1};
    for f in 0..25 {
      if let Some((mvd,c)) = self.cells[f] {
        if c!=self.xside {continue;}
        // step moves
        for i in 0..MOVE_STEP_NUM[mvd as usize] {
          let t = BOX_TO_BOARD[(BOARD_TO_BOX[f] + mult*MOVE_STEP_OFFSET[mvd as usize][i]) as usize];
          if t<0 {continue;}
          if let Some(pc) = self.cells[t as usize] {
            if pc == (Piece::King, self.side) {
              return true;
            }
          }
        }
        // slide moves
        for &offset in &MOVE_SLIDE_OFFSET[mvd as usize] {
          if offset == 0 {break;}
          let mut t = f;
          loop {
            let pt = BOX_TO_BOARD[(BOARD_TO_BOX[t as usize] + offset) as usize];
            if pt<0 {break;}
            t = pt as usize;
            if let Some(pc) = self.cells[t as usize] {
              if pc == (Piece::King, self.side) {
                return true;
              }
              break;
            }
          }
        }
      }
    }
    return false;
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

////////////////////////////////////////

impl abzu::Move for Move {
  #[inline] fn to_i(self) -> u32 { self.0 }
  #[inline] fn from_i(i:u32) -> Self { Move(i) }
  #[inline] fn is_valid(&self) -> bool { true }
  #[inline] fn is_null(&self) -> bool { self.nullmove() }
  #[inline] fn null_move() -> Self { Move::null() }
}

impl abzu::Board<Move> for Board {
  #[inline] fn new() -> Self { Board::new() }
  #[inline] fn init(&mut self) { *self = Board::new() }
  #[inline] fn terminal(&self) -> bool { self.play != State::Playing }
  #[inline] fn hash(&self) -> H { self.hash }
  #[inline] fn gen_moves(&self) -> Vec<Move> { self.generate_moves::<false>() }
  #[inline] fn gen_q_moves(&self) -> Vec<Move> { self.generate_moves::<true>() }
  #[inline] fn make_move(&mut self, m:Move) -> abzu::MoveResult { self.make_move(m) }
  #[inline] fn unmake_move(&mut self, m:Move) -> abzu::MoveResult { self.unmake_move(m) }
}

////////////////////////////////////////

use abzu::{Value};
use abzu::value::{IValue};
pub struct MatEval;
const MAT_VALUE : [i32;10] = [
  100, 300, 200, 400, 500, 1000, 150, 250, 500, 600
];
const MAT_VALUE_HAND : [i32;5] = [
  100*11/12, 300*11/12, 200*11/12, 400*11/12, 500*11/12,
];

impl abzu::Evaluator<Board,Move,IValue> for MatEval {
  fn evaluate_absolute(&self, b:&Board, ply:usize) -> IValue {
    match b.play {
      State::Draw => {return IValue::DRAW; }
      State::BlackWin => {return IValue::mate_in_n(ply); }
      State::WhiteWin => {return -IValue::mate_in_n(ply); }
      State::Playing => {}
    }
    let mut v = 0;
    for &c in b.cells.iter() {
      match c {
        Some((p,Color::Black)) => { v += MAT_VALUE[p as usize]; }
        Some((p,Color::White)) => { v -= MAT_VALUE[p as usize]; }
        _ => {}
      }
    }
    for i in 0..5 { v += (b.hand[0][i] as i32)*MAT_VALUE_HAND[i]; }
    for i in 0..5 { v -= (b.hand[1][i] as i32)*MAT_VALUE_HAND[i]; }
    IValue(v)
  }
  fn evaluate_relative(&self, b:&Board, ply:usize) -> IValue {
    match b.side {
      Color::Black => self.evaluate_absolute(b, ply),
      Color::White => -self.evaluate_absolute(b, ply),
    }
  }
  fn stalemate_absolute(&self, b:&Board, ply:usize) -> IValue {
    match b.side {
      Color::Black => -IValue::mate_in_n(ply),
      Color::White => IValue::mate_in_n(ply),
    }
  }
  fn stalemate_relative(&self, b:&Board, ply:usize) -> IValue {
    match b.side {
      Color::Black => self.stalemate_absolute(b, ply),
      Color::White => -self.stalemate_absolute(b, ply),
    }
  }
}

////////////////////////////////////////

struct GameIMat;
impl abzu::Game for GameIMat {
  type M = Move;
  type B = Board;
  type V = IValue;
  type E = MatEval;
}

fn main() {
  let mut hg = abzu::hash::HashGen::new(1);
  unsafe{gen_hash(&mut hg)};
  let mut b = Board::new();

  println!("{}", b);
  for m in b.generate_moves::<true>() { print!("{} ", m); } println!("");
  println!("**********");

  let eval = MatEval;
  let mut pv = abzu::pv::PV::new(16);
  let mut tt = abzu::tt::Table::new(1024);
  let mut stats = abzu::search::Stats::new();
  let mut settings =
    abzu::search::Settings {
      depth: 400,
      hh: None,
      tt: Some(abzu::search::TT{}),
      id: None,
      iid: None,
      mws: None,
      asp: None,
      qs: None,
      nmp: None,
    };
  while !abzu::Board::<Move>::terminal(&b) {
    println!("\n{}", b);
    let (v,mpv) = abzu::search::negamax::search_negamax::<GameIMat>(
      &mut b, &eval,
      &mut pv, &mut tt,
      &settings, &mut stats);
    print!("M={} ( ", v.0); for &m in mpv.iter() { print!("{} ", m); } println!(")");
    println!("{:?}", stats);
    b.make_move(mpv[0]);
  }
  println!("\n{}", b);
}

