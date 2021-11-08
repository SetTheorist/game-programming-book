use crate::{Board,Game,GameResult,Side};

////////////////////////////////////////

#[derive(Clone,Debug)]
pub struct Ply<G:Game> {
  pub side: Side,
  pub evaluation: G::V,
  pub m: G::M,
  pub pv: Vec<G::M>,
  pub time_spent_cs: u64,
  pub nodes_searched: u64,
  pub qnodes_searched: u64,
  pub nodes_evaluated: u64,
  pub qnodes_evaluated: u64,
  // pub sm: special_move,
  pub annotation: String,
}

impl<G:Game> std::fmt::Display for Ply<G> {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    write!(f, "[{} {}  ({}) [",
      if self.side == Side::First {'F'}
        else if self.side == Side::Second {'S'}
        else {'?'},
      self.m,
      self.evaluation
      )?;
    for &m in self.pv.iter() { write!(f, " {}", m)?; }
    write!(f, " ] {:.2}s {}ns/{}qs/{}ne/{}qe \"{}\"]",
      (self.time_spent_cs as f64)/100.0,
      self.nodes_searched, self.qnodes_searched,
      self.nodes_evaluated, self.qnodes_evaluated,
      self.annotation
      )
  }
}

////////////////////////////////////////

#[derive(Clone,Debug)]
pub struct GameRecord<G:Game> {
  pub initial_board: G::B,
  pub current_board: G::B,
  pub move_list: Vec<Ply<G>>,
  pub first_player: String,
  pub second_player: String,
  pub terminal: bool,
  pub result: Option<GameResult>,
}

impl<G:Game> GameRecord<G> {
  pub fn new(first_player:String, second_player:String) -> Self {
    GameRecord {
      initial_board: G::B::new(),
      current_board: G::B::new(),
      move_list: Vec::new(),
      first_player,
      second_player,
      terminal: false,
      result: None,
    }
  }

  pub fn add_ply(&mut self, ply:Ply<G>) {
    let m = ply.m;
    self.move_list.push(ply);
    self.current_board.make_move(m);
    if self.current_board.terminal() {
      self.terminal = true;
      self.result = Some(self.current_board.game_result());
    }
  }
}

impl<G:Game> std::fmt::Display for GameRecord<G> {
  fn fmt(&self, f:&mut std::fmt::Formatter) -> std::fmt::Result {
    let mut b = self.initial_board.clone();
    writeln!(f, "#####========================================#####")?;
    for ply in self.move_list.iter() {
      writeln!(f, "{}", b)?;
      b.make_move(ply.m);
      writeln!(f, "{}", ply)?;
    }
    writeln!(f, "{}", self.current_board)?;
    writeln!(f, "#####========================================#####")
  }
}
