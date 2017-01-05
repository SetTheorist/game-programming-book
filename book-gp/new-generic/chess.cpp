#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "evalt.h"
#include "gen-search.h"
#include "player.h"
#include "util.h"

namespace chess {

enum color {
  none, white, black,
  NCOLOR
};
enum piece {
  empty, pawn, knight, bishop, rook, queen, king,
  NPIECE
};
struct move {
  union {
    int i;
    struct {
      unsigned w  : 1;  //  0       white to-move?
      unsigned p  : 3;  //  1-- 3   piece moving
      unsigned f  : 7;  //  4--10   from square
      unsigned t  : 7;  // 11--17   to square
      unsigned c  : 1;  // 18       capture?
      unsigned cp : 3;  // 19--21   captured piece
      unsigned pp : 3;  // 22--24   promote-to piece
      unsigned p2 : 1;  // 25       pawn double-move?
      unsigned ep : 1;  // 26       en-passant capture?
      unsigned k  : 1;  // 27       castling?
      unsigned __ : 4;  // 28--31   (unused)
    };
  };
  move(int i_=0) : i(i_) { }
  int showf(FILE* ff) const {
    int n = 0;
    n += fprintf(ff, "%c%c%c%c", w?'w':'b', " PNBRQK"[p], 'a'+(f&0x7), '1'+(f>>4));
    if (c) n+= fprintf(ff, "x%c", " PNBRQK"[cp]);
    else   n+= fprintf(ff, "-");
    n += fprintf(ff, "%c%c", 'a'+(t&0x7), '1'+(t>>4));
    if (k) n+= fprintf(ff, "OO");
    if (ep) n+= fprintf(ff, "ep");
    if (pp) n+= fprintf(ff, "=%c", " PNBRQK"[pp]);
    return n;
  }
  int valid() const { 
    if ((f&0x88) || (f&0x88) || p>king || cp>king || (!p && f!=t) || (c && !cp) || pp>king)
      return 0;
    return 1;
  }
  int null() const { return f==t; }
  static const move nullmove;
};
const move move::nullmove(0);
struct squaret {
  color c; 
  piece p; 
};
struct flagt {
  unsigned char ep[2];
  unsigned char moved[2];
};
struct board {
  enum { MAXNMOVES = 64 };
  static int      dirs[8];
  static int      knight_dirs[8];
  static squaret  init_squares[128];

  squaret square[128];

  color   to_move;
  int     is_terminal;
  color   result;
  flagt   flags;

  int     num_moves;
  move    move_stack[256];
  hasht   hash_stack[256];
  flagt   flags_stack[256];

  hasht   square_hash[128][NCOLOR][NPIECE];
  hasht   to_move_hash[NCOLOR];
  hasht   ep_hash[2][128];

  board() {
    init_hash();
    init();
  }

  void init() {
    memcpy(square, init_squares, sizeof(square));
    to_move = white;
    is_terminal = 0;
    num_moves = 0;
    hash_stack[0] = compute_hash();
    flags.ep[0] = flags.ep[1] = 0;
    flags.moved[0] = flags.moved[1] = 0;
  }

  void init_hash() {
    for (int i=0; i<128; ++i)
      for (int j=0; j<NCOLOR; ++j)
        for (int k=0; k<NPIECE; ++k)
          square_hash[i][j][k] = gen_hash();
    for (int i=0; i<NCOLOR; ++i)
      to_move_hash[i] = gen_hash();
    for (int i=0; i<2; ++i)
      for (int j=0; j<128; ++j)
        ep_hash[i][j] = gen_hash();
  }

  hasht compute_hash() const {
    hasht h = to_move_hash[to_move];
    for (int i=0; i<8*16; i+=16)
      for (int j=0; j<8; ++j)
        h ^= square_hash[i+j][square[i+j].c][square[i+j].p];
    h ^= ep_hash[0][flags.ep[0]] ^ ep_hash[1][flags.ep[1]];
    return h;
  }

  int terminal() const { return is_terminal; }

  hasht hash() const { return hash_stack[num_moves]; }

  move last_move() const { return move_stack[num_moves-1]; }

