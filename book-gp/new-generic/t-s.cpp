#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long long int hasht;
static hasht gen_hash() {
  return
     (((((hasht)lrand48())<<5)^((hasht)lrand48())^(((hasht)lrand48())>>5))<<32)
    ^(((((hasht)lrand48())<<5)^((hasht)lrand48())^(((hasht)lrand48())>>5))<<16)
    ^(((((hasht)lrand48())<<5)^((hasht)lrand48())^(((hasht)lrand48())>>5)));
}

enum piecet {
  empty, phoenix, falcon, crane, pheasant, quail_l, quail_r, swallow, eagle, goose, block,
  NPIECE
};
static const piecet promotion[NPIECE] = {
  empty, phoenix, eagle, crane, pheasant, quail_l, quail_r, goose, eagle, goose, block,
};
static const piecet unpromotion[NPIECE] = {
  empty, phoenix, falcon, crane, pheasant, quail_l, quail_r, swallow, falcon, swallow, block,
};
static const char* piece_kanji[NPIECE] = {
  "--", "鵬", "鷹", "鶴", "雉", "鶉左", "鶉右", "燕", "鵰", "鴈", "xx"
};
static const int piece_kanji_len[NPIECE] = { 2, 2, 2, 2, 2, 4, 4, 2, 2, 2, 2 };
enum colort { none=0, white=1, black=2, border=3, NCOLOR };
static const char* color_kanji[NCOLOR] = { "**", "先手", "後手", "##" };
static const int color_kanji_len[NCOLOR] = { 2, 4, 4, 2 };

static const char* number_kanji[7] = { "一", "二", "三", "四", "五", "六", "七" };

static inline int x2r(int x) { return (x/11)-2; }
static inline int x2c(int x) { return (x%11)-2; }
static inline int rc2x(int r, int c) { return (r+2)*11 + (c+2); }

struct movet {
  union {
    int i;
    struct {
      unsigned w : 1;   //  0     wtm?
      unsigned f : 7;   //  1-- 7 from-square
      unsigned t : 7;   //  8--14 to-square
      unsigned p : 4;   // 15--18 moved-piece
      unsigned d : 1;   // 19     drop?
      unsigned c : 1;   // 20     capture?
      unsigned q : 4;   // 21--24 captured-piece
      unsigned x : 1;   // 25     promotion?
    };
  };
  int showf(FILE* ff) {
    int n = 0;
    fprintf(ff, "(%s)", color_kanji[w?white:black]); n += color_kanji_len[w?white:black]+2;
    fprintf(ff, "%s", piece_kanji[p]); n += piece_kanji_len[p];
    if (!d) n += fprintf(ff, "%c%c", x2c(f)+'a', x2r(f)+'1');
    n += fprintf(ff, "%c", d?'*':c?'x':'-');
    n += fprintf(ff, "%c%c", x2c(t)+'a', x2r(t)+'1');
    if (x) n += fprintf(ff, "%c", '+');
    return n;
  }
};

struct board {
  static const colort init_color[11*11];
  static const piecet init_piece[11*11];

  colort  color[11*11];
  piecet  piece[11*11];
  colort  to_move;
  int     is_terminal;
  colort  result;
  int     hand[NCOLOR][NPIECE];

  int     num_moves;
  movet   move_stack[512];
  hasht   hash_stack[512];

  hasht   to_move_hash[NCOLOR];
  hasht   hand_hash[NCOLOR][NPIECE][16];
  hasht   piece_hash[11*11][NCOLOR][NPIECE];

  board() {
    init();
  }
  void init() {
    memset(this, '\x00', sizeof(*this));
    init_hash();
    memcpy(color, init_color, sizeof(color));
    memcpy(piece, init_piece, sizeof(piece));
    to_move = white;
    hash_stack[num_moves] = compute_hash();
  }

