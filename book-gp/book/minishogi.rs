


/* 5x5 mini-shogi with drops and promotion
 *
 * uses single-array board with 'mailbox'
 */


static let debug_dump : boolean = false;
static let check_incremental_hash : boolean = false;

type Hash = u64;
type Weight = f64;

pub enum Color {
    black=1,
    white,
}

pub enum Piece {
    pawn=1, gold, silver, bishop, rook, king,
    tokin, promoted_silver, horse, dragon,
}
impl Piece {
    pub fn name(&self) -> char {
        match self {
            pawn => 'P',
            gold => 'G',
            silver => 'S',
            bishop => 'B',
            rook => 'R',
            king => 'K',
            tokin => 'T',
            promoted_silver => 'N',
            horse => 'H',
            dragon => 'D',
        }
    }
}


