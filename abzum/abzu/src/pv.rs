////////////////////////////////////////////////////////////////////////////////

pub struct PV<M>{
  pub length:Vec<usize>,
  pub pv:Vec<Vec<M>>,
}

impl<M:Clone+Copy+Default> PV<M> {
  pub fn new(maxn:usize) -> Self {
    let length = vec![0; maxn];
    let pv = vec![vec![M::default();maxn];maxn];
    PV { length, pv }
  }
  
  pub fn init(&mut self) {
    for l in self.length.iter_mut() { *l = 0; }
  }

  pub fn update(&mut self, ply:usize, m:M) {
    let n = self.length[ply+1];
    //let (l,r) = self.pv.split_at_mut(ply+1);
    //l[ply][(ply+1)..(ply+1+n)].copy_from_slice(&r[0][(ply+1)..(ply+1+n)]);
    let (l,r) = self.pv[ply..=(ply+1)].split_at_mut(1);
    l[0][(ply+1)..(ply+1+n)].copy_from_slice(&r[0][(ply+1)..(ply+1+n)]);
    self.pv[ply][ply] = m;
    self.length[ply] = self.length[ply+1]+1;
  }
}

