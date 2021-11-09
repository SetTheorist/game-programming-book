pub mod alphabeta;
pub mod negamax;

use crate::value;

////////////////////////////////////////////////////////////////////////////////

#[derive(Clone,Copy,Debug)]
pub struct HH {}
#[derive(Clone,Copy,Debug)]
pub struct TT {}
#[derive(Clone,Copy,Debug)]
pub struct ID {
  pub base:i16,
  pub step:i16,
}
#[derive(Clone,Copy,Debug)]
pub struct IID {
  pub base:i16,
  pub step:i16,
}
#[derive(Clone,Copy,Debug)]
pub struct MWS {}
#[derive(Clone,Copy,Debug)]
pub struct ASP<E:Clone+Copy+Sized> {
  pub width:E,
}
#[derive(Clone,Copy,Debug)]
pub struct QS {
  pub depth:i16,
}
#[derive(Clone,Copy,Debug)]
pub struct NMP {
  pub cutoff:i16,
  pub r1:i16,
  pub r2:i16,
}

// TODO: make these compile-time? (Generic/traits?)
// (could be more efficient with optimization)
#[derive(Clone,Copy,Debug)]
pub struct Settings<V:value::Value> {
  pub depth: i16,
  pub hh: Option<HH>,
  pub tt: Option<TT>,
  pub id: Option<ID>,
  pub iid: Option<IID>,
  pub mws: Option<MWS>,
  pub asp: Option<ASP<V>>,
  pub qs: Option<QS>,
  pub nmp: Option<NMP>,
}

impl<V:value::Value> Settings<V> {
  pub fn new(depth:i16) -> Self {
    Settings {
      depth,
      hh: None,
      tt: None,
      id: None,
      iid: None,
      mws: None,
      asp: None,
      qs: None,
      nmp: None,
    }
  }

  pub fn hh(mut self, hh:Option<HH>) -> Self { self.hh = hh; self }
  pub fn tt(mut self, tt:Option<TT>) -> Self { self.tt = tt; self }
  pub fn id(mut self, id:Option<ID>) -> Self { self.id = id; self }
  pub fn iid(mut self, iid:Option<IID>) -> Self { self.iid = iid; self }
  pub fn mws(mut self, mws:Option<MWS>) -> Self { self.mws = mws; self }
  pub fn asp(mut self, asp:Option<ASP<V>>) -> Self { self.asp = asp; self }
  pub fn qs(mut self, qs:Option<QS>) -> Self { self.qs = qs; self }
  pub fn nmp(mut self, nmp:Option<NMP>) -> Self { self.nmp = nmp; self }
}

////////////////////////////////////////////////////////////////////////////////

#[derive(Clone,Copy,Debug,Default)]
pub struct Stats {
  pub nodes_searched: u64,
  pub qnodes_searched: u64,
}

impl Stats {
  pub fn new() -> Self {
    Stats {
      nodes_searched: 0,
      qnodes_searched: 0,
    }
  }
  pub fn init(&mut self) {
    self.nodes_searched = 0;
    self.qnodes_searched = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////

