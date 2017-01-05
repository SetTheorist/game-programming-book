#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

/* (和将棋, wa shōgi, peaceful chess) */

static const char* number_kanji[11] = {
  "一", "二", "三", "四", "五", "六", "七", "八", "九", "十", "十一"
};

enum colort {
  none=0, white=1, black=2,
  NCOLOR
};

enum piecet {
 empty=0,         crane_king,       cloud_eagle,      flying_falcon,
 swallows_wings,  treacherous_fox,  running_rabbit,   violent_wolf,
 violent_stag,    flying_goose,     flying_cock,      strutting_crow,
 swooping_owl,    blind_dog,        climbing_monkey,  liberated_horse,
 oxcart,          sparrow_pawn,
 NPIECE
};

static const char* piece_names[NPIECE][2][3] = {
  {{"??","??","??"},{"??","??","??"}},
  {{"Crane King", "靏玉", "kakugyoku"},       {"Crane King", "靏玉", "kakugyoku"}},
  {{"Cloud Eagle", "雲鷲", "unjū"},           {"Cloud Eagle", "雲鷲", "unjū"}},
  {{"Flying Falcon", "飛鷹", "hiyō"},         {"Tenacious Falcon", "鶏鷹", "keiyō"}},
  {{"Swallow's Wings", "燕羽", "en’u"},       {"Gliding Swallow", "燕行", "engyō"}}, 
  {{"Treacherous Fox", "隠狐", "inko, onko"}, {"Treacherous Fox", "隠狐", "inko, onko"}},
  {{"Running Rabbit", "走兎", "sōto"},        {"Treacherous fox", "隠狐", "inko, onko"}}, 
  {{"Violent Wolf", "猛狼", "mōrō"},          {"Bear's Eyes", "熊眼", "yūgan"}}, 
  {{"Violent Stag", "猛鹿", "mōroku"},        {"Roaming Boar", "行猪", "gyōcho"}}, 
  {{"Flying Goose", "鳫飛", "ganhi"},         {"Swallow's Wings", "燕羽", "en’u"}}, 
  {{"Flying Cock", "鶏飛", "keihi"},          {"Raiding Falcon", "延鷹", "en’yō"}}, 
  {{"Strutting Crow", "烏行", "ukō"},         {"Flying Falcon", "飛鷹", "hiyō"}}, 
  {{"Swooping Owl", "鴟行", "shigyō"},        {"Cloud Eagle", "雲鷲", "unjū"}}, 
  {{"Blind Dog", "盲犬", "mōken"},            {"Violent Wolf", "猛狼", "mōrō"}}, 
  {{"Climbing Monkey", "登猿", "tōen"},       {"Violent Stag", "猛鹿", "mōroku"}}, 
  {{"Liberated Horse", "風馬", "fūma"},       {"Heavenly Horse", "天馬", "temma"}}, 
  {{"Oxcart", "牛車", "gissha"},              {"Plodding Ox", "歬牛", "sengyū"}}, 
  {{"Sparrow Pawn", "萑歩", "jakufu"},        {"Golden Bird", "金鳥", "kinchō"}}, 
};

/* encode moves as 1-step, 2-step, 3-step, slide in bits of a byte
 * have mask table for each square on the board for 1-step & 2-step
 * to filter...
 * (first did this for chu-shogi)
 */

#define NN  (1u<<0)
#define NE  (1u<<1)
#define EE  (1u<<2)
#define SE  (1u<<3)
#define SS  (1u<<4)
#define SW  (1u<<5)
#define WW  (1u<<6)
#define NW  (1u<<7)

// for knight-jump only
#define NNE (1u<<0)
#define EEN (1u<<1)
#define EES (1u<<2)
#define SSE (1u<<3)
#define SSW (1u<<4)
#define WWS (1u<<5)
#define WWN (1u<<6)
#define NNW (1u<<7)

#define X1(n) ((n))
#define X2(n) ((n)<<8)
#define X3(n) ((n)<<16)
#define Xk(n) ((n)<<16) // overloaded - only for heavenly_horse = promoted liberated_horse
#define Xn(n) ((n)<<24)

