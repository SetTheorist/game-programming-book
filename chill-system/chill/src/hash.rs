pub type H = u64;

pub struct HashGen(u64);

impl HashGen {
  #[inline]
  pub fn new(x:u64) -> Self {
    HashGen(x)
  }
  #[inline]
  pub fn next(&mut self) -> H {
    let mut x = self.0;
    x ^=  x << 13;
    x ^=  x >> 7;
    x ^=  x << 17;
    self.0 = x;
    x
  }
}