  void init_hash() {
    for (int i=0; i<NCOLOR; ++i)
      to_move_hash[i] = gen_hash();
    for (int i=0; i<NCOLOR; ++i)
      for (int j=0; j<NPIECE; ++j)
        for (int k=0; k<16; ++k)
          hand_hash[i][j][k] = gen_hash();
    for (int i=0; i<11*11; ++i)
      for (int j=0; j<NCOLOR; ++j)
        for (int k=0; k<NPIECE; ++k)
          piece_hash[i][j][k] = gen_hash();
  }
  hasht compute_hash() const {
    hasht h = to_move_hash[to_move];
    for (int i=0; i<NCOLOR; ++i)
      for (int j=0; j<NPIECE; ++j)
        h ^= hand_hash[i][j][hand[i][j]];
    for (int i=0; i<11*11; ++i)
      h ^= piece_hash[i][color[i]][piece[i]];
    return h;
  }

  int in_check(colort side) {
    return 0;
  }

  inline int add_drop(int& n, movet ml[], int pp, int tt) const {
    ml[n].i = 0;
    ml[n].w = to_move==white;
    ml[n].t = tt;
    ml[n].p = pp;
    ml[n].d = 1;
    ++n;
    return 1;
  }
  inline int add_move(int& n, movet ml[], int pp, int ff, int tt) const {
    ml[n].i = 0;
    ml[n].w = to_move==white;
    ml[n].f = ff;
    ml[n].t = tt;
    ml[n].p = pp;
    if (color[tt] != none) {
      ml[n].c = 1;
      ml[n].q = piece[tt];
    }
    if ((pp==falcon) || (pp==swallow))
      if ( ((to_move==white) && (ff>=rc2x(5,0) || tt>=rc2x(5,0)))
        || ((to_move==black) && (ff<rc2x(2,0) || tt<rc2x(2,0))))
        ml[n].x = 1;
    ++n;
    return !ml[n].c;
  }
  int gen_moves(movet ml[]) const {
    int n = 0;
    //// TODO: swallow drop restriction - no mate
    // drops
    for (int i=1; i<NPIECE; ++i) {
      if (hand[to_move][i]) {
        if (i==swallow) {
          if (to_move==white) {
            for (int c=0; c<7; ++c) {
              int cnt = 0;
              for (int r=0; r<6; ++r)
                if (piece[(c+2)+(r+2)*11]==swallow && color[(c+2)+(r+2)*11]==to_move)
                  ++cnt;
              if (cnt < 2) {
                for (int r=0; r<6; ++r) {
                  const int t = rc2x(r,c);
                  if (color[t]==none)
                    add_drop(n, ml, i, t);
                }
              }
            }
          } else {
            for (int c=0; c<7; ++c) {
              int cnt = 0;
              for (int r=1; r<7; ++r)
                if (piece[(c+2)+(r+2)*11]==swallow && color[(c+2)+(r+2)*11]==to_move)
                  ++cnt;
              if (cnt < 2) {
                for (int r=1; r<7; ++r) {
                  const int t = rc2x(r,c);
                  if (color[t]==none)
                    add_drop(n, ml, i, t);
                }
              }
            }
          }
        } else {
          for (int r=0; r<7; ++r) {
            for (int c=0; c<7; ++c) {
              const int t = rc2x(r,c);
              if (color[t]==none)
                add_drop(n, ml, i, t);
            }
          }
        }
      }
    }
    // moves
    for (int r=6; r>=0; --r) {
      for (int c=0; c<7; ++c) {
        const int f = rc2x(r,c);
        if (color[f]==to_move) {
          switch (piece[f]) {
            case phoenix: {
              static const int steps[8] = {+11, +11+1, +1, -11+1, -11, -11-1, -1, +11-1};
              for (int i=0; i<8; ++i)
                if (!(to_move&color[f+steps[i]]))
                  add_move(n, ml, phoenix, f, f+steps[i]);
            } break;
            case falcon: {
              static const int steps[2][7] = {
                  {+11, +11+1, +1, -11+1, -11-1, -1, +11-1},
                  {-11, -11+1, +1, +11+1, +11-1, -1, -11-1}};
              for (int i=0; i<7; ++i)
                if (!(to_move&color[f+steps[to_move!=white][i]]))
                  add_move(n, ml, falcon, f, f+steps[to_move!=white][i]);
            } break;
            case crane: {
              static const int steps[6] = {+11, +11+1, -11+1, -11, -11-1, +11-1};
              for (int i=0; i<6; ++i)
                if (!(to_move&color[f+steps[i]]))
                  add_move(n, ml, crane, f, f+steps[i]);
            } break;
            case pheasant: {
              static const int steps[2][3] = {{+22,-11-1,-11+1},{-22,+11-1,+11+1}};
              for (int i=0; i<3; ++i)
                if (!(to_move&color[f+steps[to_move!=white][i]]))
                  add_move(n, ml, pheasant, f, f+steps[to_move!=white][i]);
            } break;
            case quail_l: {
              static const int steps[2][1] = {{-11-1},{+11+1}};
              for (int i=0; i<1; ++i)
                if (!(to_move&color[f+steps[to_move!=white][i]]))
                  add_move(n, ml, quail_l, f, f+steps[to_move!=white][i]);
              static const int slides[2][2] = {{+11,-11+1}, {-11,+11-1}};
              for (int i=0; i<2; ++i)
                for (int d=1; !(to_move&color[f+d*slides[to_move!=white][i]]); ++d)
                  if (!add_move(n, ml, quail_l, f, f+d*slides[to_move!=white][i]))
                    break;
            } break;
            case quail_r: {
              static const int steps[2][1] = {{-11+1},{+11-1}};
              for (int i=0; i<1; ++i)
                if (!(to_move&color[f+steps[to_move!=white][i]]))
                  add_move(n, ml, quail_r, f, f+steps[to_move!=white][i]);
              static const int slides[2][2] = {{+11,-11-1}, {-11,+11+1}};
              for (int i=0; i<2; ++i)
                for (int d=1; !(to_move&color[f+d*slides[to_move!=white][i]]); ++d)
                  if (!add_move(n, ml, quail_r, f, f+d*slides[to_move!=white][i]))
                    break;
            } break;
            case swallow: {
              static const int steps[2][1] = {{+11},{-11}};
              for (int i=0; i<1; ++i)
                if (!(to_move&color[f+steps[to_move!=white][i]]))
                  add_move(n, ml, swallow, f, f+steps[to_move!=white][i]);
            } break;
            case eagle: {
              // TODO: 2-step jump is actually slide...
              static const int steps[2][7] = {{+11,-1,+1,-11-1,-11+1,-22-2,-22+2},{-11,-1,+1,+11-1,+11+1,+22-2,+22+2}};
              for (int i=0; i<1; ++i)
                if (!(to_move&color[f+steps[to_move!=white][i]]))
                  add_move(n, ml, eagle, f, f+steps[to_move!=white][i]);
              static const int slides[2][3] = {{+11-1,+11+1,-11}, {-11-1,-11+1,+11}};
              for (int i=0; i<3; ++i)
                for (int d=1; !(to_move&color[f+d*slides[to_move!=white][i]]); ++d)
                  if (!add_move(n, ml, eagle, f, f+d*slides[to_move!=white][i]))
                    break;
            } break;
            case goose: {
              static const int steps[2][3] = {{+22-2,+22+2,-22},{-22-2,-22+2,+22}};
              for (int i=0; i<3; ++i)
                if (!(to_move&color[f+steps[to_move!=white][i]]))
                  add_move(n, ml, goose, f, f+steps[to_move!=white][i]);
            } break;
            default: break;
          }
        }
      }
    }
    return n;
  }
  int gen_moves_quiescence(movet ml[]) const {
    const colort xside = (colort)(to_move^3);
    int n = 0;
    // moves
    for (int r=6; r>=0; --r) {
      for (int c=0; c<7; ++c) {
        const int f = rc2x(r,c);
        if (color[f]==to_move) {
          switch (piece[f]) {
            case phoenix: {
              static const int steps[8] = {+11, +11+1, +1, -11+1, -11, -11-1, -1, +11-1};
              for (int i=0; i<8; ++i)
                if (color[f+steps[i]]==xside)
                  add_move(n, ml, phoenix, f, f+steps[i]);
            } break;
            case falcon: {
              static const int steps[2][7] = {
                  {+11, +11+1, +1, -11+1, -11-1, -1, +11-1},
                  {-11, -11+1, +1, +11+1, +11-1, -1, -11-1}};
              for (int i=0; i<7; ++i)
                if (color[f+steps[to_move!=white][i]]==xside)
                  add_move(n, ml, falcon, f, f+steps[to_move!=white][i]);
            } break;
            case crane: {
              static const int steps[6] = {+11, +11+1, -11+1, -11, -11-1, +11-1};
              for (int i=0; i<6; ++i)
                if (color[f+steps[i]]==xside)
                  add_move(n, ml, crane, f, f+steps[i]);
            } break;
            case pheasant: {
              static const int steps[2][3] = {{+22,-11-1,-11+1},{-22,+11-1,+11+1}};
              for (int i=0; i<3; ++i)
                if (color[f+steps[to_move!=white][i]]==xside)
                  add_move(n, ml, pheasant, f, f+steps[to_move!=white][i]);
            } break;
            case quail_l: {
              static const int steps[2][1] = {{-11-1},{+11+1}};
              for (int i=0; i<1; ++i)
                if (color[f+steps[to_move!=white][i]]==xside)
                  add_move(n, ml, quail_l, f, f+steps[to_move!=white][i]);
              static const int slides[2][2] = {{+11,-11+1}, {-11,+11-1}};
              for (int i=0; i<2; ++i)
                for (int d=1; !(to_move&color[f+d*slides[to_move!=white][i]]); ++d)
                  if (!add_move(n, ml, quail_l, f, f+d*slides[to_move!=white][i]))
                    break;
            } break;
            case quail_r: {
              static const int steps[2][1] = {{-11+1},{+11-1}};
              for (int i=0; i<1; ++i)
                if (color[f+steps[to_move!=white][i]]==xside)
                  add_move(n, ml, quail_r, f, f+steps[to_move!=white][i]);
              static const int slides[2][2] = {{+11,-11-1}, {-11,+11+1}};
              for (int i=0; i<2; ++i)
                for (int d=1; !(to_move&color[f+d*slides[to_move!=white][i]]); ++d)
                  if (!add_move(n, ml, quail_r, f, f+d*slides[to_move!=white][i]))
                    break;
            } break;
            case swallow: {
              static const int steps[2][1] = {{+11},{-11}};
              for (int i=0; i<1; ++i)
                if (color[f+steps[to_move!=white][i]]==xside)
                  add_move(n, ml, swallow, f, f+steps[to_move!=white][i]);
            } break;
            case eagle: {
              // TODO: 2-step jump is actually slide...
              static const int steps[2][7] = {{+11,-1,+1,-11-1,-11+1,-22-2,-22+2},{-11,-1,+1,+11-1,+11+1,+22-2,+22+2}};
              for (int i=0; i<1; ++i)
                if (color[f+steps[to_move!=white][i]]==xside)
                  add_move(n, ml, eagle, f, f+steps[to_move!=white][i]);
              static const int slides[2][3] = {{+11-1,+11+1,-11}, {-11-1,-11+1,+11}};
              for (int i=0; i<3; ++i)
                for (int d=1; !(to_move&color[f+d*slides[to_move!=white][i]]); ++d)
                  if (!add_move(n, ml, eagle, f, f+d*slides[to_move!=white][i]))
                    break;
            } break;
            case goose: {
              static const int steps[2][3] = {{+22-2,+22+2,-22},{-22-2,-22+2,+22}};
              for (int i=0; i<3; ++i)
                if (color[f+steps[to_move!=white][i]]==xside)
                  add_move(n, ml, goose, f, f+steps[to_move!=white][i]);
            } break;
            default: break;
          }
        }
      }
    }
    return n;
  }

