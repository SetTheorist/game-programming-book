
// two sides playing
#[derive(Clone,Copy,Debug,Eq,Ord,PartialEq,PartialOrd)]
pub enum Side {
    X=1,
    O=2
}

impl Side {
    pub fn other(self) -> Self {
        match self {
            Side::O => Side::X,
            Side::X => Side::O,
        }
    }
}

#[derive(Clone,Copy,Debug,Eq,Ord,PartialEq,PartialOrd)]
pub enum MoveResult {
    SameSideMoves=1,
    OtherSideMoves=2,
    GameOver=3,
    Error=4,
}

#[derive(Clone,Copy,Debug,Eq,Ord,PartialEq,PartialOrd)]
pub enum GameResult {
    Win(Side),
    Draw,
    Error,
}

// possibly empty board cell
//#[derive(Clone,Copy,Debug,Eq,Ord,PartialEq,PartialOrd)]
type Cell = Option<Side>;

//#[derive(Clone,Copy,Debug,Eq,Ord,PartialEq,PartialOrd)]
type Move = usize;

// board representation
#[derive(Clone,Copy,Debug,Eq,Ord,PartialEq,PartialOrd)]
pub struct Board {
    cells : [Cell; 9],
    to_move : Option<Side>,
    moves_played : usize,
}

impl Board {
    // initialization
    pub fn new() -> Self {
        Board { cells: [None; 9], to_move: Some(Side::X), moves_played: 0 }
    }

    // an inefficient but straightforward implementation
    // we return None to indicate the game is still being played,
    // otherwise return Some(Some(side)) where side won
    // and Some(None) for a drawn game
    pub fn is_game_over(&self) -> Option<GameResult> {
        for &(s,d) in [
            (0,1),(3,1),(6,1), // horizontal
            (0,3),(1,3),(2,3), // vertical
            (0,4),(3,2),       // diagonal
            ].iter()
        {
            if self.cells[s]==self.cells[s+d]
                && self.cells[s+d]==self.cells[s+d+d]
                && self.cells[s].is_some()
            {
                return Some(GameResult::Win(if self.cells[s]==Some(Side::X) {Side::X} else {Side::O} ))
            }
            
        }
        if self.moves_played==9 {
            // draw?
            return Some(GameResult::Draw);
        } else {
            // not over
            return None;
        }
    }

    // a highly inefficient but straightforward implementation:
    // iterate through integers 0..8 and keep those with empty cell
    pub fn moves(&self) -> Vec<Move> {
        (0..9).filter(|&i| self.cells[i].is_none()).collect()
    }

    pub fn make_move(&mut self, m: Move) -> MoveResult {
        // check for ended game
        if self.cells[m]!=None || self.to_move==None {
            MoveResult::Error
        } else {
            self.moves_played += 1;
            self.cells[m] = self.to_move;
            self.to_move = Some(if self.to_move.unwrap()==Side::O {Side::X} else {Side::O});
            if self.is_game_over().is_some() { self.to_move = None; }
            MoveResult::OtherSideMoves
        }
    }
    pub fn unmake_move(&mut self, m: Move) -> MoveResult {
        MoveResult::Error
    }
}

pub fn fullsearch_v1(b: &mut Board) -> (GameResult,usize) {
    let mut nodes = 1;
    // check if game is just over
    let res = b.is_game_over();
    match res {
        Some(s) => { return (s,nodes); }
        None => { }
    }
    if b.to_move==Some(Side::X) {
        let ml = b.moves();
        // initialize to loss
        let mut best_so_far = GameResult::Win(Side::O);
        for m in ml {
            b.make_move(m);
            let (result,n) = fullsearch_v1(b);
            b.unmake_move(m);
            nodes += n;
            best_so_far =
                match (result, best_so_far) {
                    (_,GameResult::Win(Side::X)) | (GameResult::Win(Side::X),_) => GameResult::Win(Side::X),
                    (_,GameResult::Draw) | (GameResult::Draw,_) => GameResult::Draw,
                    _ => GameResult::Win(Side::O),
                };
        }
        return (best_so_far, nodes);
    } else if b.to_move==Some(Side::O) {
        let ml = b.moves();
        let mut best_so_far = GameResult::Win(Side::X);
        for m in ml {
            b.make_move(m);
            let (result,n) = fullsearch_v1(b);
            b.unmake_move(m);
            nodes += n;
            best_so_far =
                match (result, best_so_far) {
                    (_,GameResult::Win(Side::O)) | (GameResult::Win(Side::O),_) => GameResult::Win(Side::O),
                    (_,GameResult::Draw) | (GameResult::Draw,_) => GameResult::Draw,
                    _ => GameResult::Win(Side::X),
                };
        }
        return (best_so_far, nodes);
    } else {
        panic!("Unexpected: nobody to move in position {:?}", b);
    }
}


pub fn fullsearch_v2(b: &mut Board) -> (GameResult,usize) {
    // check if game is just over
    let res = b.is_game_over();
    match res {
        Some(s) => { return (s,1); }
        None => { }
    }
    let tomove = b.to_move.unwrap();
    let other = tomove.other();
    let mut nodes = 1;
    let ml = b.moves();
    // initialize to loss
    let mut best_so_far = GameResult::Win(other);
    for m in ml {
        b.make_move(m);
        let (result,n) = fullsearch_v2(b);
        b.unmake_move(m);
        nodes += n;
        best_so_far =
            match (result, best_so_far) {
                (_,GameResult::Win(x)) | (GameResult::Win(x),_) if x==tomove => { return (GameResult::Win(tomove),nodes) },
                (_,GameResult::Draw) | (GameResult::Draw,_) => GameResult::Draw,
                _ => GameResult::Win(other),
            };
    }
    return (best_so_far, nodes);
}

pub fn fullsearch_v3(b: &mut Board) -> (GameResult,usize) {
    // check if game is just over
    let res = b.is_game_over();
    match res {
        Some(s) => { return (s,1); }
        None => { }
    }
    let tomove = b.to_move.unwrap();
    let other = tomove.other();
    let mut nodes = 1;
    let ml = b.moves();
    // initialize to loss
    let mut best_so_far = GameResult::Win(other);
    for m in ml {
        b.make_move(m);
        let (result,n) = fullsearch_v3(b);
        b.unmake_move(m);
        nodes += n;
        best_so_far =
            match (result, best_so_far) {
                (_,GameResult::Win(x)) | (GameResult::Win(x),_) if x==tomove => { return (GameResult::Win(tomove),nodes) },
                (_,GameResult::Draw) | (GameResult::Draw,_) => GameResult::Draw,
                _ => GameResult::Win(other),
            };
    }
    return (best_so_far, nodes);
}

pub fn main() {
    let mut board = Board::new();
    println!("board = {:?}", board);
}


