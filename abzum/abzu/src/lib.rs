#![allow(mixed_script_confusables)]

pub mod hash;
pub mod pv;
pub mod search;
pub mod td_lambda;
pub mod time;
pub mod tt;
pub mod value;

use crate::value::{Value};

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

pub trait Board<M:Move> : Clone {
  fn new() -> Self;
  fn init(&mut self);
  fn terminal(&self) -> bool;

  fn hash(&self) -> hash::H;

  fn gen_moves(&self) -> Vec<M>;
  fn gen_q_moves(&self) -> Vec<M>;

  fn make_move(&mut self, m:M) -> MoveResult;
  fn unmake_move(&mut self, m:M) -> MoveResult;
}

pub trait Evaluator<B:Board<M>,M:Move,V:Value> {
  fn evaluate_absolute(&self, b:&B, ply:usize) -> V;
  fn evaluate_relative(&self, b:&B, ply:usize) -> V;

  fn stalemate_absolute(&self, b:&B, ply:usize) -> V;
  fn stalemate_relative(&self, b:&B, ply:usize) -> V;

  fn num_weights(&self) -> usize;
  fn get_weight_f32(&self, j:usize) -> f32;
  fn set_weight_f32(&mut self, j:usize, wj:f32);
  fn get_all_weights_f32(&self) -> Vec<f32>;
}

pub trait Game {
  type M : Move;
  type B : Board<Self::M>;
  type V : Value;
  type E : Evaluator<Self::B,Self::M,Self::V>;
}


#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        let result = 2 + 2;
        assert_eq!(result, 4);
    }
}
