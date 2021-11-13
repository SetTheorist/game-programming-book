

const E  : u8 = 0b0000_0001;
const NE : u8 = 0b0000_0010;
const N  : u8 = 0b0000_0100;
const NW : u8 = 0b0000_1000;
const W  : u8 = 0b0001_0000;
const SW : u8 = 0b0010_0000;
const S  : u8 = 0b0100_0000;
const SE : u8 = 0b1000_0000;
const FOX   : u8 = 0b0000_0001;
const HORSE : u8 = 0b0000_0010;
const EAGLE : u8 = 0b0000_0100;

////////////////////////////////////////////////////////////

macro_rules! process_pieces {
  ($(($p:ident,$x1:expr,$x2:expr,$x3:expr,$x4:expr,$x5:expr,$x6:expr,$x7:expr)),+$(,)*) => {
    #[derive(Clone,Copy,Debug,Eq,PartialEq)]
    pub enum Piece {
      $($p),+
    }

    impl Piece {
      pub fn kanji1(self) -> char {
        use Piece::*;
        match self { $($p => $x1),+ }
      }
      pub fn kanji(self) -> &'static str {
        use Piece::*;
        match self { $($p => $x2),+ }
      }
      pub fn abbrev(self) -> &'static str {
        use Piece::*;
        match self { $($p => $x3),+ }
      }
      pub fn name_en(self) -> &'static str {
        use Piece::*;
        match self { $($p => $x4),+ }
      }
      pub fn name_jp(self) -> &'static str {
        use Piece::*;
        match self { $($p => $x5),+ }
      }
      pub fn movecode(self) -> (u8,u8,u8,u8) {
        use Piece::*;
        match self { $($p => $x6),+ }
      }
      pub fn promotes(self) -> Option<Piece> {
        use Piece::*;
        match self { $($p => $x7),+ }
      }
    }
  }
}

// (Piece,kanji1,kanji,Abbrev,EName,JName,(steps,step2,slide,special),promotes)
process_pieces!{
(BearsEyes,'熊',"熊眼","BE","Bear's Eyes","yūgan",(0xff,0,0,0),None),
(BlindDog,'犬',"盲犬","BD","Blind Dog","mōken",(!N&!SW&!SE,0,0,0),Some(ViolentWolf)),
(ClimbingMonkey,'猿',"登猿","CM","Climbing Monkey","tōen",(NE|N|NW|S,0,0,0),Some(ViolentStag)),
(CloudEagle,'鷲',"靏玉","CE","Cloud Eagle","unjū",(!N&!S,NE|NW,0,EAGLE),None),
(CraneKing,'靏',"靏玉","CK","Crane King","kakugyoku",(0xff,0,0,0),None),
(FlyingCock,'鶏',"鶏飛","FC","Flying Cock","keihi",(E|NE|NW|W,0,0,0),Some(RaidingFalcon)),
(FlyingFalcon,'鷹',"飛鷹","FF","Flying Falcon","hiyō",(N,0,NE|NW|SW|SE,0),Some(TenaciousFalcon)),
(FlyingGoose,'鳫',"鳫飛","FG","Flying Goose","ganhi",(NE|N|NW|S,0,0,0),Some(SwallowsWings)),
(GlidingSwallow,'行',"燕行","GS","Gliding Swallow","engyō",(0,0,E|N|W|S,0),None),
(GoldenBird,'金',"金鳥","GB","Golden Bird","kinchō",(!SW&!SE,0,0,0),None),
(HeavenlyHorse,'天',"天馬","HH","Heavenly Horse","temma",(0,0,0,HORSE),None),
(LiberatedHorse,'風',"風馬","LH","Liberated Horse","fūma",(S,S,N,0),Some(HeavenlyHorse)),
(Oxcart,'車',"牛車","OC","Oxcart","gissha",(0,0,N,0),Some(PloddingOx)),
(PloddingOx,'牛',"歬牛","PO","Plodding Ox","sengyū",(0xff,0,0,0),None),
(RaidingFalcon,'延',"延鷹","RF","Raiding Falcon","en’yō",(E|NE|NW|W,0,N|S,0),None),
(RoamingBoar,'猪',"行猪","RB","Roaming Boar","gyōcho",(!S,0,0,0),None),
(RunningRabbit,'兎',"走兎","RR","Running Rabbit","sōto",(!N&!E&!W,0,N,0),Some(TreacherousFox)),
(SparrowPawn,'歩',"萑歩","SP","Sparrow Pawn","jakufu",(N,0,0,0),Some(GoldenBird)),
(StruttingCrow,'烏',"烏行","SC","Strutting Crow","ukō",(N|SW|SE,0,0,0),Some(FlyingFalcon)),
(SwallowsWings,'燕',"燕羽","SW","Swallow's Wings","en’u",(N|S,0,E|W,0),Some(GlidingSwallow)),
(SwoopingOwl,'鴟',"鴟行","SO","Swooping Owl","shigyō",(N|SW|SE,0,0,0),Some(CloudEagle)),
(TenaciousFalcon,'鷹',"執鷹","TF","Tenacious Falcon","keiyō",(E|W,0,!E&!W,0),None), //TODO: 2-kanji probably incorrect
(TreacherousFox,'狐',"隠狐","TF","Treacherous Fox","inko",(!E&!W,0,0,FOX),None),
(ViolentStag,'鹿',"猛鹿","VS","Violent Stag","mōroku",(!E&!W&!S,0,0,0),Some(RoamingBoar)),
(ViolentWolf,'狼',"猛狼","VW","Violent Wolf","mōrō",(!SW&!SE,0,0,0),Some(BearsEyes)),
}