const int step1_delt[8] = { +11,   +11+1,    +1, -11+1, -11,   -11-1,    -1, +11-1 };
const int step2_delt[8] = { +22,   +22+2,    +2, -22+2, -22,   -22-2,    -2, +22-2 };
const int step3_delt[8] = { +33,   +33+3,    +3, -33+3, -33,   -33-3,    -3, +33-3 };
const int stepk_delt[8] = { +22+1, +11+2, -11+2, -22+1, -22-1, -11-2, +11-2, +22-1 };

// color, piece, promoted?
static const unsigned int piece_moves[NCOLOR][NPIECE][2] = {
  {},
  {
   { /* empty */            0, 0},
   { /* crane_king */       X1(NN|NE|EE|SE|SS|SW|WW|NW),
                            X1(NN|NE|EE|SE|SS|SW|WW|NW)},
   { /* cloud_eagle */      X1(EE|WW|SE|SW|NE|NW)|X2(NE|NW)|X3(NE|NW)|Xn(NN|SS),
                            X1(EE|WW|SE|SW|NE|NW)|X2(NE|NW)|X3(NE|NW)|Xn(NN|SS)},
   { /* flying_falcon */    X1(NN)|Xn(NE|SE|SW|NW),
                            X1(EE|WW)|Xn(NW|NN|NE|SW|SS|SE)},
   { /* swallows_wings */   X1(NN|SS)|Xn(EE|WW),
                            Xn(NN|SS|EE|WW)},
   { /* treacherous_fox */  X1(NW|NN|NE|SW|SS|SE)|X2(NW|NN|NE|SW|SS|SE),
                            X1(NW|NN|NE|SW|SS|SE)|X2(NW|NN|NE|SW|SS|SE)},
   { /* running_rabbit */   X1(NW|NE|SW|SS|SE)|Xn(NN),
                            X1(NW|NN|NE|SW|SS|SE)|X2(NW|NN|NE|SW|SS|SE)},
   { /* violent_wolf */     X1(NW|NN|NE|WW|EE|SS),
                            X1(NN|NE|EE|SE|SS|SW|WW|NW)},
   { /* violent_stag */     X1(NW|NN|NE|SW|SE),
                            X1(SW|WW|NW|NN|NE|EE|SE)},
   { /* flying_goose */     X1(NW|NN|NE|SS),
                            X1(NN|SS)|Xn(EE|WW)},
   { /* flying_cock */      X1(NW|WW|NE|EE),
                            X1(NW|WW|NE|EE)|Xn(NN|SS)},
   { /* strutting_crow */   X1(NN|SE|SW),
                            X1(NN)|Xn(NE|SE|SW|NW)},
   { /* swooping_owl */     X1(NN|SE|SW),
                            X1(EE|WW|SE|SW|NE|NW)|X2(NE|NW)|X3(NE|NW)|Xn(NN|SS)},
   { /* blind_dog */        X1(NE|NW|EE|WW|SS),
                            X1(NW|NN|NE|WW|EE|SS)},
   { /* climbing_monkey */  X1(NE|NN|NW|SS),
                            X1(NW|NN|NE|SW|SE)},
   { /* liberated_horse */  X1(SS)|X2(SS)|Xn(NN),
                            Xk(NNE|NNW|SSE|SSW)},
   { /* oxcart */           Xn(NN),
                            X1(NN|NE|EE|SE|SS|SW|WW|NW)},
   { /* sparrow_pawn */     X1(NN),
                            X1(NW|NN|NE|EE|WW|SS)},
  },
  {
   { /* empty */            0, 0},
   { /* crane_king */       X1(SS|SE|EE|NE|NN|NW|WW|SW),
                            X1(SS|SE|EE|NE|NN|NW|WW|SW)},
   { /* cloud_eagle */      X1(EE|WW|NE|NW|SE|SW)|X2(SE|SW)|X3(SE|SW)|Xn(SS|NN),
                            X1(EE|WW|NE|NW|SE|SW)|X2(SE|SW)|X3(SE|SW)|Xn(SS|NN)},
   { /* flying_falcon */    X1(SS)|Xn(SE|NE|NW|SW),
                            X1(EE|WW)|Xn(SW|SS|SE|NW|NN|NE)},
   { /* swallows_wings */   X1(SS|NN)|Xn(EE|WW),
                            Xn(SS|NN|EE|WW)},
   { /* treacherous_fox */  X1(SW|SS|SE|NW|NN|NE)|X2(SW|SS|SE|NW|NN|NE),
                            X1(SW|SS|SE|NW|NN|NE)|X2(SW|SS|SE|NW|NN|NE)},
   { /* running_rabbit */   X1(SW|SE|NW|NN|NE)|Xn(SS),
                            X1(SW|SS|SE|NW|NN|NE)|X2(SW|SS|SE|NW|NN|NE)},
   { /* violent_wolf */     X1(SW|SS|SE|WW|EE|NN),
                            X1(SS|SE|EE|NE|NN|NW|WW|SW)},
   { /* violent_stag */     X1(SW|SS|SE|NW|NE),
                            X1(NW|WW|SW|SS|SE|EE|NE)},
   { /* flying_goose */     X1(SW|SS|SE|NN),
                            X1(SS|NN)|Xn(EE|WW)},
   { /* flying_cock */      X1(SW|WW|SE|EE),
                            X1(SW|WW|SE|EE)|Xn(SS|NN)},
   { /* strutting_crow */   X1(SS|NE|NW),
                            X1(SS)|Xn(SE|NE|NW|SW)},
   { /* swooping_owl */     X1(SS|NE|NW),
                            X1(EE|WW|NE|NW|SE|SW)|X2(SE|SW)|X3(SE|SW)|Xn(SS|NN)},
   { /* blind_dog */        X1(SE|SW|EE|WW|NN),
                            X1(SW|SS|SE|WW|EE|NN)},
   { /* climbing_monkey */  X1(SE|SS|SW|NN),
                            X1(SW|SS|SE|NW|NE)},
   { /* liberated_horse */  X1(NN)|X2(NN)|Xn(SS),
                            Xk(SSE|SSW|NNE|NNW)},
   { /* oxcart */           Xn(SS),
                            X1(SS|SE|EE|NE|NN|NW|WW|SW)},
   { /* sparrow_pawn */     X1(SS),
                            X1(SW|SS|SE|EE|WW|NN)},
  },
};
#undef NN
#undef NE
#undef EE
#undef SE
#undef SS
#undef SW
#undef WW
#undef NW
#undef NNE
#undef EEN
#undef EES
#undef SSE
#undef SSW
#undef WWS
#undef WWN
#undef NNW
#undef X1
#undef X2
#undef X3
#undef Xk
#undef Xn

