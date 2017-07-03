


/* 5x5 mini-shogi with drops and promotion
 *
 * uses single-array board with 'mailbox'
 */


static debug_dump : bool = false;
static check_incremental_hash : bool = false;

type Hash = u64;
type Weight = f64;

#[derive(Clone,Copy,Debug,Eq,PartialEq,Ord,PartialOrd)]
#[repr(u32)]
pub enum Color {
    Black=1,
    White,
}
impl Color {
    #[inline]
    pub fn name(self) -> char {
        match self {
            Color::Black => 'b',
            Color::White => 'w',
        }
    }
    #[inline]
    pub fn other(self) -> Color {
        match self {
            Color::Black => Color::White,
            Color::White => Color::Black,
        }
    }
}
#[derive(Clone,Copy,Debug,Eq,PartialEq,Ord,PartialOrd)]
#[repr(u32)]
pub enum Piece {
    Pawn=1, Gold=2, Silver=3, Bishop=4, Rook=5, King=6,
    Tokin=7, Nari=8, Horse=9, Dragon=10,
}
impl From<u32> for Piece {
    #[inline]
    fn from(t:u32) -> Piece {
        assert!(1<=t && t<=10);
        unsafe { std::mem::transmute(t) }
    }
}
impl Piece {
    #[inline]
    pub fn name(self) -> char {
        match self {
            Piece::Pawn    => 'P',
            Piece::Gold    => 'G',
            Piece::Silver  => 'S',
            Piece::Bishop  => 'B',
            Piece::Rook    => 'R',
            Piece::King    => 'K',
            Piece::Tokin   => 'T',
            Piece::Nari    => 'N',
            Piece::Horse   => 'H',
            Piece::Dragon  => 'D',
        }
    }
    #[inline]
    pub fn promotes(self) -> Option<Piece> {
        match self {
            Piece::Pawn    => Some(Piece::Tokin),
            Piece::Gold    => None,
            Piece::Silver  => Some(Piece::Nari),
            Piece::Bishop  => Some(Piece::Horse),
            Piece::Rook    => Some(Piece::Dragon),
            Piece::King    => None,
            Piece::Tokin   => None,
            Piece::Nari    => None,
            Piece::Horse   => None,
            Piece::Dragon  => None,
        }
    }
    #[inline]
    pub fn promoted(self) -> Option<Piece> {
        match self {
            Piece::Pawn    => None,
            Piece::Gold    => None,
            Piece::Silver  => None,
            Piece::Bishop  => None,
            Piece::Rook    => None,
            Piece::King    => None,
            Piece::Tokin   => Some(Piece::Pawn),
            Piece::Nari    => Some(Piece::Silver),
            Piece::Horse   => Some(Piece::Bishop),
            Piece::Dragon  => Some(Piece::Rook),
        }
    }
    #[inline]
    pub fn captured(self) -> Piece {
        match self {
            Piece::Pawn    => Piece::Pawn,
            Piece::Gold    => Piece::Gold,
            Piece::Silver  => Piece::Silver,
            Piece::Bishop  => Piece::Bishop,
            Piece::Rook    => Piece::Rook,
            Piece::King    => Piece::King,
            Piece::Tokin   => Piece::Pawn,
            Piece::Nari    => Piece::Silver,
            Piece::Horse   => Piece::Bishop,
            Piece::Dragon  => Piece::Rook,
        }
    }
}

//  3         2         1         0
// 10987654321098765432109876543210
// ||||||        [  ][  ][   ][   ]
// sncqxd        k   p   t    f    
//
// f : 5= from
// t : 5= to
// p : 4= piece
// k : 4= captured piece
// d : 1= drop flag
// x : 1= refused promotion flag
// q : 1= promotion flag
// c : 1= capture flag
// n : 1= non-null move flag
// s : 1= side is black
//
// 0/0x10000000 = nullmove
//
#[derive(Clone,Copy,Debug,Eq,PartialEq,Ord,PartialOrd)]
pub struct Move(u32);
impl Move {
    #[inline]
    fn build_move(
        f: u32,
        t: u32,
        p: u32,
        k: u32,
        d: bool,
        x: bool,
        q: bool,
        c: bool,
        n: bool,
        s: bool,
    ) -> Move {
        Move( ((f & 0b011111) << 0)
            | ((t & 0b011111) << 5)
            | ((p & 0b01111) << 10)
            | ((k & 0b01111) << 14)
            | (if d {1<<26} else {0})
            | (if x {1<<27} else {0})
            | (if q {1<<28} else {0})
            | (if c {1<<29} else {0})
            | (if n {1<<30} else {0})
            | (if s {1<<31} else {0})
            )
    }

