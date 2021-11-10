#![feature(destructuring_assignment)]

use abzu::hash::H;

#[cfg(debug_assertions)]
const DEBUG_MODE : bool = true;
#[cfg(not(debug_assertions))]
const DEBUG_MODE : bool = false;

const CHECK_INCREMENTAL_HASH : bool = DEBUG_MODE;

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
  u8,into Piece, moved,_    : 23,18; // moved piece
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

fn mpv(p:Piece) -> isize {
  use Piece::*;
  match p {
    Pawn => 1,
    Tokin => 2,
    Silver => 3,
    Nari => 4,
    Gold => 5,
    Bishop => 6,
    Rook => 7,
    Horse => 8,
    Dragon => 9,
    King => 50,
  }
}

fn move_sort_key(m:&Move) -> isize {
  -(
  (if m.cap() {1000} else {0})
  + (if m.pro() {100} else if m.nonpro() {-10} else {0})
  + (if m.drop() {-5} else {0})
  + (if m.cap() {mpv(m.piece())*100 - mpv(m.moved())} else {0})
  )
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

const MAX_GAME_LENGTH : usize = 256;

////////////////////////////////////////

#[derive(Clone,Copy,Debug,Eq,PartialEq)]
pub enum Color {
  Black = 0,
  White = 1,
}

impl std::fmt::Display for Color {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    write!(f, "{}",
      match self {
        Color::Black => 'B',
        Color::White => 'W',
      }
    )
  }
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

  pub fn from_char(ch:char) -> Option<(Piece,Color)> {
    use Color::*;
    use Piece::*;
    match ch {
      'P' => Some((Pawn,Black)),
      'G' => Some((Gold,Black)),
      'S' => Some((Silver,Black)),
      'B' => Some((Bishop,Black)),
      'R' => Some((Rook,Black)),
      'K' => Some((King,Black)),
      'T' => Some((Tokin,Black)),
      'N' => Some((Nari,Black)),
      'H' => Some((Horse,Black)),
      'D' => Some((Dragon,Black)),
      'p' => Some((Pawn,White)),
      'g' => Some((Gold,White)),
      's' => Some((Silver,White)),
      'b' => Some((Bishop,White)),
      'r' => Some((Rook,White)),
      'k' => Some((King,White)),
      't' => Some((Tokin,White)),
      'n' => Some((Nari,White)),
      'h' => Some((Horse,White)),
      'd' => Some((Dragon,White)),
      _ => None,
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
    write!(f, "    5   4   3   2   1  \n")?;
    write!(f, " +--------------------+ [{:016X}]\n", self.hash)?;
    for r in (0..5).rev() {
      write!(f, " |")?;
      for c in 0..5 {
        let rc = r*5 + c;
        if let Some((p,c)) = self.cells[rc] {
          //write!(f, " {} ", Piece::from(p).name(c))?;
          write!(f, " {}{}{} ",
            if c==Color::Black {format!("{}{}",abzu::ansi::BRIGHT,abzu::ansi::FG_WHITE)}
            else {format!("{}{}",abzu::ansi::BRIGHT,abzu::ansi::FG_GREEN)},
            ["歩", "金", "銀", "角", "飛",
             "王", "と", "成", "馬", "竜",
            ][p as usize],
            "\x1b[0m"
            )?;
        } else {
          write!(f, " .. ")?;
        }
      }
      write!(f, "| ({})", ['e','d','c','b','a'][r])?;
      match r {
        0 => {
          write!(f, " {}.{}\n", (self.ply+1)/2, (if self.side==Color::Black{""}else{".."}))?;
        }
        1 => {
          write!(f, " Black({})[", self.nhand[Color::Black as usize])?;
          for i in 0..5 {
            if self.hand[Color::Black as usize][i] > 0 {
              for _ in 0..self.hand[Color::Black as usize][i] {
                write!(f, " {}", Piece::from(i as u8).name(Color::Black))?;
              }
            }
          }
          write!(f, " ]\n")?;
        }
        2 => {
          write!(f, " White({})[", self.nhand[Color::White as usize])?;
          for i in 0..5 {
            if self.hand[Color::White as usize][i] > 0 {
              for _ in 0..self.hand[Color::White as usize][i] {
                write!(f, " {}", Piece::from(i as u8).name(Color::White))?;
              }
            }
          }
          write!(f, " ]\n")?;
        }
        3 => {
          write!(f, " \"{}\"\n", self.to_fen())?;
        }
        4 => {
          let eval = <MatEval as abzu::Evaluator<Board,Move,FValue>>
            ::evaluate_absolute(&MatEval::new(false,true),self, 0).0;
          write!(f, " {:?} {}{{{}}}\n",
            self.play, if self.in_check() {"!"} else {""}, eval)?;
        }
        _ => {}
      }
    }
    write!(f, " +--------------------+\n")?;
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

  // TODO: not robust against bad FEN input
  fn from_fen(s:&str) -> Result<Self,String> {
    let mut b = Board::new();
    let mut bk = false;
    let mut wk = false;
    let mut cs = s.chars().peekable();
    let mut row = 4;
    let mut col = 0;
    loop {
      match cs.next() {
        Some(' ') => { break; }
        Some('/') => { row-=1; col=0; }
        Some('1') => { for _ in 0..1 { b.cells[row*5 + col] = None; col+=1; } }
        Some('2') => { for _ in 0..2 { b.cells[row*5 + col] = None; col+=1; } }
        Some('3') => { for _ in 0..3 { b.cells[row*5 + col] = None; col+=1; } }
        Some('4') => { for _ in 0..4 { b.cells[row*5 + col] = None; col+=1; } }
        Some('5') => { for _ in 0..5 { b.cells[row*5 + col] = None; col+=1; } }
        Some(ch) => {
          if let Some((p,c)) = Piece::from_char(ch) {
            b.cells[row*5 + col] = Some((p,c)); col+=1;
            if (p,c) == (Piece::King,Color::Black) {
              bk = true;
            } else if (p,c) == (Piece::King,Color::White) {
              wk = true;
            }
          } else {
            return Err(format!("Malformed FEN string '{}'", s));
          }
        }
        _ => { return Err(format!("Malformed FEN string '{}'", s)); }
      }
    }
    match cs.next() {
      Some('b') => {
        b.side = Color::Black;
        b.xside = Color::White;
      }
      Some('w') => {
        b.side = Color::White;
        b.xside = Color::Black;
      }
      _ => { return Err(format!("Malformed FEN string '{}'", s)); }
    }
    cs.next(); // consume ' '
    if cs.peek() == Some(&'-') {
      cs.next();
    } else {
      for ch in cs {
        match Piece::from_char(ch) {
          Some((p,c)) if c==Color::Black => {
            b.nhand[0] += 1;
            b.hand[0][p as usize] += 1;
          }
          Some((p,c)) if c==Color::White => {
            b.nhand[1] += 1;
            b.hand[1][p as usize] += 1;
          }
          _ => { return Err(format!("Malformed FEN string '{}'", s)); }
        }
      }
    }
    b.hash_board();
    b.drawhash[0] = b.hash;
    match (bk,wk) {
      (true,true) => { b.play = State::Playing; }
      (true,false) => { b.play = State::BlackWin; }
      (false,true) => { b.play = State::WhiteWin; }
      (false,false) => { b.play = State::Draw; }
    }
    Ok(b)
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
        for _ in 0..self.hand[0][i] {
          s.push(Piece::from(i as u8).name(Color::Black));
        }
      }
      for i in 0..5 {
        for _ in 0..self.hand[1][i] {
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

    let mut h = self.hash;
    let s = self.side as usize;
    let f = m.f() as usize;
    let t = m.t() as usize;
    let p = m.p() as usize;
    let mm = m.m() as usize;
    if m.drop() {
      unsafe {
        h ^= HASH_PIECE[s][p][t];
        h ^= HASH_PIECE_HAND[s][p][self.hand[s][p]];
        h ^= HASH_PIECE_HAND[s][p][self.hand[s][p]-1];
      }
      if DEBUG_MODE {
        if !self.cells[t].is_none() {
          eprintln!("{}\n{}", self, m);
          panic!("** Invalid move: drop onto non-empty square **");
        }
      }
      self.cells[t] = Some((m.piece(), self.side));
      self.nhand[s] -= 1;
      self.hand[s][p] -= 1;
    } else {
      let pu = m.piece().unpromotes() as usize;
      if m.cap() {
        if m.piece() == Piece::King {
          self.play = if self.side==Color::Black {State::BlackWin} else {State::WhiteWin};
        } else {
          self.nhand[s] += 1;
          self.hand[s][pu] += 1;
          h ^= unsafe{HASH_PIECE_HAND[s][pu][self.hand[s][pu]]};
          h ^= unsafe{HASH_PIECE_HAND[s][pu][self.hand[s][pu]-1]};
        }
        h ^= unsafe{HASH_PIECE[self.xside as usize][p][t]};
      }
      self.cells[f] = None;
      h ^= unsafe{HASH_PIECE[s][mm][f]};
      if m.pro() {
        self.cells[t] = Some((Piece::from(m.m()).promotes(), self.side));
        h ^= unsafe{HASH_PIECE[s][Piece::from(m.m()).promotes() as usize][t]};
      } else {
        self.cells[t] = Some((Piece::from(m.m()), self.side));
        h ^= unsafe{HASH_PIECE[s][mm][t]};
      }
    }

    (self.side, self.xside) = (self.xside, self.side);
    h ^= unsafe{HASH_SIDE_WHITE};

    self.ply += 1;
    self.hash = h;
    if CHECK_INCREMENTAL_HASH {
      self.hash_board();
      if h != self.hash {
        eprintln!("* make_move(): Error incremental hash: move = {} *\n{}", m, self);
      }
    }

    // draw by repetition check
    // TODO: lame slow approach
    // (replace with hash for speed)
    self.drawhash[self.ply-1] = self.hash;
    if true /* the_settings.check_draws */ {
      if self.drawhash[0..(self.ply-1)].iter().any(|&xh|xh==h) {
        self.play = State::Draw;
      }
    }
    if self.ply == MAX_GAME_LENGTH-1 {
      self.play = State::Draw;
    }
    abzu::MoveResult::OtherSideMoves
  }

  fn unmake_move(&mut self, m:Move) -> abzu::MoveResult {
    if m.nullmove() { return self.unmake_null_move() };

    self.play = State::Playing;
    let f = m.f() as usize;
    let t = m.t() as usize;
    if f < 25 {
      self.cells[f] = Some((Piece::from(m.m()), self.xside));
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
        eprintln!("* unmake_move(): Error incremental hash: move = {} *", m);
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
    if !CAPS_ONLY && self.nhand[self.side as usize]>0 {
      for i in 0..5 {
        if self.hand[self.side as usize][i] > 0 {
          let f = if self.side == Color::Black {B_HAND} else {W_HAND};
          if i == (Piece::Pawn as usize) {
            for t in 0..25 {
              if self.cells[t] == None && PROMOTION[t]!=Some(self.side) {
                // no drop in same column as existing friendly pawn
                for tx in 0..5 {
                  if self.cells[tx*5 + t%5] == Some((Piece::Pawn,self.side)) {
                    continue;
                  }
                }
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
    ml.sort_unstable_by_key(move_sort_key);
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
  #[inline] fn to_fen(&self) -> String { self.to_fen() }
  #[inline] fn from_fen(fen:&str) -> Result<Self,String> { Board::from_fen(fen) }
  #[inline] fn terminal(&self) -> bool { self.play != State::Playing }
  #[inline] fn to_move(&self) -> abzu::Side {
    match self.side {
      Color::Black => abzu::Side::First,
      Color::White => abzu::Side::Second,
    }
  }
  #[inline] fn game_result(&self) -> abzu::GameResult {
    match self.play {
      State::Playing => abzu::GameResult::NoResult,
      State::BlackWin => abzu::GameResult::FirstWin,
      State::WhiteWin => abzu::GameResult::SecondWin,
      State::Draw => abzu::GameResult::Draw,
    }
  }
  #[inline] fn hash(&self) -> H { self.hash }
  #[inline] fn gen_moves(&self) -> Vec<Move> { self.generate_moves::<false>() }
  #[inline] fn gen_q_moves(&self) -> Vec<Move> { self.generate_moves::<true>() }
  #[inline] fn make_move(&mut self, m:Move) -> abzu::MoveResult { self.make_move(m) }
  #[inline] fn unmake_move(&mut self, m:Move) -> abzu::MoveResult { self.unmake_move(m) }
}

////////////////////////////////////////

use abzu::value::{FValue,Value};
const MAT_VALUE : [f32;10] = [
  100.0, 300.0, 200.0, 400.0, 500.0, 1000.0, 150.0, 250.0, 500.0, 600.0,
];
const MAT_VALUE_HAND : [[f32;3];5] = [
  [0.0, 100.0/2.0, 100.0],
  [0.0, 300.0/2.0, 300.0],
  [0.0, 200.0/2.0, 200.0],
  [0.0, 400.0/2.0, 400.0],
  [0.0, 500.0/2.0, 500.0],
];
pub struct MatEval {
  random:bool,
  use_piece_square:bool,
  mat_value: [f32;10],
  mat_value_hand: [[f32;3];5],
  piece_square: [[f32;10];25]
}

impl MatEval {
  pub fn new(random:bool, use_piece_square:bool) -> Self {
    MatEval {
      random,
      use_piece_square,
      mat_value: MAT_VALUE.clone(),
      mat_value_hand: MAT_VALUE_HAND.clone(),
      piece_square: [[0.0; 10]; 25],
    }
  }
}

impl abzu::Evaluator<Board,Move,FValue> for MatEval {
  fn evaluate_absolute(&self, b:&Board, ply:usize) -> FValue {
    match b.play {
      State::Draw => {return FValue::DRAW; }
      State::BlackWin => {return FValue::mate_in_n(ply); }
      State::WhiteWin => {return -FValue::mate_in_n(ply); }
      State::Playing => {}
    }
    let mut v = 0.0;
    for &c in b.cells.iter() {
      match c {
        Some((p,Color::Black)) => { v += self.mat_value[p as usize]; }
        Some((p,Color::White)) => { v -= self.mat_value[p as usize]; }
        _ => {}
      }
    }
    for i in 0..5 { v += self.mat_value_hand[i][b.hand[0][i]]; }
    for i in 0..5 { v -= self.mat_value_hand[i][b.hand[1][i]]; }
    if self.use_piece_square {
      for i in 0..25 {
        match b.cells[i] {
          Some((p,c)) if c==Color::Black => {
            v += self.piece_square[i][p as usize];
          }
          Some((p,c)) if c==Color::White => {
            v -= self.piece_square[24-i][p as usize];
          }
          _ => {}
        }
      }
    }
    if self.random { v += (((b.hash%15) as f32)-7.0)/15.0; }
    FValue(v)
  }
  fn evaluate_relative(&self, b:&Board, ply:usize) -> FValue {
    match b.side {
      Color::Black => self.evaluate_absolute(b, ply),
      Color::White => -self.evaluate_absolute(b, ply),
    }
  }
  fn stalemate_absolute(&self, b:&Board, ply:usize) -> FValue {
    match b.side {
      Color::Black => -FValue::mate_in_n(ply),
      Color::White => FValue::mate_in_n(ply),
    }
  }
  fn stalemate_relative(&self, b:&Board, ply:usize) -> FValue {
    match b.side {
      Color::Black => self.stalemate_absolute(b, ply),
      Color::White => -self.stalemate_absolute(b, ply),
    }
  }

  fn num_weights(&self) -> usize {
    10 + 5*3 + 25*10
  }
  fn get_weight_f32(&self, j:usize) -> f32 {
    if j<10 {
      self.mat_value[j]
    } else if j<25 {
      self.mat_value_hand[(j-10)/3][(j-10)%3]
    } else {
      self.piece_square[(j-25)/10][(j-25)%10]
    }
  }
  fn set_weight_f32(&mut self, j:usize, wj:f32) {
    if j<10 {
      self.mat_value[j] = wj;
    } else if j<25 {
      self.mat_value_hand[(j-10)/3][(j-10)%3] = wj;
    } else {
      self.piece_square[(j-25)/10][(j-25)%10] = wj;
    }
  }
  fn get_all_weights_f32(&self) -> Vec<f32> {
    let mut v = vec![0.0; 10+5*3+25*10];
    for j in 0..(10+5*3+25*10) {
      v[j] = self.get_weight_f32(j);
    }
    v
  }
  fn normalize_weights(&mut self) {
    // force Pawn value == 100
    self.mat_value[Piece::Pawn as usize] = 100.0;
    for k in 0..5 {
      self.mat_value_hand[k][0] = 0.0;
    }
    // force piece-square values to be net==0
    for k in 0..10 {
      let mut sum = 0.0;
      for j in 0..25 { sum += self.piece_square[j][k]; }
      sum /= 25.0;
      for j in 0..25 { self.piece_square[j][k] -= sum; }
    }
  }
}

////////////////////////////////////////

impl abzu::Color for Color { }

////////////////////////////////////////

#[derive(Clone,Copy,Debug,Default)]
struct GameMat;

impl abzu::Game for GameMat {
  type M = Move;
  type B = Board;
  type V = FValue;
  type E = MatEval;
  type C = Color;
  const FIRST : Color = Color::Black;
  const SECOND : Color = Color::White;
}


fn play_game(eval:&mut MatEval,
  bsettings:&abzu::search::Settings<FValue>, wsettings:&abzu::search::Settings<FValue>)
  -> abzu::record::GameRecord<GameMat>
{
  let mut tt = abzu::tt::Table::new(1024*7-1);
  let mut stats = abzu::search::Stats::new();
  let mut pv = abzu::pv::PV::new(16);

  let mut b = Board::new();
  let mut gr = abzu::record::GameRecord::new("αβ".into(),"αβ".into());
  while !abzu::Board::<Move>::terminal(&b) {
    let settings = if b.side==Color::Black {bsettings} else {wsettings};
    let ply = abzu::search::alphabeta::search_alphabeta::<GameMat>(
      &mut b, &eval,
      &mut pv, &mut tt,
      &settings, &mut stats);
    b.make_move(ply.m);
    gr.add_ply(ply);
  }
  gr
}

fn show_weights(eval:&MatEval) {
  let ws = eval.get_all_weights_f32();
  print!("{}", abzu::ansi::CLEAR_SCREEN);
  print!("{}", abzu::ansi::BG_RGB(1,1,1));

  for p in 0..10 {
    for i in 0..25 {
      let w = eval.piece_square[i][p];
      print!("{}", abzu::ansi::CURSOR_POSITION(5-(i/5), 6*p+1+i%5));
      if w < -0.01 {
        print!("{}{}", abzu::ansi::BG_RGB(200,25,50), ((-w).floor() as usize).min(9));
      } else if w > 0.01 {
        print!("{}{}", abzu::ansi::BG_RGB(50,25,200), (w.floor() as usize).min(9));
      } else {
        print!("{}{}", abzu::ansi::BG_RGB(50,50,50), 0);
      }
    }
    print!("{}", abzu::ansi::RESET);
    print!("{}{}", abzu::ansi::CURSOR_POSITION(6, 6*p+1), Piece::from(p as u8).name(Color::Black));
  }

  println!("{}", abzu::ansi::RESET);
  for w in ws[0..10].iter() { print!(" {:.2}", w); }
  println!("");
  for w in ws[11..25].iter() { print!(" {:.2}", w); }
  println!("");
}

use abzu::Evaluator;
fn main() {

  let mut hg = abzu::hash::HashGen::new(1);
  unsafe{gen_hash(&mut hg)};

  //for i in (0..255).step_by(10) { println!("\x1b[38;2;{};177;249mHELLO",i); }

  let bsettings = abzu::search::Settings::new(500).tt(Some(abzu::search::TT{}));
  let wsettings = abzu::search::Settings::new(400).tt(Some(abzu::search::TT{}));
  let mut eval = MatEval::new(true,true);

  //println!("{}", play_game(&mut eval, &bsettings, &wsettings));
  //return;

  let mut tdl = abzu::td_lambda::TDLambda::new(eval.num_weights(),
    0.9, 10.0, 0.5/100.0, 1.0, true);
  show_weights(&eval);
  for n in 0..100 {
    unsafe{gen_hash(&mut hg)};
    let gs : Vec<_> =
      (0..100).map(|_|play_game(&mut eval, &bsettings, &wsettings)).collect();
    for g in gs.iter() {
      tdl.process_game(&mut eval, &g);
    }
    if (n+1)%100 == 0 {
      show_weights(&eval);
    }
  }
}