// stepn|step3|step2|step1
static const unsigned int board_move_mask[11*11] = {
  0x07070707,0xC70707C7,0xC707C7C7,0xC7C7C7C7,0xC7C7C7C7,0xC7C7C7C7,0xC7C7C7C7,0xC7C7C7C7,0xC7C1C7C7,0xC7C1C1C7,0xC1C1C1C1,
  0x1F07071F,0xFF0707FF,0xFF07C7FF,0xFFC7C7FF,0xFFC7C7FF,0xFFC7C7FF,0xFFC7C7FF,0xFFC7C7FF,0xFFC1C7FF,0xFFC1C1FF,0xF1C1C1F1,
  0x1F071F1F,0xFF071FFF,0xFF07FFFF,0xFFC7FFFF,0xFFC7FFFF,0xFFC7FFFF,0xFFC7FFFF,0xFFC7FFFF,0xFFC1FFFF,0xFFC1F1FF,0xF1C1F1F1,
  0x1F1F1F1F,0xFF1F1FFF,0xFF1FFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFF1FFFF,0xFFF1F1FF,0xF1F1F1F1,
  0x1F1F1F1F,0xFF1F1FFF,0xFF1FFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFF1FFFF,0xFFF1F1FF,0xF1F1F1F1,
  0x1F1F1F1F,0xFF1F1FFF,0xFF1FFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFF1FFFF,0xFFF1F1FF,0xF1F1F1F1,
  0x1F1F1F1F,0xFF1F1FFF,0xFF1FFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFF1FFFF,0xFFF1F1FF,0xF1F1F1F1,
  0x1F1F1F1F,0xFF1F1FFF,0xFF1FFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFF1FFFF,0xFFF1F1FF,0xF1F1F1F1,
  0x1F1C1F1F,0xFF1C1FFF,0xFF1CFFFF,0xFF7CFFFF,0xFF7CFFFF,0xFF7CFFFF,0xFF7CFFFF,0xFF7CFFFF,0xFF70FFFF,0xFF70F1FF,0xF170F1F1,
  0x1F1C1C1F,0xFF1C1CFF,0xFF1C7CFF,0xFF7C7CFF,0xFF7C7CFF,0xFF7C7CFF,0xFF7C7CFF,0xFF7C7CFF,0xFF707CFF,0xFF7070FF,0xF17070F1,
  0x1C1C1C1C,0x7C1C1C7C,0x7C1C7C7C,0x7C7C7C7C,0x7C7C7C7C,0x7C7C7C7C,0x7C7C7C7C,0x7C7C7C7C,0x7C707C7C,0x7C70707C,0x70707070,
};
// -----|stepk|-----|-----
static const unsigned int board_move_mask_k[11*11] = {
  0x00030000,0x00830000,0x00C30000,0x00C30000,0x00C30000,0x00C30000,0x00C30000,0x00C30000,0x00C30000,0x00C10000,0x00C00000,
  0x00070000,0x00870000,0x00E70000,0x00E70000,0x00E70000,0x00E70000,0x00E70000,0x00E70000,0x00E70000,0x00E10000,0x00E00000,
  0x000F0000,0x009F0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00F90000,0x00F00000,
  0x000F0000,0x009F0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00F90000,0x00F00000,
  0x000F0000,0x009F0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00F90000,0x00F00000,
  0x000F0000,0x009F0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00F90000,0x00F00000,
  0x000F0000,0x009F0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00F90000,0x00F00000,
  0x000F0000,0x009F0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00F90000,0x00F00000,
  0x000F0000,0x009F0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000,0x00F90000,0x00F00000,
  0x000E0000,0x001E0000,0x007E0000,0x007E0000,0x007E0000,0x007E0000,0x007E0000,0x007E0000,0x007E0000,0x00780000,0x00700000,
  0x000C0000,0x001C0000,0x003C0000,0x003C0000,0x003C0000,0x003C0000,0x003C0000,0x003C0000,0x003C0000,0x00380000,0x00300000,
};