    #[inline]
    pub fn f(self) -> u32 { ((self.0>>0) & 0b011111) }
    #[inline]
    pub fn t(self) -> u32 { ((self.0>>5) & 0b011111) }
    #[inline]
    pub fn p(self) -> Option<Piece> {
        let t = ((self.0>>10)&0b01111);
        if 1<=t && t<=10 { Some(Piece::from(t)) }
        else { None }
    }
    #[inline]
    pub fn k(self) -> Option<Piece> {
        let t = ((self.0>>14)&0b01111);
        if 1<=t && t<=10 { Some(Piece::from(t)) }
        else { None }
    }
    #[inline]
    pub fn d(self) -> bool { ((self.0 & (1<<26)) != 0) }
    #[inline]
    pub fn x(self) -> bool { ((self.0 & (1<<27)) != 0) }
    #[inline]
    pub fn q(self) -> bool { ((self.0 & (1<<28)) != 0) }
    #[inline]
    pub fn c(self) -> bool { ((self.0 & (1<<29)) != 0) }
    #[inline]
    pub fn n(self) -> bool { ((self.0 & (1<<30)) != 0) }
    #[inline]
    pub fn s(self) -> Color { if ((self.0 & (1<<31)) != 0) {Color::Black} else {Color::White} }

    #[inline]
    pub fn new_null(s:Color) -> Move {
        Move::build_move(0, 0, 0, 0, false, false, false, false, true, (s==Color::Black))
    }
    #[inline]
    pub fn new_move(f: u32, t: u32, s:Color, p: Piece, promotion: bool, refused: bool) -> Move {
        Move::build_move(f, t, (p as u32), 0, false, refused, promotion, false,  true, (s==Color::Black))
    }
    #[inline]
    pub fn new_drop(t: u32, s:Color, p: Piece) -> Move {
        Move::build_move(26, t, (p as u32), 0, false, false, false, false,  true, (s==Color::Black))
    }
    #[inline]
    pub fn new_cap(f: u32, t: u32, s:Color, p: Piece, k: Piece, promotion: bool, refused: bool) -> Move {
        Move::build_move(f, t, (p as u32), (k as u32), false, refused, promotion, true,  true, (s==Color::Black))
    }
}

#[derive(Clone,Copy,Debug,Eq,PartialEq,Ord,PartialOrd)]
pub enum MoveResult {
    SameSide, OtherSide, Error,
}

#[derive(Clone,Debug,Eq,PartialEq)]
pub struct Board {
    cells : [Option<(Color,Piece)>; 25],
    to_move : Color,
    moves_played : usize,
    move_stack : Vec<Move>,
    hand : [[usize; 6]; 3],
}
impl Board {
    pub fn new() -> Board {
        Board {
            cells : Board::init_cells(),
            to_move : Color::Black,
            moves_played : 0,
            move_stack : Vec::new(),
            hand : [[0; 6]; 3],
        }
    }
    pub fn reset(&mut self) {
        *self = Board::new();
    }
    fn init_cells() -> [Option<(Color,Piece)>; 25] {
        [
            Some((Color::Black,Piece::King)),
            Some((Color::Black,Piece::Gold)),
            Some((Color::Black,Piece::Bishop)),
            Some((Color::Black,Piece::Silver)),
            Some((Color::Black,Piece::Rook)),

            Some((Color::Black,Piece::Pawn)),
            None, None, None, None,

            None, None, None, None, None,

            None, None, None, None,
            Some((Color::White,Piece::Pawn)),

            Some((Color::White,Piece::Rook)),
            Some((Color::White,Piece::Silver)),
            Some((Color::White,Piece::Bishop)),
            Some((Color::White,Piece::Gold)),
            Some((Color::White,Piece::King)),
        ]
    }

    pub fn make_move(&mut self, m: Move) -> MoveResult {
        self.moves_played += 1;
        self.to_move = self.to_move.other();
        self.move_stack.push(m);
        return MoveResult::OtherSide;
    }

    pub fn unmake_move(&mut self) -> MoveResult {
        if self.moves_played==0 {
            return MoveResult::Error;
        }
        let m = self.move_stack.pop();
        self.moves_played -= 1;
        self.to_move = self.to_move.other();
        return MoveResult::OtherSide;
    }
}


pub fn main() {
    let mut b = Board::new();
    println!("b = {:?}", b);

    let m = Move::new_null(Color::Black);
    println!("m = {:?}", m);
    let m = Move::new_move(3, 4, Color::Black, Piece::Gold, false, true);
    println!("m = {:?}", m);
    let m = Move::new_drop(4, Color::White, Piece::Horse);
    println!("m = {:?}", m);
    let m = Move::new_cap(7, 8, Color::White, Piece::Silver, Piece::Rook, true, true);
    println!("m = {:?}", m);
}


