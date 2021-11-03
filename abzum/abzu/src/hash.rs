pub type H = u64;

pub struct HashGen(u64);

impl HashGen {
  #[inline]
  pub fn raw(x:u64) -> Self { HashGen(x) }
  #[inline]
  pub fn new(x:u64) -> Self {
    let mut x = if x==0 {13} else {x};
    for _ in 0..8 { HashGen::mix(&mut x); }
    HashGen(x)
  }
  #[inline]
  fn mix(x:&mut u64) {
    *x ^=  *x << 13;
    *x ^=  *x >> 7;
    *x ^=  *x << 17;
  }
  #[inline]
  pub fn next(&mut self) -> H {
    HashGen::mix(&mut self.0);
    self.0
  }
}