////////////////////////////////////////////////////////////

// E,NE,N,NW, W,SW,S,SE
const STEP : [isize;8] = [ 1, 1-11, -11, -1-11, -1, -1+11, 11, 1+11 ];

// 0x3step_2step_1step
const BOARD_STEP : [u32; 11*11] = [
0x838383,0x83838f,0x838f8f,0x8f8f8f,0x8f8f8f,0x8f8f8f,0x8f8f8f,0x8f8f8f,0x0e8f8f,0x0e0e8f,0x0e0e0e,
0x8383e3,0x8383ff,0x838fff,0x8f8fff,0x8f8fff,0x8f8fff,0x8f8fff,0x8f8fff,0x0e8fff,0x0e0eff,0x0e0e3e,
0x83e3e3,0x83e3ff,0x83ffff,0x8fffff,0x8fffff,0x8fffff,0x8fffff,0x8fffff,0x0effff,0x0e3eff,0x0e3e3e,
0xe3e3e3,0xe3e3ff,0xe3ffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0x3effff,0x3e3eff,0x3e3e3e,
0xe3e3e3,0xe3e3ff,0xe3ffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0x3effff,0x3e3eff,0x3e3e3e,
0xe3e3e3,0xe3e3ff,0xe3ffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0x3effff,0x3e3eff,0x3e3e3e,
0xe3e3e3,0xe3e3ff,0xe3ffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0x3effff,0x3e3eff,0x3e3e3e,
0xe3e3e3,0xe3e3ff,0xe3ffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0x3effff,0x3e3eff,0x3e3e3e,
0xc0e3e3,0xc0e3ff,0xc0ffff,0xf8ffff,0xf8ffff,0xf8ffff,0xf8ffff,0xf8ffff,0x38ffff,0x3838ff,0x383e3e,
0xc0c0e3,0xc0c0ff,0xc0f8ff,0xf8f8ff,0xf8f8ff,0xf8f8ff,0xf8f8ff,0xf8f8ff,0x38f8ff,0x3838ff,0x38383e,
0xc0c0c0,0xc0c000,0xc0f8f8,0xf8f8f8,0xf8f8f8,0xf8f8f8,0xf8f8f8,0xf8f8f8,0x38f8f8,0x3838f8,0x383838,
];

////////////////////////////////////////////////////////////

fn main() {
    println!("{:?}", Piece::RunningRabbit);
    println!("{:?}", Piece::RunningRabbit.kanji1());
    println!("{:?}", Piece::RunningRabbit.kanji());
    println!("{:?}", Piece::RunningRabbit.abbrev());
    println!("{:?}", Piece::RunningRabbit.name_en());
    println!("{:?}", Piece::RunningRabbit.name_jp());
    println!("{:?}", Piece::RunningRabbit.promotes());
    println!("{:?}", Piece::RunningRabbit.movecode());
}
