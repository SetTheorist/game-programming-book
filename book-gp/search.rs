enum MoveResult {
  InvalidMove,
  SameSideMovesAgain,
  OtherSideMoves
}

#[deriving(Show,Eq)]
struct Movet(u32);

#[deriving(Show,Eq,Ord)]
struct Evalt(i32);

impl Neg<Evalt> for Evalt {
  fn neg(&self) -> Evalt {
    match *self {
      Evalt(v) => Evalt(-v)
    }
  }
}

//let b_infinity = -1000000;

trait Evaluator<B:Board> {
  fn evaluate(&self, b:&mut B, depth:int, ply:int) -> Evalt;
}

trait Board {
  fn gen_moves(&self) -> ~[Movet];
  fn make_move(&mut self, Movet) -> MoveResult;
  fn unmake_move(&mut self, Movet) -> MoveResult;
}

fn search_negamax
  <E:Evaluator<B>,B:Board+std::fmt::Show>
  (e:&E, b:&mut B, depth : int)
  -> (Evalt, ~[Movet])
{
  let mut best_val = Evalt(-1000000);
  let mut made_valid_move = false;
  let ml = b.gen_moves();
  if ml.len()==0 {
    // stalemate is loss
    return (Evalt(-1000000), box []);
  }
  for m in ml.iter() {
    //println!("Trying move {} / {}", *m, *b);
    let v = match b.make_move(*m) {
      InvalidMove          =>
        { continue }
      SameSideMovesAgain =>
        { search_negamax_general(e, b, depth-100, 1) }
      _ =>
        { -search_negamax_general(e, b, depth-100, 1) }
    };
    made_valid_move = true;
    b.unmake_move(*m);
    if v>best_val { best_val = v; }
  }
  if !made_valid_move {
    // stalemate is loss
    return (Evalt(-1000000), box []);
  }
  (best_val,box [])
}

fn search_negamax_general<E:Evaluator<B>,B:Board>(e:&E, b:&mut B, depth:int, ply:int)
  -> Evalt
{
  e.evaluate(b,depth,ply)
}

#[deriving(Show,Eq,Ord)]
struct IBoard {
  v : u32
}

impl Evaluator<IBoard> for i32 {
  fn evaluate(&self, b:&mut IBoard, depth:int, ply:int) -> Evalt {
    Evalt(*self + (b.v as i32) + 0*((depth+ply) as i32))
  }
}

impl Board for IBoard {
  fn gen_moves(&self) -> ~[Movet] { box [Movet(self.v), Movet(self.v+1)] }
  fn make_move(&mut self, m : Movet) -> MoveResult { 
    let Movet(mv) = m;
    self.v += mv as u32;
    if mv!=0 { SameSideMovesAgain } else { InvalidMove }
  }
  fn unmake_move(&mut self, m : Movet) -> MoveResult {
    let Movet(mv) = m;
    self.v -= mv as u32;
    if mv!=0 { SameSideMovesAgain } else { InvalidMove }
  }
}

fn main() {
  let d = 100;
  let e = 13i32;
  let mut b = IBoard{v:1};
  let (v,pv) = search_negamax(&e, &mut b, d);
  println!("{} {}", v, pv);
}
