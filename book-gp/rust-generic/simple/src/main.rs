mod search;
mod microshogi;
mod ttt;

use search::{Board};

extern crate rand;
use rand::Rng;

fn main() {
    let mut ab = search::AlphaBetaSearcher::<microshogi::Board>::new();
    println!("{:?}", ab.b.to_fen());
    let mut ml = [microshogi::Move::default(); 99];
    let n = ab.b.moves(&mut ml);
    println!("{} : {:?}", n, &ml[0..n]);
    //println!("{:?}", ab.b);
    return;


    let mut ab = search::AlphaBetaSearcher::<ttt::Board>::new();
    println!("{:?}", ab.b);
    while ab.b.state() == search::GameState::Playing {
        println!("====================");
        ab.b.show();
        println!("FEN: {:?}", ab.b.to_fen());
        println!("Hash: {:?}", ab.b.hash());
        let mut ml = [ttt::Move::default(); 10];
        let n = ab.b.moves(&mut ml);
        println!("{:?} {:?}", n, &ml[0..n]);

        let v = ab.alpha_beta(0, 900, ttt::Eval(-1000001), ttt::Eval(1000001), 0);
        println!("AB: {:?} {:?}", v, &ab.pv[0][0..(ab.pv_length[0])]);
        let m = ml[(19+n/2)%n];
        println!("Move ==> {:?}", m);
        ab.b.make_move(m);
    }
    println!("--------------------");
    ab.b.show();
    println!("{:?}", ab.b);
    println!("{}", ab.nodes_searched);
    println!("{} {} {} {} {}",
        ab.ttable.tt_hits, ab.ttable.tt_false,
        ab.ttable.tt_deep, ab.ttable.tt_shallow,
        ab.ttable.tt_used);


    println!("{}", std::mem::size_of::<search::TTEntry<ttt::Move,ttt::Eval>>());
    println!("{}", std::mem::size_of::<search::TTable<ttt::Board>>());
    println!("{}", std::mem::size_of::<Option<(microshogi::Piece,microshogi::Color)>>());
    let mut rng = rand::thread_rng();
    println!("{}", rng.gen::<search::Hash>());
}

