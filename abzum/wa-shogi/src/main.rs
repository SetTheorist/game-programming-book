

const E  : u8 = 0b0000_0001;
const NE : u8 = 0b0000_0010;
const N  : u8 = 0b0000_0100;
const NW : u8 = 0b0000_1000;
const W  : u8 = 0b0001_0000;
const SW : u8 = 0b0010_0000;
const S  : u8 = 0b0100_0000;
const SE : u8 = 0b1000_0000;
const DI : u8 = NE|NW|SW|SE;

// (Piece,kanji1,kanji,Abbrev,EName,JName,(steps,step2,step3,slide,jump2),promotes)
(CraneKing,'靏',"靏玉","CK","Crane King","kakugyoku",(0xff,0,0,0,0),false),
(CloudEagle,'鷲',"靏玉","CE","Cloud Eagle","unjū",(!N&!S,NE|NW,NE|NW,0,0),false),
(FlyingFalcon,'鷹',"飛鷹","FF","Flying Falcon","hiyō",(N,0,0,DI,0),true),
(TenaciousFalcon,'
(SwallowsWings,'燕',"燕羽","SW","Swallow's Wings","en’u",(N|S,0,0,E|W,0),true),
(TreacherousFox,'狐',"隠狐","TF","Treacherous Fox","inko",(!E&!W,0,0,0,!E&!W),false),
(RunningRabbit,'兎',"走兎","RR","Running Rabbit","sōto",(!N&!E&!W,0,0,N,0),true),
(ViolentWolf,'狼',"猛狼","VW","Violent Wolf","mōrō",(!SW&!SE,0,0,0,0),true),
(ViolentStag,'鹿',"猛鹿","VS","Violent Stag","mōroku",(!E&!W&!S,0,0,0,0),true),
(FlyingGoose,'鳫',"鳫飛","FG","Flying Goose","ganhi",(NE|N|NW|S,0,0,0,0),true),
(FlyingCock,'鶏',"鶏飛","FC","Flying Cock","keihi",(E|NE|NW|W,0,0,0,0),true),
(StruttingCrow,'烏',"烏行","SC","Strutting Crow","ukō",(N|SW|SE,0,0,0,0),true),
(SwoopingOwl,'鴟',"鴟行","SO","Swooping Owl","shigyō",(N|SW|SE,0,0,0,0),true),
(BlindDog,'犬',"盲犬","BD","Blind Dog","mōken",(!N&!SW&!SE,0,0,0,0),true),
(ClimbingMonkey,'猿',"登猿","CM","Climbing Monkey","tōen",(NE|N|NW|S,0,0,0,0),true),
(LiberatedHorse,'風',"風馬","LH","Liberated Horse","fūma",(S,S,0,N,0),true),
(Oxcart,'車',"牛車","OC","Oxcart","gissha",(0,0,0,N,0),true),
(SparrowPawn,'歩',"萑歩","SP","Sparrow Pawn","jakufu",(N,0,0,0,0),true),

fn main() {
    println!("Hello, world!");
}
