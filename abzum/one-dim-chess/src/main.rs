
////////////////////////////////////////

#[derive(Clone,Copy,Debug,Eq,PartialEq)]
pub enum Piece {
  Knight,
  Rook,
  King,
}

impl From<u8> for Piece {
  #[inline]
  fn from(x:u8) -> Self {
    unsafe{std::mem::transmute(x)}
  }
}

impl std::fmt::Display for Piece {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    write!(f, "{}",
      match self {
        Piece::Knight=>'N',
        Piece::Rook=>'R',
        Piece::King=>'K',
      }
    )
  }
}

////////////////////////////////////////

#[derive(Clone,Copy,Debug,Eq,PartialEq)]
pub enum Color {
  White,
  Black,
}

impl std::ops::Not for Color {
  type Output = Color;
  fn not(self) -> Self {
    match self {
      Color::White => Color::Black,
      Color::Black => Color::White,
    }
  }
}

impl std::fmt::Display for Color {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    write!(f, "{}",
      match self {
        Color::White=>'W',
        Color::Black=>'B',
      }
    )
  }
}

////////////////////////////////////////

bitfield::bitfield! {
  #[derive(Clone,Copy,Eq,PartialEq)]
  pub struct Move(u32);
  //impl Debug;
  u8,f,_    : 5,0; // from
  u8,t,_    : 11,6; // to
  u8,m,_               : 14,12; // moved piece
  u8,into Piece, mov,_ : 14,12; // moved piece
  u8,p,_                 : 17,15; // captured piece
  u8,into Piece, piece,_ : 17,15; // captured piece
  cap,_      : 30;
  nullmove,_ : 31;
}

impl Move {
  fn new(f:u8, t:u8, m:u8, p:u8, cap:bool, nullmove:bool) -> Self {
    Move((f as u32)
      | ((t as u32) << 6)
      | ((m as u32) << 12)
      | ((p as u32) << 15)
      | (if cap {1<<30} else {0})
      | (if nullmove {1<<31} else {0})
      )
  }
}

impl Default for Move {
  #[inline] fn default() -> Self { Move(0) }
}

impl std::fmt::Display for Move {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    write!(f, "{}{}{}{}",
      Piece::from(self.mov()),
      char::from(97 + self.f()),
      if self.cap() {'x'} else {'-'},
      char::from(97 + self.t())
    )
  }
}
 
////////////////////////////////////////

static mut HASH_WHITE : abzu::hash::H = 0;
static mut HASH_PIECE : [[abzu::hash::H;3];2] = [[0;3];2];

unsafe fn init_hash(hg:&mut abzu::hash::HashGen) {
  HASH_WHITE = hg.next();
  for i in 0..2 {
    for j in 0..3 {
      HASH_PIECE[i][j] = hg.next();
    }
  }
}

////////////////////////////////////////

#[derive(Clone,Copy,Debug,Eq,PartialEq)]
enum GameState {
  Playing,
  Win(Color),
  Draw,
}

impl std::fmt::Display for GameState {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    match self {
      GameState::Playing => { write!(f, "...") }
      GameState::Win(Color::White) => { write!(f, "1-0") }
      GameState::Win(Color::Black) => { write!(f, "0-1") }
      GameState::Draw => { write!(f, "1/2") }
    }
  }
}

////////////////////////////////////////

#[derive(Clone,Debug)]
pub struct Board<const N:usize> {
  cells: [Option<(Piece,Color)>; N],
  to_move: Color,
  state: GameState,
  ply: usize,
  hash: abzu::hash::H,
  drawhash: [abzu::hash::H; 64],
}

impl<const N:usize> Board<N> {
  fn new() -> Self {
    let mut b = Board {
      cells: [None; N],
      to_move: Color::White,
      state: GameState::Playing,
      ply: 0,
      hash: 0,
      drawhash: [0; 64],
    };
    b.cells[0] = Some((Piece::King,Color::White));
    b.cells[1] = Some((Piece::Knight,Color::White));
    b.cells[2] = Some((Piece::Rook,Color::White));
    b.cells[N-1] = Some((Piece::King,Color::Black));
    b.cells[N-2] = Some((Piece::Knight,Color::Black));
    b.cells[N-3] = Some((Piece::Rook,Color::Black));
    b.hash = b.compute_hash();
    b.drawhash[b.ply] = b.hash;
    b
  }