  int make_move(movet m) {
    if (m.d) {
      color[m.t] = to_move;
      piece[m.t] = (piecet)m.p;
      --hand[to_move][m.p];
    } else if (m.c) {
      ++hand[to_move][unpromotion[m.q]];
      color[m.t] = to_move;
      piece[m.t] = (piecet)m.p;
      color[m.f] = none;
      piece[m.f] = empty;
    } else {
      color[m.t] = to_move;
      piece[m.t] = (piecet)m.p;
      color[m.f] = none;
      piece[m.f] = empty;
    }
    if (m.x)
      piece[m.t] = promotion[m.p];

    // check for "check-mate"
    if (m.c && m.q==phoenix) {
      is_terminal = 1;
      result = to_move;
    }

    to_move = (colort)(to_move ^ 3);
    move_stack[num_moves++] = m;
    hash_stack[num_moves] = compute_hash();

    // check for illegal repetition
    {
      int cnt = 0;
      for (int k=num_moves-1; k>=0 && cnt<3; --k)
        if (hash_stack[k]==hash_stack[num_moves])
          ++cnt;
      if (cnt>=3) {
        // mover did 3rd repetition = loss (千日手 sennichite rule)
        is_terminal = 1;
        result = to_move;
      }
    }
    return 0;
  }

