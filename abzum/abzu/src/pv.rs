////////////////////////////////////////////////////////////////////////////////

pub struct PV<M>{
  pub pv:Vec<Vec<M>>,
  pub length:Vec<usize>,
}

impl<M:Clone+Copy+Default> PV<M> {
  pub fn new(maxn:usize) -> Self {
    let pv = vec![vec![M::default();maxn];maxn];
    let length = vec![0; maxn];
    PV { pv, length }
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

