
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
        Board { cells: [None; 9], to_move: Some(Side.X), moves_played: 0 }
    }

    // an inefficient but straightforward implementation
    // we return None to indicate the game is still being played,
    // otherwise return Some(Some(side)) where side won
    // and Some(None) for a drawn game
    pub fn is_game_over(&self) -> Option<Option<Side>> {
        // check horizontal lines
        if self.cells[0]==self.cells[1] && self.cells[1]==self.cells[2] && self.cells[0].is_some() {
            Some(Some(self.cells[0]))
        } else if self.cells[3]==self.cells[4] && self.cells[4]==self.cells[5] && self.cells[3].is_some() {
            Some(Some(self.cells[3]))
        } else if self.cells[6]==self.cells[7] && self.cells[7]==self.cells[8] && self.cells[6].is_some() {
            Some(Some(self.cells[6]))
        // check vertical lines
        } else if self.cells[0]==self.cells[3] && self.cells[3]==self.cells[6] && self.cells[0].is_some() {
            Some(Some(self.cells[0]))
        } else if self.cells[1]==self.cells[4] && self.cells[4]==self.cells[7] && self.cells[1].is_some() {
            Some(Some(self.cells[3]))
        } else if self.cells[2]==self.cells[5] && self.cells[5]==self.cells[8] && self.cells[2].is_some() {
            Some(Some(self.cells[6]))
        // check diagonal lines
        } else if self.cells[0]==self.cells[3] && self.cells[3]==self.cells[6] && self.cells[0].is_some() {
            Some(Some(self.cells[0]))
        } else if self.cells[1]==self.cells[4] && self.cells[4]==self.cells[7] && self.cells[1].is_some() {
            Some(Some(self.cells[3]))
        // check for draw
        } else if self.moves_played==9 {
            Some(None)
        } else {
            None
        }
    }

    // a highly inefficient but straightforward implementation:
    // iterate through integers 0..8 and keep those with empty cell
    pub fn moves(&self) -> Vector<Move> {
        (0..9).filter(|&i| self.cells[i].is_none()).collect()
    }

    pub fn make_move(&mut self, m: Move) -> Result<Option<Side>,()> {
        // check for ended game
        if self.cells[m]!=None || self.to_move==None {
            Err(())
        } else {
            self.moves_played += 1;
            self.cells[m] = self.to_move;
            self.to_move = if self.to_move==Side::O {Side::X} else {Side::O};
            if self.is_game_over().is_some() { self.to_move = None; }
            Ok(self.to_move)
        }
    }
    pub fn unmake_move(&mut self, m: Move) -> Result<Side,()> {
    }
}

pub fn fullsearch_v1(b: &mut Board) -> (Option<Side>,usize) {
    // check if game is just over
    let res = b.is_game_over();
    match res {
        Some(s) => { return (s,1); }
        None => { }
    }
    if b.to_move==Some(Side::X) {
        let mut nodes = 1;
        let ml = b.moves();
        // initialize to loss
        let mut best_so_far = Some(Side::O);
        for m in ml {
            b.make_move(m);
            let (result,n) = fullsearch_v1(b);
            b.unmake_move(m);
            nodes += n;
            best_so_far =
                match (result, best_so_far) {
                    (_,Some(Side::X)) | (Some(Side::X),_) => Some(Side::X),
                    (_,None) | (None,_) => None,
                    _ => Some(Side::O),
                };
        }
        return (best_so_far, nodes);
    } else if b.to_move==Some(Side::O) {
        let ml = b.moves();
        let mut best_so_far = Some(Side::X);
        for m in ml {
            b.make_move(m);
            let (result,n) = fullsearch_v1(b);
            b.unmake_move(m);
            nodes += n;
            best_so_far =
                match (result, best_so_far) {
                    (_,Some(Side::O)) | (Some(Side::O),_) => Some(Side::O),
                    (_,None) | (None,_) => None,
                    _ => Some(Side::X),
                };
        }
        return (best_so_far, nodes);
    } else {
        panic!("Unexpected: nobody to move in position {:?}", b);
    }
}


pub fn fullsearch_v2(b: &mut Board) -> (Option<Side>,usize) {
    // check if game is just over
    let res = b.is_game_over();
    match res {
        Some(s) => { return (s,1); }
        None => { }
    }
    let tomove = b.to_move;
    let other = tomove.unwrap().other();
    let mut nodes = 1;
    let ml = b.moves();
    // initialize to loss
    let mut best_so_far = Some(other);
    for m in ml {
        b.make_move(m);
        let (result,n) = fullsearch_v2(b);
        b.unmake_move(m);
        nodes += n;
        best_so_far =
            match (result, best_so_far) {
                (_,Some(tomove)) | (Some(tomove),_) => Some(tomove),
                (_,None) | (None,_) => None,
                _ => Some(other),
            };
    }
    return (best_so_far, nodes);
}

pub fn fullsearch_v3(b: &mut Board) -> (Option<Side>,usize) {
    // check if game is just over
    let res = b.is_game_over();
    match res {
        Some(s) => { return (s,1); }
        None => { }
    }
    let tomove = b.to_move;
    let other = tomove.unwrap().other();
    let mut nodes = 1;
    let ml = b.moves();
    // initialize to loss
    let mut best_so_far = Some(other);
    for m in ml {
        b.make_move(m);
        let (result,n) = fullsearch_v3(b);
        b.unmake_move(m);
        nodes += n;
        best_so_far =
            match (result, best_so_far) {
                (_,Some(tomove)) | (Some(tomove),_) => { return (Some(tomove),nodes) },
                (_,None) | (None,_) => None,
                _ => Some(other),
            };
    }
    return (best_so_far, nodes);
}

pub fn main() {
    let mut board = Board::new();
}