  int in_check(color) const { return 0; }

  make_move_flags make_move(move m) {
    if (!m.valid()) {
      fprintf(stderr, "!!!! w=%i p=%i f=%i t=%i c=%i cp=%i pp=%i p2=%i ep=%i k=%i __=%i !!!!\n",
        m.w, m.p, m.f, m.t, m.c, m.cp, m.pp, m.p2, m.ep, m.k, m.__);
      return invalid_move;
    }
    flags_stack[num_moves] = flags;
    hasht h = hash_stack[num_moves];

    if (m.f != m.t)
    {
      const squaret empty_square = {none, empty};
      //TODO: handle castling moves

      {
        h ^= square_hash[m.f][none][empty]
           ^ square_hash[m.f][square[m.f].c][square[m.f].p]
           ^ square_hash[m.t][square[m.f].c][m.pp ? m.pp : square[m.f].p]
           ^ square_hash[m.t][square[m.t].c][square[m.t].p];
        h ^= ep_hash[to_move==white][flags.ep[to_move==white]]
           ^ ep_hash[to_move==white][m.p2 ? (1 + (m.t+m.f)/2) : 0];
      }
      square[m.t] = square[m.f];
      if (m.pp) square[m.t].p = (piece)m.pp;
      square[m.f] = empty_square;

      flags.ep[to_move==white] = m.p2 ? (1 + (m.t+m.f)/2) : 0;
      if (to_move==white && (m.f>>4)==0) flags.moved[1] |= (1<<(m.f&7));
      if (to_move==white && (m.t>>4)==0) flags.moved[1] |= (1<<(m.t&7));
      if (to_move==black && (m.f>>4)==7) flags.moved[0] |= (1<<(m.f&7));
      if (to_move==black && (m.t>>4)==7) flags.moved[0] |= (1<<(m.t&7));
    }

    move_stack[num_moves++] = m;
    to_move = (color)(to_move ^ 3);
    h ^= to_move_hash[white]^to_move_hash[black];
    //hash_stack[num_moves] = compute_hash();
    hash_stack[num_moves] = h;
    if (0){
      if (h != compute_hash()) {
        printf("================================\n");
        printf("================================\n");
        printf("================================\n");
        printf("================================\n");
        showf(stdout);
        printf("================================\n");
        fstatus<move,i_evalt>(stdout, "%M\n", move_stack, num_moves);
        printf("================================\n");
        printf("================================\n");
        printf("================================\n");
      }
    }

    if (m.c && m.cp==king) {
      is_terminal = 1;
      result = (color)(to_move ^ 3);
    } else {
      // check for repetition
      for (int k=num_moves-1; k>=0; --k) {
        if (hash_stack[k] == hash_stack[num_moves]) {
          is_terminal = 1;
          result = none;
          break;
        }
      }
    }
    return ok_move;
  }

