use crate::tt;

pub mod negamax;

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
pub struct Settings<E:Clone+Copy+Sized> {
  pub depth: i16,
  pub hh: Option<HH>,
  pub tt: Option<TT>,
  pub id: Option<ID>,
  pub iid: Option<IID>,
  pub mws: Option<MWS>,
  pub asp: Option<ASP<E>>,
  pub qs: Option<QS>,
  pub nmp: Option<NMP>,
}

////////////////////////////////////////////////////////////////////////////////

#[derive(Clone,Copy,Debug,Default)]
pub struct Stats {
  pub nodes_searched: u64,
  pub tt: tt::Stats,
}

////////////////////////////////////////////////////////////////////////////////