  fn compute_hash(&mut self) -> abzu::hash::H {
    unsafe {
      let mut h = if self.to_move == Color::White {HASH_WHITE} else {0};
      for i in 0..N {
        match self.cells[i] {
          Some((p,c)) => { h ^= HASH_PIECE[c as usize][p as usize].rotate_left(i as u32); }
          None => {}
        }
      }
      h
    }
  }

  // TODO: validate for check
  // (for chess-style play, rather than "capture-the-king")
  fn gen_moves(&self) -> Vec<Move> {
    let mut ml = Vec::new();
    for f in 0..N {
      match self.cells[f] {
        Some((Piece::Knight,c)) if c==self.to_move => {
          if f>=2 {
            match self.cells[f-2] {
              Some((p,x)) if x==!self.to_move => {
                ml.push(Move::new(f as u8, (f-2) as u8, Piece::Knight as u8, p as u8, true, false));
              }
              None => {
                ml.push(Move::new(f as u8, (f-2) as u8, Piece::Knight as u8, 0, false, false));
              }
              _ => {}
            }
          }
          if f<=N-3 {
            match self.cells[f+2] {
              Some((p,x)) if x==!self.to_move => {
                ml.push(Move::new(f as u8, (f+2) as u8, Piece::Knight as u8, p as u8, true, false));
              }
              None => {
                ml.push(Move::new(f as u8, (f+2) as u8, Piece::Knight as u8, 0, false, false));
              }
              _ => {}
            }
          }
        }
        Some((Piece::King,c)) if c==self.to_move => {
          if f>=1 {
            match self.cells[f-1] {
              Some((p,x)) if x==!self.to_move => {
                ml.push(Move::new(f as u8, (f-1) as u8, Piece::King as u8, p as u8, true, false));
              }
              None => {
                ml.push(Move::new(f as u8, (f-1) as u8, Piece::King as u8, 0, false, false));
              }
              _ => {}
            }
          }
          if f<=N-2 {
            match self.cells[f+1] {
              Some((p,x)) if x==!self.to_move => {
                ml.push(Move::new(f as u8, (f+1) as u8, Piece::King as u8, p as u8, true, false));
              }
              None => {
                ml.push(Move::new(f as u8, (f+1) as u8, Piece::King as u8, 0, false, false));
              }
              _ => {}
            }
          }
        }
        Some((Piece::Rook,c)) if c==self.to_move => {
          for t in (0..f).rev() {
            match self.cells[t] {
              Some((p,x)) if x==!self.to_move => {
                ml.push(Move::new(f as u8, t as u8, Piece::Rook as u8, p as u8, true, false));
                break;
              }
              None => {
                ml.push(Move::new(f as u8, t as u8, Piece::Rook as u8, 0, false, false));
              }
              _ => {break;}
            }
          }
          for t in (f+1)..N {
            match self.cells[t] {
              Some((p,x)) if x==!self.to_move => {
                ml.push(Move::new(f as u8, t as u8, Piece::Rook as u8, p as u8, true, false));
                break;
              }
              None => {
                ml.push(Move::new(f as u8, t as u8, Piece::Rook as u8, 0, false, false));
              }
              _ => {break;}
            }
          }
        }
        _ => {}
      }
    }
    ml
  }

  fn make_move(&mut self, m:Move) -> abzu::MoveResult {
    let s = self.to_move as usize;
    if m.nullmove() {
      /* nop */
    } else if m.cap() {
      self.hash ^= unsafe{HASH_PIECE[s][m.m() as usize].rotate_left(m.f() as u32)};
      self.hash ^= unsafe{HASH_PIECE[s][m.m() as usize].rotate_left(m.t() as u32)};
      self.hash ^= unsafe{HASH_PIECE[1-s][m.p() as usize].rotate_left(m.t() as u32)};
      self.cells[m.t() as usize] = self.cells[m.f() as usize];
      self.cells[m.f() as usize] = None;
      if m.p() == Piece::King as u8 {
        self.state = GameState::Win(self.to_move);
      }
    } else {
      self.hash ^= unsafe{HASH_PIECE[s][m.m() as usize].rotate_left(m.f() as u32)};
      self.hash ^= unsafe{HASH_PIECE[s][m.m() as usize].rotate_left(m.t() as u32)};
      self.cells[m.t() as usize] = self.cells[m.f() as usize];
      self.cells[m.f() as usize] = None;
    }
    self.hash ^= unsafe{HASH_WHITE};
    self.to_move = !self.to_move;
    self.ply += 1;
    let h = self.compute_hash();
    if h != self.hash { eprintln!("!!"); }
    self.drawhash[self.ply] = self.hash;

    // treat any repeat as a draw
    if self.state == GameState::Playing {
      for i in (0..self.ply).rev() {
        if self.drawhash[i] == self.hash {
          self.state = GameState::Draw;
        }
      }
    }
    abzu::MoveResult::OtherSideMoves
  }

