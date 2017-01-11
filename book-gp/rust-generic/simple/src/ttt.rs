use search;

use std::ops::{Neg,Not};

#[derive(Clone,Copy,Debug,Default,Eq,PartialEq)]
pub struct Move(u8);
#[derive(Clone,Copy,Debug,Default,Eq,Ord,PartialEq,PartialOrd)]
pub struct Eval(pub i32);
impl Neg for Eval {
    type Output = Eval;
    fn neg(self) -> Eval { Eval(-self.0) }
}
#[derive(Clone,Copy,Debug,Eq,Ord,PartialEq,PartialOrd)]
pub enum Side { X=1, O }
impl Not for Side {
    type Output = Side;
    fn not(self) -> Side {
        match self { Side::X=>Side::O, Side::O=>Side::X }
    }
}
#[derive(Clone,Copy,Debug)]
pub struct Board {
    game_state: search::GameState,
    to_play: Side,
    board: [Option<Side>; 9],
    n: usize,
}
impl Board {
    pub fn show(&self) {
        for i in 0..9 {
            print!("{}",
                match self.board[i] {
                    None => '.',
                    Some(Side::X) => 'X',
                    Some(Side::O) => 'O',
                }
            );
            if (i+1)%3==0 { println!(""); }
        }
    }
}
impl search::Board for Board {
    type Move = Move;
    type Eval = Eval;
    fn new() -> Self {
        Board {
            game_state:search::GameState::Playing,
            to_play:Side::X,
            board:[None;9],
            n:0,
        }
    }
    fn from_fen(_s: &str) -> Self {
        Self::new()
    }
    fn to_fen(&self) -> String {
        let mut s = String::new();
        s.push(match self.to_play {
                    Side::X => 'X',
                    Side::O => 'O',
                }
            );
        s.push('|');
        for &t in self.board.iter() {
            s.push(
                match t {
                    None => '.',
                    Some(Side::X) => 'X',
                    Some(Side::O) => 'O',
                }
            );
        }
        s
    }
    fn init(&mut self) {
        *self = Self::new();
    }
    fn make_move(&mut self, m: Self::Move) -> search::MoveResult {
        self.board[m.0 as usize] = Some(self.to_play);
        self.to_play = !self.to_play;
        self.n += 1;
        self.game_state = self.state();
        search::MoveResult::OtherSide
    }
    fn unmake_move(&mut self, m: Self::Move) -> search::MoveResult {
        self.board[m.0 as usize] = None;
        self.to_play = !self.to_play;
        self.n -= 1;
        self.game_state = search::GameState::Playing;
        search::MoveResult::OtherSide
    }
    fn hash(&self) -> search::Hash {
        let mut h = 0;
        for &t in self.board.iter() {
            match t {
                None => { h = h*13; }
                Some(Side::X) => { h = h*13+3; }
                Some(Side::O) => { h = h*13+7; }
            }
        }
        h
    }
    fn moves(&self, ml: &mut [Self::Move]) -> usize {
        let mut n = 0;
        for i in 0..9 {
            if self.board[i].is_none() {
                ml[n] = Move(i as u8);
                n += 1;
            }
        }
        n
    }
    fn evaluate(&self, ply:usize) -> Self::Eval {
        let v = match self.game_state {
            search::GameState::WhiteWin =>  Eval(1000000 - (ply as i32)),
            search::GameState::BlackWin => -Eval(1000000 - (ply as i32)),
            _ => Eval(0),
        };
        if self.to_play==Side::X {v} else {-v}
    }
    fn state(&self) -> search::GameState {
        for &(a,b,c) in &[(0,1,2),(3,4,5),(6,7,8),(0,3,6),(1,4,7),(2,5,8),(0,4,8),(2,4,6)] {
            if self.board[a]==self.board[b] && self.board[b]==self.board[c] {
                match self.board[a] {
                    Some(Side::X) => { return search::GameState::WhiteWin }
                    Some(Side::O) => { return search::GameState::BlackWin }
                    None => {}
                }
            }
        }
        if self.board.iter().all(|&x|x.is_some()) {
            return search::GameState::Draw;
        } else {
            return search::GameState::Playing;
        }
    }
}