  int unmake_move() {
    movet m = move_stack[--num_moves];
    to_move = (colort)(to_move ^ 3);
    is_terminal = 0;
    result = none;
    if (m.d) {
      color[m.t] = none;
      piece[m.t] = empty;
      ++hand[to_move][m.p];
    } else if (m.c) {
      color[m.f] = to_move;
      piece[m.f] = (piecet)m.p;
      color[m.t] = (colort)(to_move^3);
      piece[m.t] = (piecet)m.q;
      --hand[to_move][unpromotion[m.q]];
    } else {
      color[m.f] = to_move;
      piece[m.f] = (piecet)m.p;
      color[m.t] = none;
      piece[m.t] = empty;
    }
    return 0;
  }

  int showf(FILE* ff) const {
    int n = 0;
    n += fprintf(ff, "       7       6       5       4       3       2       1    \n");
    n += fprintf(ff, "   +-------+-------+-------+-------+-------+-------+-------+\n");
    for (int r=6; r>=0; --r) {
      n += fprintf(ff, " %c |", '1'+r);
      for (int c=0; c<7; ++c) {
        const int x = rc2x(r,c);
        if (color[x]==white) {
          n += fprintf(ff, " w%-5s ", piece_kanji[piece[x]]);
        } else if (color[x]==black) {
          n += fprintf(ff, "\x1b[33m b%-5s \x1b[0m", piece_kanji[piece[x]]);
        } else {
          n += fprintf(ff, "       ");
        }
        n += fprintf(ff, "|");
      }
      n += fprintf(ff, " %s  ", number_kanji[6-r]);
      if (r==6) n += fprintf(ff, "[%016llX][%016llX]", hash_stack[num_moves], compute_hash());
      if (r==5) {
        n += fprintf(ff, "(%s) ", color_kanji[black]);
        for (int i=0; i<NPIECE; ++i)
          for (int j=0; j<hand[black][i]; ++j)
            n += fprintf(ff, " %s", piece_kanji[i]);
      }
      if (r==4) {
        n += fprintf(ff, "(%s) ", color_kanji[white]);
        for (int i=0; i<NPIECE; ++i)
          for (int j=0; j<hand[white][i]; ++j)
            n += fprintf(ff, " %s", piece_kanji[i]);
      }
      if (r==1) n += fprintf(ff, "# %i", num_moves);
      if (r==0) {
        if (is_terminal)
          n += fprintf(ff, "! %s wins !", color_kanji[result]);
        else
          n += fprintf(ff, "%s to move", color_kanji[to_move]);
      }
      n += fprintf(ff, "\n");
      n += fprintf(ff, "   +-------+-------+-------+-------+-------+-------+-------+\n");
    }
    n += fprintf(ff, "       A       B       C       D       E       F       G    \n");
    return n;
  }
};
const piecet board::init_piece[11*11] = {
  block,  block,  block,   block,    block,   block,   block,   block,    block,   block,  block, 
  block,  block,  block,   block,    block,   block,   block,   block,    block,   block,  block, 
  block,  block,  quail_l, pheasant, crane,   phoenix, crane,   pheasant, quail_r, block,  block, 
  block,  block,  empty,   empty,    empty,   falcon,  empty,   empty,    empty,   block,  block, 
  block,  block,  swallow, swallow,  swallow, swallow, swallow, swallow,  swallow, block,  block, 
  block,  block,  empty,   empty,    swallow, empty,   swallow, empty,    empty,   block,  block, 
  block,  block,  swallow, swallow,  swallow, swallow, swallow, swallow,  swallow, block,  block, 
  block,  block,  empty,   empty,    empty,   falcon,  empty,   empty,    empty,   block,  block, 
  block,  block,  quail_r, pheasant, crane,   phoenix, crane,   pheasant, quail_l, block,  block, 
  block,  block,  block,   block,    block,   block,   block,   block,    block,   block,  block, 
  block,  block,  block,   block,    block,   block,   block,   block,    block,   block,  block, 
};
const colort board::init_color[11*11] = {
  border, border, border,  border,   border,  border,  border,  border,   border,  border, border,
  border, border, border,  border,   border,  border,  border,  border,   border,  border, border,
  border, border, white,   white,    white,   white,   white,   white,    white,   border, border,
  border, border, none,    none,     none,    white,   none,    none,     none,    border, border,
  border, border, white,   white,    white,   white,   white,   white,    white,   border, border,
  border, border, none,    none,     black,   none,    white,   none,     none,    border, border,
  border, border, black,   black,    black,   black,   black,   black,    black,   border, border,
  border, border, none,    none,     none,    black,   none,    none,     none,    border, border,
  border, border, black,   black,    black,   black,   black,   black,    black,   border, border,
  border, border, border,  border,   border,  border,  border,  border,   border,  border, border,
  border, border, border,  border,   border,  border,  border,  border,   border,  border, border,
};