static const int promotable = 0x0003FFD8;

struct movet {
  union {
    unsigned int i;
    struct {
      unsigned w   : 1; //  0       white?
      unsigned f   : 7; //  1-- 7   from-square
      unsigned t   : 7; //  8--14   to-square
      unsigned p   : 5; // 15--19   piece moving
      unsigned pp  : 1; // 20       promoted piece moving?
      unsigned fc  : 1; // 21       capture?
      unsigned fpp : 1; // 22       possible-promotion?
      unsigned fp  : 1; // 23       promotion?
      unsigned cp  : 5; // 24--28   captured piece
      unsigned cpp : 1; // 29       promoted captured piece?
      unsigned fd  : 1; // 30       drop? [when playing with drops]
      unsigned xx  : 1; // 31       <unused>
    };
  };
  int showf(FILE* ff) const {
    int n = 0;
    n += fprintf(ff, "%c", w ? 'w' : 'b');
    if (pp) n += fprintf(ff, "+");
    n += fprintf(ff, "%s", piece_names[p][pp][1]);
    n += fprintf(ff, "%c%i", 'a'+(f%11), 1+(f/11));
    n += fprintf(ff, "%c", fc ? 'x' : '-');
    n += fprintf(ff, "%c%i", 'a'+(t%11), 1+(t/11));
    if (fpp) n += fprintf(ff, "%c", fp ? '+' : '=');
    return n;
  }
};