  fn unmake_move(&mut self, m:Move) -> abzu::MoveResult {
    let s = !self.to_move as usize;
    if m.nullmove() {
      /* nop */
    } else if m.cap() {
      self.hash ^= unsafe{HASH_PIECE[s][m.m() as usize].rotate_left(m.f() as u32)};
      self.hash ^= unsafe{HASH_PIECE[s][m.m() as usize].rotate_left(m.t() as u32)};
      self.hash ^= unsafe{HASH_PIECE[1-s][m.p() as usize].rotate_left(m.t() as u32)};
      self.cells[m.f() as usize] = self.cells[m.t() as usize];
      self.cells[m.t() as usize] = Some((m.piece(),self.to_move));
    } else {
      self.hash ^= unsafe{HASH_PIECE[s][m.m() as usize].rotate_left(m.f() as u32)};
      self.hash ^= unsafe{HASH_PIECE[s][m.m() as usize].rotate_left(m.t() as u32)};
      self.cells[m.f() as usize] = self.cells[m.t() as usize];
      self.cells[m.t() as usize] = None;
    }
    self.to_move = !self.to_move;
    self.ply -= 1;
    self.hash = self.drawhash[self.ply];
    self.state = GameState::Playing;
    abzu::MoveResult::OtherSideMoves
  }

  fn is_terminal(&self) -> bool {
    self.state != GameState::Playing
  }
}

impl<const N:usize> std::fmt::Display for Board<N> {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    write!(f, " ")?;
    for i in 0..(N as u8) { write!(f, " {} ", char::from(97+i))?; }
    write!(f, "\n")?;

    write!(f, "[")?;
    for i in 0..N {
      match self.cells[i] {
        None => write!(f, " __")?,
        Some((p,c)) => write!(f, " {}{}", c, p)?,
      }
    }
    write!(f, " ] {} ", self.state)?;
    write!(f, " {} ({:016x})", self.to_move, self.hash)
  }
}

////////////////////////////////////////

impl abzu::Move for Move {
  fn to_i(self) -> u32 { self.0 }
  fn from_i(i:u32) -> Self { Move(i) }
  fn is_valid(&self) -> bool { true }
  fn is_null(&self) -> bool { self.nullmove() }
  fn null_move() -> Self { Move::new(0,0,0,0,false,true) }
}

impl<const N:usize> abzu::Board<Move> for Board<N> {
  fn new() -> Self { Board::new() }
  fn init(&mut self) { *self = Board::new(); }
  fn terminal(&self) -> bool { self.is_terminal() }
  fn hash(&self) -> abzu::hash::H { self.hash }

  fn gen_moves(&self) -> Vec<Move> {
    self.gen_moves()
  }
  fn gen_q_moves(&self) -> Vec<Move> {
    todo!()
  }
  fn make_move(&mut self, m:Move) -> abzu::MoveResult {
    self.make_move(m)
  }
  fn unmake_move(&mut self, m:Move) -> abzu::MoveResult {
    self.unmake_move(m)
  }
}

////////////////////////////////////////

fn main() {
  let mut hg = abzu::hash::HashGen::new(13);
  unsafe{init_hash(&mut hg)};
  let mut ms = vec![];
  let mut b = Board::<8>::new();
  println!("-----");
  println!("{}", b);
  while b.state == GameState::Playing {
    let ml = b.gen_moves();
    let m = ml[(hg.next() as usize) % ml.len()];
    ms.push(m);
    b.make_move(m);
    println!("{}\n{}", m, b);
  }
  println!("-----");
  for &m in ms.iter().rev() {
    b.unmake_move(m);
    println!("{}\n{}", m, b);
  }
  println!("-----");
}