  make_move_flags unmake_move() {
    if (!num_moves) return invalid_move;
    move m = move_stack[--num_moves];
    flags = flags_stack[num_moves];

    if (m.f != m.t)
    {
      const squaret empty_square = {none, empty};
      //TODO: handle castling moves

      square[m.f] = square[m.t];
      if (m.pp) square[m.f].p = pawn;
      square[m.t] = empty_square;

      if (m.c) {
        if (m.ep) {
          square[m.t + ((to_move==black)?-16:+16)].c = to_move;
          square[m.t + ((to_move==black)?-16:+16)].p = (piece)m.cp;
        } else {
          square[m.t].c = to_move;
          square[m.t].p = (piece)m.cp;
        }
      }
    }

    is_terminal = 0;
    to_move = (color)(to_move ^ 3);
    if (0){
      if (hash_stack[num_moves] != compute_hash()) {
        printf("--------------------------------\n");
        printf("--------------------------------\n");
        printf("--------------------------------\n");
        printf("--------------------------------\n");
        showf(stdout);
        printf("--------------------------------\n");
        fstatus<move,i_evalt>(stdout, "%M [%m]\n", move_stack, num_moves, m);
        printf("--------------------------------\n");
        printf("--------------------------------\n");
        printf("--------------------------------\n");
      }
    }
    return ok_move;
  }

#define ADD_MOVE(ff,tt,pc) \
  do { \
    if (square[(tt)].c==to_move) break; \
    ml[n].i = 0; \
    ml[n].w = (to_move == white); \
    ml[n].f = (ff); \
    ml[n].t = (tt); \
    ml[n].p = (pc); \
    if (square[(tt)].c!=none) { \
      ml[n].c = 1; \
      ml[n].cp = square[(tt)].p; \
    } \
    if ((pc)==pawn && (((tt)>>4)==0 || ((tt)>>4)==7)) ml[n].pp = queen; \
    ++n; \
  } while (0)
  int gen_moves(move* ml) const {
    int n = 0;
    for (int j=0; j<8*16; j+=16) {
      for (int i=0; i<8; ++i) {
        int f = i+j;
        if (square[f].c == to_move) {
          switch (square[f].p) {
            case pawn:
              if (to_move==white) {
                int t;
                t = f + 16; if (!(t&0x88) && square[t].c==none ) ADD_MOVE(f,t,pawn);
                t = f + 15; if (!(t&0x88) && square[t].c==black) ADD_MOVE(f,t,pawn);
                if (flags.ep[0] && (t==flags.ep[0]-1)) { ADD_MOVE(f,t-16,pawn); ml[n-1].t = t; ml[n-1].ep = 1; }
                t = f + 17; if (!(t&0x88) && square[t].c==black) ADD_MOVE(f,t,pawn);
                if (flags.ep[0] && (t==flags.ep[0]-1)) { ADD_MOVE(f,t-16,pawn); ml[n-1].t = t; ml[n-1].ep = 1; }
                if (f<2*16 && square[f+16].c==none && square[f+32].c==none) {
                  ADD_MOVE(f,f+32,pawn);
                  ml[n-1].p2 = 1;
                }
              } else {
                int t;
                t = f - 16; if (!(t&0x88) && square[t].c==none ) ADD_MOVE(f,t,pawn);
                t = f - 15; if (!(t&0x88) && square[t].c==white) ADD_MOVE(f,t,pawn);
                if (flags.ep[1] && (t==flags.ep[1]-1)) { ADD_MOVE(f,t+16,pawn); ml[n-1].t = t; ml[n-1].ep = 1; }
                t = f - 17; if (!(t&0x88) && square[t].c==white) ADD_MOVE(f,t,pawn);
                if (flags.ep[1] && (t==flags.ep[1]-1)) { ADD_MOVE(f,t+16,pawn); ml[n-1].t = t; ml[n-1].ep = 1; }
                if (f>=6*16 && square[f-16].c==none && square[f-32].c==none) {
                  ADD_MOVE(f,f-32,pawn);
                  ml[n-1].p2 = 1;
                }
              }
              break;
            case knight:
              for (int k=0; k<8; ++k) {
                int t = f + knight_dirs[k];
                if (!(t&0x88))
                  ADD_MOVE(f,t,knight);
              }
              break;
            case bishop:
              for (int k=1; k<8; k+=2) {
                int d = dirs[k];
                for (int t=f+d; !(t&0x88); t+=d) {
                  ADD_MOVE(f,t,bishop);
                  if (square[t].c!=none) break;
                }
              }
              break;
            case rook:
              for (int k=0; k<8; k+=2) {
                int d = dirs[k];
                for (int t=f+d; !(t&0x88); t+=d) {
                  ADD_MOVE(f,t,rook);
                  if (square[t].c!=none) break;
                }
              }
              break;
            case queen:
              for (int k=0; k<8; ++k) {
                int d = dirs[k];
                for (int t=f+d; !(t&0x88); t+=d) {
                  ADD_MOVE(f,t,queen);
                  if (square[t].c!=none) break;
                }
              }
              break;
            case king:
              for (int k=0; k<8; ++k) {
                int t = f + dirs[k];
                if (!(t&0x88))
                  ADD_MOVE(f,t,king);
              }
              if (to_move==white) {
                // TODO: forbid castling in check, etc.
                if (!(flags.moved[1]&0x90)) {
                  if ((square[4].p==king && square[4].c==white)
                      && (square[7].p==rook && square[7].c==white)
                      && square[5].c==none && square[6].c==none) {
                    ADD_MOVE(4,6,king);
                    ml[n-1].k = 1;
                  }
                }
                if (!(flags.moved[1]&0x11)) {
                  if ((square[4].p==king && square[4].c==white)
                      && (square[0].p==rook && square[0].c==white)
                      && square[1].c==none && square[2].c==none && square[3].c==none) {
                    ADD_MOVE(4,2,king);
                    ml[n-1].k = 1;
                  }
                }
              } else {
                // TODO: forbid castling in check, etc.
                if (!(flags.moved[0]&0x90)) {
                  if ((square[7*16+4].p==king && square[7*16+4].c==black)
                      && (square[7*16+7].p==rook && square[7*16+7].c==black)
                      && square[7*16+5].c==none && square[7*16+6].c==none) {
                    ADD_MOVE(7*16+4,7*16+6,king);
                    ml[n-1].k = 1;
                  }
                }
                if (!(flags.moved[0]&0x11)) {
                  if ((square[7*16+4].p==king && square[7*16+4].c==black)
                      && (square[7*16+0].p==rook && square[7*16+0].c==black)
                      && square[7*16+1].c==none && square[7*16+2].c==none && square[7*16+3].c==none) {
                    ADD_MOVE(7*16+4,7*16+2,king);
                    ml[n-1].k = 1;
                  }
                }
              }
              break;
            default:
              break;
          }
        }
      }
    }
    return n;
  }
  // add all _captures_
  int gen_moves_quiescence(move* ml) const {
    int n = 0;
    color xside = (color)(to_move ^ 3);
    for (int j=0; j<8*16; j+=16) {
      for (int i=0; i<8; ++i) {
        int f = i+j;
        if (square[f].c == to_move) {
          switch (square[f].p) {
            case pawn:
              if (to_move==white) {
                int t;
                t = f + 15; if (!(t&0x88) && square[t].c==xside) ADD_MOVE(f,t,pawn);
                if (flags.ep[0] && (t==flags.ep[0]-1)) { ADD_MOVE(f,t-16,pawn); ml[n-1].t = t; ml[n-1].ep = 1; }
                t = f + 17; if (!(t&0x88) && square[t].c==xside) ADD_MOVE(f,t,pawn);
                if (flags.ep[0] && (t==flags.ep[0]-1)) { ADD_MOVE(f,t-16,pawn); ml[n-1].t = t; ml[n-1].ep = 1; }
              } else {
                int t;
                t = f - 15; if (!(t&0x88) && square[t].c==xside) ADD_MOVE(f,t,pawn);
                if (flags.ep[1] && (t==flags.ep[1]-1)) { ADD_MOVE(f,t+16,pawn); ml[n-1].t = t; ml[n-1].ep = 1; }
                t = f - 17; if (!(t&0x88) && square[t].c==xside) ADD_MOVE(f,t,pawn);
                if (flags.ep[1] && (t==flags.ep[1]-1)) { ADD_MOVE(f,t+16,pawn); ml[n-1].t = t; ml[n-1].ep = 1; }
              }
              break;
            case knight:
              for (int k=0; k<8; ++k) {
                int t = f + knight_dirs[k];
                if (!(t&0x88) && square[t].c==xside)
                  ADD_MOVE(f,t,knight);
              }
              break;
            case bishop:
              for (int k=1; k<8; k+=2) {
                int d = dirs[k];
                for (int t=f+d; !(t&0x88); t+=d) {
                  if (square[t].c==xside)
                    ADD_MOVE(f,t,bishop);
                  if (square[t].c!=none) break;
                }
              }
              break;
            case rook:
              for (int k=0; k<8; k+=2) {
                int d = dirs[k];
                for (int t=f+d; !(t&0x88); t+=d) {
                  if (square[t].c==xside)
                    ADD_MOVE(f,t,rook);
                  if (square[t].c!=none) break;
                }
              }
              break;
            case queen:
              for (int k=0; k<8; ++k) {
                int d = dirs[k];
                for (int t=f+d; !(t&0x88); t+=d) {
                  if (square[t].c==xside)
                    ADD_MOVE(f,t,queen);
                  if (square[t].c!=none) break;
                }
              }
              break;
            case king:
              for (int k=0; k<8; ++k) {
                int t = f + dirs[k];
                if (!(t&0x88))
                  if (square[t].c==xside)
                    ADD_MOVE(f,t,king);
              }
              break;
            default:
              break;
          }
        }
      }
    }
    return n;
  }
  int showf(FILE* ff) const {
    int n = 0;
    n += fprintf(ff, "   +----+----+----+----+----+----+----+----+\n");
    for (int r=7; r>=0; --r) {
      n += fprintf(ff, " %c |", '1'+r);
      for (int c=0; c<8; ++c) {
        n += fprintf(ff, " %c%c ",
            " wb"[square[r*16+c].c],
            " PNBRQK"[square[r*16+c].p] );
        n += fprintf(ff, c==7 ? "|" : ":");
      }
      if (r==7) n+=fprintf(ff, "  [%016llX]", hash());
      if (r==6) n+=fprintf(ff, "  [%016llX]", compute_hash());
      if (r==4) n+=fprintf(ff, "  (%02x %02x)", flags.ep[0], flags.ep[1]);
      if (r==3) n+=fprintf(ff, "  (%02x %02x)", flags.moved[0], flags.moved[1]);
      n += fprintf(ff, "\n");
    }
    n += fprintf(ff, "   +----+----+----+----+----+----+----+----+\n");
    n += fprintf(ff, "     a    b    c    d    e    f    g    h\n");
    if (is_terminal) n += fprintf(ff, "*** result = %s ***\n",
        result==none?"draw":result==white?"white":result==black?"black":"???");
    else
      n += fprintf(ff, "%s to move [%i]\n",
        to_move==none?"nobody":to_move==white?"white":to_move==black?"black":"???",
        num_moves);
    return n;
  }
};
int board::dirs[8] = {+16, +17, +1, -15, -16, -17, -1, +15};
int board::knight_dirs[8] = {+33, +18, -14, -31, -33, -18, +14, +31};
#define w white
#define b black
#define P pawn
#define N knight
#define B bishop
#define R rook
#define Q queen
#define K king
#define xxxxx {none,empty}
#define xxx xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx
squaret board::init_squares[128] = {
  {w,R},{w,N},{w,B},{w,Q},{w,K},{w,B},{w,N},{w,R},xxx,
  {w,P},{w,P},{w,P},{w,P},{w,P},{w,P},{w,P},{w,P},xxx,
  xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxx,
  xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxx,
  xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxx,
  xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxxxx,xxx,
  {b,P},{b,P},{b,P},{b,P},{b,P},{b,P},{b,P},{b,P},xxx,
  {b,R},{b,N},{b,B},{b,Q},{b,K},{b,B},{b,N},{b,R},xxx,
};
#undef xxx
#undef x
#undef K
#undef Q
#undef R
#undef B
#undef N
#undef P
#undef b
#undef w

struct evaluator {
  static int piece_vals[NPIECE];

