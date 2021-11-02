pub mod hash;
pub mod search;
pub mod tt;

pub enum MoveResult {
  InvalidMove,
  OtherSideMoves,
  SameSideMovesAgain
}

pub trait Move : Clone+Copy+Default+Sized+PartialEq {
  fn to_i(self) -> u32;
  fn from_i(i:u32) -> Self;
  fn is_valid(&self) -> bool;
  fn is_null(&self) -> bool;
  fn null_move() -> Self;
}

pub trait Board : Clone {
  type M : Move;

  fn new() -> Self;
  fn init(&mut self);
  fn terminal(&self) -> bool;

  fn hash(&self) -> hash::H;

  fn gen_moves(&self) -> Vec<Self::M>;
  fn gen_q_moves(&self) -> Vec<Self::M>;

  fn make_move(&mut self, m:Self::M) -> MoveResult;
  fn unmake_move(&mut self, m:Self::M) -> MoveResult;
}

pub trait Value
  : Clone + Copy + Sized + Default
  + PartialOrd + PartialEq
  + std::ops::Add<Output=Self>
  + std::ops::Sub<Output=Self>
  + std::ops::Neg<Output=Self>
{
  const MIN : Self;
  const DRAW : Self;
  const MAX : Self;
  fn mate_in_n(n:usize) -> Self;
}

pub trait Evaluator<B:Board,V:Value> {
  fn evaluate_relative(&self, b:&B, ply:usize) -> V;
  fn evaluate_absolute(&self, b:&B, ply:usize) -> V;
}

pub trait Game {
  type M : Move;
  type B : Board<M=Self::M>;
  type V : Value;
  type E : Evaluator<Self::B,Self::V>;
}


#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        let result = 2 + 2;
        assert_eq!(result, 4);
    }
}
