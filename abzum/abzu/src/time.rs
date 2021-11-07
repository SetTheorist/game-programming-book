
#[derive(Clone,Copy,Debug,PartialEq)]
pub struct TimeControl {
  pub active: bool,

  /* Initial time-control */
  pub max_depth: i16,        /* 0 = unlimited */
  pub max_nodes: u64,       /* 0 = unlimited */
  pub increment_cs: i32,     /* increment-per-move, in centi-seconds */
  pub byoyomi_cs: i32,       /* time per move (not accumulated), in centi-seconds */
  pub starting_cs: i32,      /* initial time on clock, in centi-seconds */
  pub moves_per: i32,        /* moves per control period */

  /* Current state at start of move */
  pub moves_remaining: i16,  /* 0 = sudden death */
  pub remaining_cs: i32,     /* current time on clock, in centi-seconds */

  /* Working details */
  pub allocated_cs: i32,     /* time allocated for move, in centi-seconds */
  pub panic_cs: i32,         /* extra time allocated for move, in centi-seconds */
  pub force_stop_cs: i32,    /* absolute max time to use */
  pub start_clock: u64,     /* starting clock time (microseconds from epoch) */
}

impl std::fmt::Display for TimeControl {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    write!(f, "<{}", if self.active {'+'} else {'-'})?;
    write!(f, "|{},{},{},{},{},{}",
      self.max_depth, self.max_nodes, self.increment_cs,
      self.byoyomi_cs, self.starting_cs, self.moves_per)?;
    write!(f, "|{},{}", self.moves_remaining, self.remaining_cs)?;
    write!(f, "|{},{},{},{}",
      self.allocated_cs, self.panic_cs, self.force_stop_cs, self.start_clock)?;
    write!(f, ">")
  }
}