  void evaluate_moves_for_search(board* b, move* ml, i_evalt* scorel, int n) const {
    for (int i=0; i<n; ++i)
      scorel[i] = i;
  }
  i_evalt evaluate_relative(const board* b, int ply, i_evalt alpha, i_evalt beta) const {
    if (b->terminal()) {
      if (!b->result) return i_evalt::STALEMATE_RELATIVE_IN_N(ply);
      if (b->result == b->to_move)
        return i_evalt::W_WIN_IN_N(ply);
      else
        return i_evalt::B_WIN_IN_N(ply);
    }
    int total = b->hash()%11-5;
    for (int r=0; r<8*16; r+=16)
      for (int c=0; c<8; ++c)
          if (b->square[r+c].c != none)
            total += piece_vals[b->square[r+c].p]*(b->square[r+c].c==b->to_move ? +1 : -1);
    i_evalt v = {total};
    return v;
  }
  int global_search_extensions(
      const board* b, int ply, i_evalt alpha, i_evalt beta, node_type type, int is_quiescence) const
  {
    return 0;
  }
  int local_search_extensions(
      const board* b, int ply, i_evalt alpha, i_evalt beta, node_type type, int is_quiescence) const
  {
    return 0;
  }
  // e.g. not if in check
  int allow_stand_pat_quiescence(const board* b) const {
    return 1;
  }
};
int evaluator::piece_vals[NPIECE] = {
  0, 100, 250, 350, 500, 1000, 1000000
};

struct game {
  typedef move movet;
  typedef board boardt;
  typedef i_evalt evalt;
  typedef color colort;

