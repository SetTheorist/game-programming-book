use std::ops::{Add,Sub,Neg};

pub trait Value
  : Clone + Copy + Sized + Default
  + std::fmt::Debug + std::fmt::Display
  + PartialEq + PartialOrd
  + std::ops::Add<Output=Self>
  + std::ops::Sub<Output=Self>
  + std::ops::Neg<Output=Self>
  + Into<f32>
{
  const MIN : Self;
  const DRAW : Self;
  const MAX : Self;
  fn mate_in_n(n:usize) -> Self;
}

////////////////////////////////////////

macro_rules! op0 {
  ($wr:ident, $tr:ident, $fn:ident) => {
    impl $tr for $wr {
      type Output = Self;
      #[inline]
      fn $fn(self) -> Self {
        $wr(self.0.$fn())
      }
    }
  }
}

macro_rules! op1 {
  ($wr:ident, $tr:ident, $fn:ident) => {
    impl $tr<$wr> for $wr {
      type Output = Self;
      #[inline]
      fn $fn(self, rhs:Self) -> Self {
        $wr(self.0.$fn(rhs.0))
      }
    }
  }
}

////////////////////////////////////////

#[derive(Clone,Copy,Debug,Default,PartialEq,PartialOrd)]
pub struct FValue(pub f32);

op0!(FValue, Neg, neg);
op1!(FValue, Add, add);
op1!(FValue, Sub, sub);

impl std::fmt::Display for FValue {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    write!(f, "{}", self.0)
  }
}

impl Into<f32> for FValue {
  #[inline] fn into(self) -> f32 { self.0 }
}

impl super::Value for FValue {
  const MIN : Self = FValue(-1.0e6);
  const DRAW : Self = FValue(0.0);
  const MAX : Self = FValue(1.0e6);
  fn mate_in_n(n:usize) -> Self {
    Self::MAX - FValue(n as f32)
  }
}

////////////////////////////////////////

#[derive(Clone,Copy,Debug,Default,PartialEq,PartialOrd)]
pub struct IValue(pub i32);

op0!(IValue, Neg, neg);
op1!(IValue, Add, add);
op1!(IValue, Sub, sub);

impl std::fmt::Display for IValue {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    write!(f, "{}", self.0)
  }
}

impl Into<f32> for IValue {
  #[inline] fn into(self) -> f32 { self.0 as f32 }
}

impl super::Value for IValue {
  const MIN : Self = IValue(-1_000_000);
  const DRAW : Self = IValue(0);
  const MAX : Self = IValue(1_000_000);
  fn mate_in_n(n:usize) -> Self {
    Self::MAX - IValue(n as i32)
  }
}

////////////////////////////////////////
