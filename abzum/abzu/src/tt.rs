use crate::hash::{H};

pub const F_INVALID : u16 = 0;
pub const F_VALID   : u16 = 1;
pub const F_UPPER   : u16 = 2|F_VALID;
pub const F_LOWER   : u16 = 4|F_VALID;
pub const F_EXACT   : u16 = F_UPPER|F_LOWER|F_VALID;

#[derive(Clone,Copy,Default,Debug)]
pub struct Entry<V,M> {
  pub lock: H,
  pub value: V,
  pub best_move: M,
  pub depth: i16,
  pub flags: u16,
}

#[derive(Clone,Copy,Default,Debug)]
pub struct Stats {
  pub hit: usize,
  pub miss: usize,
  pub deep: usize,
  pub shallow: usize,
  pub used: usize,
  pub used_exact: usize,
  pub used_lower: usize,
  pub used_upper: usize,
}

impl Stats {
  pub fn new() -> Self {
    Stats {
      hit: 0,
      miss: 0,
      deep: 0,
      shallow: 0,
      used: 0,
      used_exact: 0,
      used_lower: 0,
      used_upper: 0,
    }
  }
}

pub struct Table<V,M>{
  n:usize,
  t:Vec<Entry<V,M>>,
  pub s:Stats,
}

impl<V,M> Table<V,M> where
  V:Clone+Copy+Default,
  M:Clone+Copy+Default,
{
  pub fn new(n:usize) -> Self {
    let t = vec![Entry::default(); n];
    let s = Stats::default();
    Table { n, t, s }
  }

  pub fn init(&mut self) {
    self.t.iter_mut().for_each(|x|{*x=Entry::default();});
    self.s = Stats::default();
  }

  pub fn retrieve(&mut self, h:H) -> Option<Entry<V,M>> {
    let i = (h % (self.n as u64)) as usize;
    let e = self.t[i];
    if e.flags == F_INVALID { return None; }
    self.s.hit += 1;
    if e.lock != h { self.s.miss += 1; return None; }
    return Some(e);
  }

  pub fn store(&mut self, hash:H, val:V, m:M, flags:u16, depth:i16) {
    let i = (hash % (self.n as u64)) as usize;
    let e = &mut self.t[i];
    e.lock = hash;
    e.value = val;
    e.best_move = m;
    e.depth = depth;
    e.flags = flags;
  }
}