  static const colort white=chess::white;
  static const colort black=chess::black;
  static const colort none=chess::none;
};

} // namespace chess

unsigned long long perft(chess::board& b, int depth)
{
  if (depth<=0 || b.terminal()) return 1;
  unsigned long long count = 0;
  chess::move  ml[64];
  int n = b.gen_moves(ml);
  for (int i=0; i<n; ++i) {
    b.make_move(ml[i]);
    count += perft(b, depth-1);
    b.unmake_move();
  }
  return count;
}

int main(int argc, char* argv[]) {
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);
  setlocale(LC_ALL,"");

  srand48(argc>1 ? atoi(argv[1]) : -17);
  if (0) {
    chess::board b;
    b.showf(stdout);
    for (int d=0; d<6; ++d)
      printf("%i %12lli\n", d, perft(b, d));
  }
  if (1) {
    RandomPlayer<chess::game>       rp;
    InteractivePlayer<chess::game>  ip;
    GameRecord<chess::game>         gr;
    gr.play(rp, rp);
    gr.showf(stdout);
    return 0;
  }
  if (1) {
    int results[3] = {0,0,0};
    for (int gg=0; gg<100; ++gg)
    {
      chess::move  pv[64];
      int       pv_length = 0;
      chess::board b;
      chess::evaluator e;
      searcher<chess::game> ss_w(400, 1024*256, 1024*64);
      ss_w.settings.qdepth = 200;
      searcher<chess::game> ss_b(400, 1024*256, 1024*64);
      ss_b.settings.qdepth = 600;
      for (int i=0; !b.terminal() && i<250; ++i)
      {
        b.showf(stdout); fflush(stdout);

        int n;
        chess::move  ml[64];
        n = b.gen_moves(ml);
        fstatus<chess::move,i_evalt>(stdout, "Moves: %M\n", ml, n);
        n = b.gen_moves_quiescence(ml);
        fstatus<chess::move,i_evalt>(stdout, "QMoves: %M\n", ml, n);
        fflush(stdout);

        i_evalt res = (b.to_move==chess::game::white ? ss_w : ss_b).search(&b, &e, pv, &pv_length);
        chess::move m = pv[0];
        fstatus<chess::move,i_evalt>(stdout, "Search [ %m ] eval=%q pv=%M\n", m, res, pv, pv_length);

        b.make_move(m);
      }
      b.showf(stdout); fflush(stdout);
      ++results[b.result==chess::game::white?0 : b.result==chess::game::black?1 : 2];
      printf("\n\n***** RESULTS w:%i b:%i 0:%i*****\n\n", results[0], results[1], results[2]);
    }
    printf("\n\n***** RESULTS w:%i b:%i 0:%i*****\n\n", results[0], results[1], results[2]);
  }
  if (0) {
    chess::board b;
    b.showf(stdout);
    for (int i=0; !b.terminal() && i<100; ++i) {
      int n;
      chess::move  ml[64];
      n = b.gen_moves(ml);
      fstatus<chess::move,i_evalt>(stdout, "Moves: %M\n", ml, n);
      chess::move m = ml[lrand48() % n];
      fstatus<chess::move,i_evalt>(stdout, ">>> %m <<<\n", m);
      b.make_move(m);
      b.showf(stdout);
    }
    printf("------------------------------------------------------------\n");
    b.showf(stdout);
    while (b.num_moves) {
      fstatus<chess::move,i_evalt>(stdout, "<<< %m >>>\n", b.move_stack[b.num_moves-1]);
      b.unmake_move();
      b.showf(stdout);
    }
  }
  return 0;
}