struct squaret {
  colort    color     : 2;
  unsigned  promoted  : 1;
  piecet    piece     : 5;
};

struct boardt {
  static const squaret init_squares[11*11];

  squaret squares[11*11];
  colort  to_move;
  int     is_terminal;
  colort  result;

  int     num_moves;
  movet   move_stack[1024];
  hasht   hash_stack[1024];

  int     allow_drops;

  hasht   square_hash[11*11][NCOLOR][2][NPIECE];
  hasht   to_move_hash[NCOLOR];

  static inline int rc2x(int r, int c) { return r*11+c; }
  static inline int x2c(int x) { return x%11; }
  static inline int x2r(int x) { return x/11; }

  boardt(int allow_drops_=0) : allow_drops(allow_drops_) {
    memset(this, '\x00', sizeof(*this));
    init_hash();
    memcpy(squares, init_squares, sizeof(squares));
    to_move = white;
    hash_stack[num_moves] = compute_hash();
  }

  void init_hash() {
    for (int i=0; i<NCOLOR; ++i)
      to_move_hash[i] = gen_hash();
    for (int i=0; i<11*11; ++i)
      for (int j=0; j<NCOLOR; ++j)
        for (int k=0; k<2; ++k)
          for (int l=0; l<NPIECE; ++l)
            square_hash[i][j][k][l] = gen_hash();
  }

  hasht compute_hash() const {
    hasht h = to_move_hash[to_move];
    for (int i=0; i<11*11; ++i)
      h ^= square_hash[i][squares[i].color][squares[i].promoted][squares[i].piece];
    return h;
  }

  inline int add_move(int& n, movet ml[], int ff, int tt) const {
    if (squares[tt].color == to_move) return 0;
    ml[n].i = 0;
    ml[n].w = (to_move==white);
    ml[n].f = ff;
    ml[n].t = tt;
    ml[n].p = squares[ff].piece;
    ml[n].pp = squares[ff].promoted;
    if (squares[tt].color != none) {
      ml[n].fc = 1;
      ml[n].cp = squares[tt].piece;
      ml[n].cpp = squares[tt].promoted;
    }
    if (!squares[ff].promoted && (promotable&(1<<squares[tt].piece))) {
      if (  (to_move==white && (ff>=8*11 || tt>=8*11))
         || (to_move==black && (ff< 3*11 || tt< 3*11)) ) {
        ml[n].fpp = 1;
        ml[n].fp = 1;
        // TODO: forced promotions... (oxcart & sparrow-pawn in last rank)
        ml[n+1] = ml[n];
        ++n;
        ml[n].fp = 0;
      }
    }
    ++n;
    return (squares[tt].color == none);
  }

  // 10MM random games gives max 110 moves generated, but
  // likely it is not the maximum possible, but should be close
  // 128 is probably a (practically) safe upper bound
  // TODO: some of these are incorrect: bounded slides, not jumps...
  //    (cloud_eagle/promoted-swooping_owl and liberated_horse)
  int gen_moves(movet ml[]) const {
    int n = 0;
    for (int i=0; i<11*11; ++i) {
      if (squares[i].color == to_move) {
        if (squares[i].promoted && squares[i].piece==liberated_horse) {
          const unsigned int moves_k = piece_moves[to_move][squares[i].piece][squares[i].promoted] & board_move_mask_k[i];
          const unsigned int stepk = ((moves_k & 0x00FF0000)>>16);
          if (stepk)
            for (int k=0; k<8; ++k)
              if (stepk & (1<<k))
                add_move(n,ml,i,i+stepk_delt[k]);
        } else {
          const unsigned int moves = piece_moves[to_move][squares[i].piece][squares[i].promoted] & board_move_mask[i];
          const unsigned int step1 = ((moves & 0x000000FF)>> 0);
          if (step1)
            for (int k=0; k<8; ++k)
              if (step1 & (1<<k))
                add_move(n,ml,i,i+step1_delt[k]);
          const unsigned int step2 = ((moves & 0x0000FF00)>> 8);
          if (step2)
            for (int k=0; k<8; ++k)
              if (step2 & (1<<k))
                add_move(n,ml,i,i+step2_delt[k]);
          const unsigned int step3 = ((moves & 0x00FF0000)>>16);
          if (step3) {
            for (int k=0; k<8; ++k)
              if (step3 & (1<<k))
                add_move(n,ml,i,i+step3_delt[k]);
          }
          const unsigned int stepn = ((moves & 0xFF000000)>>24);
          if (stepn) {
            for (int k=0; k<8; ++k) {
              if (stepn & (1<<k))  {
                int t = i+step1_delt[k];
                do {
                  if (!add_move(n,ml,i,t)) break;
                  t += step1_delt[k];
                } while (board_move_mask[t] & (1<<(k+24)));
              }
            }
          }
        }
      }
    }
    return n;
  }

