use std::ops::{Add,Sub,Neg};

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

impl super::Value for IValue {
  const MIN : Self = IValue(-1_000_000);
  const DRAW : Self = IValue(0);
  const MAX : Self = IValue(1_000_000);
  fn mate_in_n(n:usize) -> Self {
    Self::MAX - IValue(n as i32)
  }
}

////////////////////////////////////////