// TODO:
//  - check, check-mate
//  - stalemate
//  - repetition checks
// 

// NB: up to 112 moves seen empirically (random games)


// right now perft runs at 2.5MM/sec on crappy work laptop (2.53Ghz)
static int xmaxn = 0;
static unsigned long long perft(board* b, int depth)
{
  if (depth<=0) return 1;
  if (b->is_terminal) return 1;
  unsigned long long cnt = 0;
  movet ml[256];
  int n = b->gen_moves(ml);
  if (n>xmaxn) xmaxn=n;
  for (int i=0; i<n; ++i) {
    b->make_move(ml[i]);
    cnt += perft(b, depth-1);
    b->unmake_move();
  }
  return cnt;
}

int main(int argc, char* argv[])
{
  srand48(-13);
  board   b;
  int maxn = 0, n;
  movet ml[256];

  for (int d=0; d<=8; ++d) {
    board bx;
    unsigned long long c = perft(&bx, d);
    printf("%2i %10llu  %i\n", d, c, xmaxn); fflush(stdout);
  }
  return 0;

  for (int gg=0; gg<1000; ++gg) {
    printf("============================================================\n");
    printf("============================================================\n");
    printf("============================================================\n");
    for (int mm=0; mm<250 && !b.is_terminal; ++mm) {
      b.showf(stdout);
      n = b.gen_moves(ml);
      for (int i=0; i<n; ++i) {
        int k = ml[i].showf(stdout);
        printf("%*s", 20-k, "");
        if (i%6==5) printf("\n");
      }
      if (n%6) printf("\n");
      int k = lrand48() % n;
      printf("*** "); ml[k].showf(stdout); printf(" ***\n");
      b.make_move(ml[k]);
      if (n>maxn) maxn=n;
    }
    printf("============================================================\n");
    printf("MAXIMUM # OF MOVES: %i\n", maxn);
    b.showf(stdout);
    printf("============================================================\n");
    while (b.num_moves)
      b.unmake_move();
    b.showf(stdout);
  }

  return 0;
}