  int make_move(movet m) {
    static const squaret blank = {none,0,empty};

    if (m.p) {
      squares[m.t] = squares[m.f];
      squares[m.f] = blank;

      if (m.fp)
        squares[m.t].promoted = 1;

      if (m.fc && m.cp==crane_king) {
        is_terminal = 1;
        result = to_move;
      }
    }

    to_move = (colort)(to_move ^ 3);
    move_stack[num_moves++] = m;
    hash_stack[num_moves] = compute_hash();

    /* 4th time repetition is loss (sennichite rule) */
    int cnt = 0;
    for (int i=num_moves-1; i>=0 && cnt<3; --i)
      if (hash_stack[i]==hash_stack[num_moves])
        ++cnt;
    if (cnt>=3) {
      is_terminal = 1;
      result = to_move;
    }

    return 0;
  }

  int unmake_move() {
    static const squaret blank = {none,0,empty};
    movet m = move_stack[--num_moves];
    to_move = (colort)(to_move ^ 3);

    is_terminal = 0;
    result = none;

    if (m.p) {
      squares[m.f] = squares[m.t];
      if (m.fc) {
        squares[m.t].color = (colort)(to_move^3);
        squares[m.t].piece = (piecet)m.cp;
        squares[m.t].promoted = m.cpp;
      } else {
        squares[m.t] = blank;
      }
      if (m.fp)
        squares[m.f].promoted = 0;
    }

    return 0;
  }

  int showf(FILE* ff) const {
    int n = 0;
    for (int r=10; r>=0; --r) {
      n += fprintf(ff, "    +-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+\n");
      n += fprintf(ff, " %2i |", r+1);
      for (int c=0; c<11; ++c) {
        const int x = rc2x(r,c);
        if (squares[x].color==none) {
          n += fprintf(ff, "       ");
        } else if (squares[x].color==white) {
          if (squares[x].promoted) ansi_fg_green(ff); else ansi_fg_white(ff);
          n += fprintf(ff, " w%s ", piece_names[squares[x].piece][squares[x].promoted][1]);
          ansi_reset(ff);
        } else if (squares[x].color==black) {
          if (squares[x].promoted) ansi_fg_red(ff); else ansi_fg_yellow(ff);
          n += fprintf(ff, " b%s ", piece_names[squares[x].piece][squares[x].promoted][1]);
          ansi_reset(ff);
        } else {
          n += fprintf(ff, " <%i,%i> ", squares[x].color, squares[x].piece);
        }
        n += fprintf(ff, "|");
      }
      n += fprintf(ff, " %s ", number_kanji[10-r]);
      if (r==10) n += fprintf(ff, "[%016llX]", hash_stack[num_moves]);
      if (r==9) n += fprintf(ff, "[%016llX]", compute_hash());
      n += fprintf(ff, "\n");
    }
    n += fprintf(ff, "    +-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+\n");
    n += fprintf(ff, "        a       b       c       d       e       f       g       h       i       j       k    \n");
    if (is_terminal) n += fprintf(ff, "*** %s wins the game ***\n", result==white?"White":result==black?"Black":"---");
    else n += fprintf(ff, "%s to move\n", to_move==white?"White":to_move==black?"Black":"---");
    return 0;
  }
};
#define W(x)  {white,0,(x)},
#define B(x)  {black,0,(x)},
#define E     {none,0,empty},
#define CK  crane_king
#define CE  cloud_eagle
#define FF  flying_falcon
#define SW  swallows_wings
#define TF  treacherous_fox
#define RR  running_rabbit
#define VW  violent_wolf
#define VS  violent_stag
#define FG  flying_goose
#define FC  flying_cock
#define SC  strutting_crow
#define SO  swooping_owl
#define BD  blind_dog
#define CM  climbing_monkey
#define LH  liberated_horse
#define OC  oxcart
#define SP  sparrow_pawn
const squaret boardt::init_squares[11*11] = {
  W(OC) W(BD) W(SC) W(FG) W(VW) W(CK) W(VS) W(FC) W(SO) W(CM) W(LH)
  E     W(FF) E     E     E     W(SW) E     E     E     W(CE) E
  W(SP) W(SP) W(SP) W(TF) W(SP) W(SP) W(SP) W(RR) W(SP) W(SP) W(SP)
  E     E     E     W(SP) E     E     E     W(SP) E     E     E
  E     E     E     E     E     E     E     E     E     E     E 
  E     E     E     E     E     E     E     E     E     E     E 
  E     E     E     E     E     E     E     E     E     E     E 
  E     E     E     B(SP) E     E     E     B(SP) E     E     E
  B(SP) B(SP) B(SP) B(RR) B(SP) B(SP) B(SP) B(TF) B(SP) B(SP) B(SP)
  E     B(CE) E     E     E     B(SW) E     E     E     B(FF) E
  B(LH) B(CM) B(SO) B(FC) B(VS) B(CK) B(VW) B(FG) B(SC) B(BD) B(OC)
};
#undef W
#undef B
#undef E
#undef CK
#undef CE
#undef FF
#undef SW
#undef TF
#undef RR
#undef VW
#undef VS
#undef FG
#undef FC
#undef SC
#undef SO
#undef BD
#undef CM
#undef LH
#undef OC
#undef SP

int gmaxn = 0;
ui64 perft(boardt& b, int depth) {
  if (depth<=0 || b.is_terminal)
    return 1;
  ui64 c = 0;
  movet ml[512];
  int n = b.gen_moves(ml);
  if (n>gmaxn) gmaxn = n;
  for (int i=0; i<n; ++i) {
    b.make_move(ml[i]);
    c += perft(b, depth-1);
    b.unmake_move();
  }
  return c;
}


int main(int argc, char* argv[]) {
  srand48(argc>1 ? atoi(argv[1]) : -13);

  if (1)
  {
    boardt  b;
    for (int d=0; d<=4; ++d) {
      ui64 c = perft(b, d);
      printf("%2i %12llu (%i)\n", d, c, gmaxn);
    }
  }

  if (0)
  {
    boardt  b;
    int maxn = 0;
    for (int gg=0; gg<1000000; ++gg)
    {
      if (!(gg%1000)) printf("."); fflush(stdout);
      if (!(gg%10000)) printf(","); fflush(stdout);
      if (!(gg%100000)) printf(":"); fflush(stdout);
      if (!(gg%1000000)) printf("="); fflush(stdout);
      //printf("--------------------------------------------------------------------------------\n");
      //printf("--------------------------------------------------------------------------------\n");
      //b.showf(stdout);
      while (!b.is_terminal && b.num_moves<256)
      {
        movet   ml[512];
        int     n = b.gen_moves(ml);
        if (n>maxn) maxn = n;
        /*for (int i=0; i<n; ++i) {
          ml[i].showf(stdout);
          printf((i%5==4)?"\n":"\t");
        }
        if (n%5) printf("\n");*/
        b.make_move(ml[lrand48()%n]);
        //b.showf(stdout);
      }
      //printf("--------------------------------------------------------------------------------\n");
      while (b.num_moves)
        b.unmake_move();
      //b.showf(stdout);
    }
    b.showf(stdout);
    printf("--- %i ---\n", maxn);
  }

  return 0;
}


