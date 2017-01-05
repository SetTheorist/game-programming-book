#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "gen-search.h"
#include "evalt.h"

/* ************************************************************ */
/* ************************************************************ */

/* ****************************************
 * Congo rewrite
 * **************************************** */

enum cpiecet
{
  lion, elephant, zebra, monkey, crocodile, pawn, superpawn, giraffe, empty,
  NPIECES
};
static const char piece_abbr[] = "LEZMCPSG=";
enum ccolort
{
  none=0, white=1, black=2,
  NCOLORS
};
enum csquaret
{
  normal, river, den
};
struct cmovet
{
  union {
    int i;
    struct {
      unsigned w  : 1;  //  1    white moving?
      unsigned p  : 3;  //  2- 4 piece
      unsigned f  : 6;  //  5-10 from square
      unsigned t  : 6;  // 11-16 to square
      unsigned fp : 1;  // 17    promote?
      unsigned fm : 1;  // 18    monkeyjump?
      unsigned fc : 1;  // 19    capture?
      unsigned cp : 3;  // 20-22 captured piece
      unsigned dl : 6;  // 23-28 drowned loc + 1 (0=no drown)
      unsigned dp : 3;  // 29-31 drowned piece
      unsigned dc : 1;  // 32    drowned color white?
    };
  };
  cmovet(int ii=0) : i(ii) { }
  bool valid() { return true; }
  bool null() { return f==t; }
  static const cmovet nullmove;
  int showf(FILE* ff) const {
    if (f==t) {
      return fprintf(ff, "<pass>");
    } else {
      int tot = 0;
      tot += fprintf(ff, "%c", piece_abbr[p]);
      tot += fprintf(ff, "%c%c", 'a'+(f%7), '1'+(f/7));
      if (fc)
        tot += fprintf(ff, "*%c", piece_abbr[cp]);
      else
        tot += fprintf(ff, "%c", '-');
      tot += fprintf(ff, "%c%c", 'a'+(t%7), '1'+(t/7));
      if (fp) tot += fprintf(ff, "+");
      if (dl) { tot += fprintf(ff, "@%c%c%c", piece_abbr[dp], 'a'+((dl-1)%7), '1'+((dl-1)/7)); }
      if (fm) tot += fprintf(ff, "..");
      return tot;
    }
  }
};
const cmovet cmovet::nullmove(0);
struct cboardt
{
  static const int MAXNMOVES = 64;
  static const cpiecet init_pieces[7*7];
  static const ccolort init_colors[7*7];
  static const csquaret init_squares[7*7];

  cpiecet pieces[7*7];
  ccolort colors[7*7];

  ccolort to_move;
  int     is_terminal;
  ccolort result;
  int     monkey_mover[NCOLORS];

  int     num_moves;
  cmovet  move_stack[256];
  hasht   hash_stack[256];
  int     monkey_stack[256][NCOLORS];

  hasht piece_hash[7*7][NCOLORS][NPIECES];
  hasht monkey_hash[7*7+1];
  hasht to_move_hash[NCOLORS];

  cboardt() {
    init();
    init_hash();
    hash_stack[num_moves] = compute_hash();
  }
  void init_hash() {
    for (int i=0; i<7*7; ++i)
      for (int j=0; j<NCOLORS; ++j)
        for (int k=0; k<NPIECES; ++k)
          piece_hash[i][j][k] = gen_hash();
    for (int i=0; i<7*7+1; ++i)
      monkey_hash[i] = gen_hash();
    for (int i=0; i<NCOLORS; ++i)
      to_move_hash[i] = gen_hash();
  }
  void init() {
    memset(this, '\x00', sizeof(*this));
    memcpy(pieces, init_pieces, sizeof(pieces));
    memcpy(colors, init_colors, sizeof(colors));
    to_move = white;
    num_moves = 0;
    result = none;
    is_terminal = 0;
    memset(monkey_mover, '\x00', sizeof(monkey_mover));
  }
  int in_check(ccolort) { return false; }
#define IN_RIVER(l) ((l)>=3*7 && (l)<(3*7+7))
#define ADD_MOVE(ff,tt) \
  do { \
    if (colors[(tt)]==to_move) break; \
    ml[n].i = 0; \
    ml[n].dl=drowner; ml[n].dc=drowner_color; ml[n].dp=drowner_piece; \
    if ((ff)==ml[n].dl-1 && !IN_RIVER(tt)) ml[n].dl=0; \
    ml[n].w = to_move==white; \
    ml[n].p = pieces[i]; \
    ml[n].f = (ff); \
    ml[n].t = (tt); \
    ml[n].fp = (pieces[i]==pawn && ((tt)>=6*7 || (tt)<1*7)); \
    if (colors[(tt)]!=none) { \
      ml[n].fc = 1; \
      ml[n].cp = pieces[(tt)]; \
    } \
    ++n; \
  } while (0)
#define ADD_MONKEY_JUMP(ff,tt) \
  do { \
    if (colors[(tt)]!=none) break; \
    if (colors[((ff)+(tt))/2]==none) break; \
    ml[n].i = 0; \
    ml[n].dl=drowner; ml[n].dc=drowner_color; ml[n].dp=drowner_piece; \
    if ((ff)==ml[n].dl-1 && !IN_RIVER(tt)) ml[n].dl=0; \
    ml[n].w = to_move==white; \
    ml[n].p = pieces[i]; \
    ml[n].f = (ff); \
    ml[n].t = (tt); \
    ml[n].fm = 1; \
    if (colors[((ff)+(tt))/2]!=to_move) { \
      ml[n].fc = 1; \
      ml[n].cp = pieces[((ff)+(tt))/2]; \
    } \
    ++n; \
  } while (0)
#define ADD_MOVE_LION(ff,tt) \
  do { \
    if (colors[(tt)]==to_move) break; \
    ml[n].i = 0; \
    if (init_squares[(tt)]!=den) break; \
    ml[n].dl=drowner; ml[n].dc=drowner_color; ml[n].dp=drowner_piece; \
    if ((ff)==ml[n].dl-1 && !IN_RIVER(tt)) ml[n].dl=0; \
    ml[n].w = to_move==white; \
    ml[n].p = pieces[i]; \
    ml[n].f = (ff); \
    ml[n].t = (tt); \
    if (colors[(tt)]!=none) { \
      ml[n].fc = 1; \
      ml[n].cp = pieces[(tt)]; \
    } \
    ++n; \
  } while (0)
#define ADD_MOVE_NOCAP(ff,tt) \
  do { \
    ml[n].i = 0; \
    if (colors[(tt)]!=none) break; \
    ml[n].dl=drowner; ml[n].dc=drowner_color; ml[n].dp=drowner_piece; \
    if ((ff)==ml[n].dl-1 && !IN_RIVER(tt)) ml[n].dl=0; \
    ml[n].w = to_move==white; \
    ml[n].p = pieces[i]; \
    ml[n].f = (ff); \
    ml[n].t = (tt); \
    ml[n].fp = (pieces[i]==pawn && ((tt)>=6*7 || (tt)<1*7)); \
    ++n; \
  } while (0)

  int gen_moves(cmovet ml[]) const {
    if (is_terminal) return 0;
    int n = 0;
    int drowner = 0, drowner_color = 0, drowner_piece = 0;
    for (int i=3*7; i<3*7+7; ++i)
      if (colors[i]==to_move && pieces[i]!=crocodile)
        drowner = i+1;
    if (drowner) { drowner_color=colors[drowner-1]==white; drowner_piece=pieces[drowner-1]; }
    if (monkey_mover[to_move]) {
      int i = monkey_mover[to_move]-1;
      int r = i/7;
      int c = i%7;
      ml[n++] = cmovet::nullmove;
      if (r<5 && pieces[i+7]!=empty) ADD_MONKEY_JUMP(i,i+14);
      if (r>1 && pieces[i-7]!=empty) ADD_MONKEY_JUMP(i,i-14);
      if (c<5 && pieces[i+1]!=empty) ADD_MONKEY_JUMP(i,i+2);
      if (c>1 && pieces[i-1]!=empty) ADD_MONKEY_JUMP(i,i-2);
      if (r<5 && c<5 && pieces[i+7+1]!=empty) ADD_MONKEY_JUMP(i,i+14+2);
      if (r<5 && c>1 && pieces[i+7-1]!=empty) ADD_MONKEY_JUMP(i,i+14-2);
      if (r>1 && c<5 && pieces[i-7+1]!=empty) ADD_MONKEY_JUMP(i,i-14+2);
      if (r>1 && c>1 && pieces[i-7-1]!=empty) ADD_MONKEY_JUMP(i,i-14-2);
    } else {
      int rel = to_move==white ? +1 : -1;
      for (int i=0; i<7*7; ++i) {
        if (colors[i]==to_move) {
          int r = i/7;
          int c = i%7;
          switch (pieces[i]) {
            default: break;
            case lion:
              if (r>0) ADD_MOVE_LION(i,i-7);
              if (r>0 && c>0) ADD_MOVE_LION(i,i-7-1);
              if (r>0 && c<6) ADD_MOVE_LION(i,i-7+1);
              if (r<6) ADD_MOVE_LION(i,i+7);
              if (r<6 && c>0) ADD_MOVE_LION(i,i+7-1);
              if (r<6 && c<6) ADD_MOVE_LION(i,i+7+1);
              if (c>0) ADD_MOVE_LION(i,i-1);
              if (c<6) ADD_MOVE_LION(i,i+1);
              if (colors[i]==white) {
                for (int j=1; r+j<7; ++j)
                  if (pieces[i+7*j]!=empty) {
                    if (pieces[i+7*j]==lion)
                      ADD_MOVE_LION(i,i+7*j);
                    else
                      break;
                  }
              } else {
                for (int j=1; r-j>=0; ++j)
                  if (pieces[i-7*j]!=empty) {
                    if (pieces[i-7*j]==lion)
                      ADD_MOVE_LION(i,i-7*j);
                    else
                      break;
                  }
              }
              break;
            case elephant:
              if (r>0) { ADD_MOVE(i,i-7); if (r>1) ADD_MOVE(i,i-14); }
              if (r<6) { ADD_MOVE(i,i+7); if (r<5) ADD_MOVE(i,i+14); }
              if (c>0) { ADD_MOVE(i,i-1); if (c>1) ADD_MOVE(i,i-2); }
              if (c<6) { ADD_MOVE(i,i+1); if (c<5) ADD_MOVE(i,i+2); }
              break;
            case monkey:
              if (r>0) ADD_MOVE_NOCAP(i,i-7);
              if (r>0 && c>0) ADD_MOVE_NOCAP(i,i-7-1);
              if (r>0 && c<6) ADD_MOVE_NOCAP(i,i-7+1);
              if (r<6) ADD_MOVE_NOCAP(i,i+7);
              if (r<6 && c>0) ADD_MOVE_NOCAP(i,i+7-1);
              if (r<6 && c<6) ADD_MOVE_NOCAP(i,i+7+1);
              if (c>0) ADD_MOVE_NOCAP(i,i-1);
              if (c<6) ADD_MOVE_NOCAP(i,i+1);
              if (r<5 && pieces[i+7]!=empty) ADD_MONKEY_JUMP(i,i+14);
              if (r>1 && pieces[i-7]!=empty) ADD_MONKEY_JUMP(i,i-14);
              if (c<5 && pieces[i+1]!=empty) ADD_MONKEY_JUMP(i,i+14);
              if (c>1 && pieces[i-1]!=empty) ADD_MONKEY_JUMP(i,i-14);
              if (r<5 && c<5 && pieces[i+7+1]!=empty) ADD_MONKEY_JUMP(i,i+14+2);
              if (r<5 && c>1 && pieces[i+7-1]!=empty) ADD_MONKEY_JUMP(i,i+14-2);
              if (r>1 && c<5 && pieces[i-7+1]!=empty) ADD_MONKEY_JUMP(i,i-14+2);
              if (r>1 && c>1 && pieces[i-7-1]!=empty) ADD_MONKEY_JUMP(i,i-14-2);
              break;
            case zebra:
              if (r<5 && c<6) ADD_MOVE(i,i+15);
              if (r<6 && c<5) ADD_MOVE(i,i+9);
              if (r>0 && c<5) ADD_MOVE(i,i-5);
              if (r>1 && c<6) ADD_MOVE(i,i-13);
              if (r>1 && c>0) ADD_MOVE(i,i-15);
              if (r>0 && c>1) ADD_MOVE(i,i-9);
              if (r<6 && c>1) ADD_MOVE(i,i+5);
              if (r<5 && c>0) ADD_MOVE(i,i+13);
              break;
            case crocodile:
              if (r>0) ADD_MOVE(i,i-7);
              if (r>0 && c>0) ADD_MOVE(i,i-7-1);
              if (r>0 && c<6) ADD_MOVE(i,i-7+1);
              if (c>0) ADD_MOVE(i,i-1);
              if (c<6) ADD_MOVE(i,i+1);
              if (r<6) ADD_MOVE(i,i+7);
              if (r<6 && c>0) ADD_MOVE(i,i+7-1);
              if (r<6 && c<6) ADD_MOVE(i,i+7+1);
              // TODO: correct these slides
              if (r==3) {
                for (int j=c+1; j<7; ++j) {
                  ADD_MOVE(i,i+j);
                  if (pieces[i+j]!=empty) break;
                }
                for (int j=c-1; j>=0; --j) {
                  ADD_MOVE(i,i-j);
                  if (pieces[i-j]!=empty) break;
                }
              } else if (r<3) {
                int ok = 1;
                for (int j=r+1; j<3; ++j)
                  ok = ok && (pieces[7*j+c]==empty);
                if (ok) ADD_MOVE(i,3*7+c);
              } else if (r>3) {
                int ok = 1;
                for (int j=r-1; j>3; --j)
                  ok = ok && (pieces[7*j+c]==empty);
                if (ok) ADD_MOVE(i,3*7+c);
              }
              break;
            case giraffe:
              if (r>0) ADD_MOVE_NOCAP(i,i-7);
              if (r>1) ADD_MOVE(i,i-14);
              if (r<5) ADD_MOVE(i,i+14);
              if (r<6) ADD_MOVE_NOCAP(i,i+7);
              if (c>0) ADD_MOVE_NOCAP(i,i-1);
              if (c>1) ADD_MOVE(i,i-2);
              if (c<5) ADD_MOVE(i,i+2);
              if (c<6) ADD_MOVE_NOCAP(i,i+1);
              break;
            case pawn:
              ADD_MOVE(i,i+7*rel);
              if (c>0) ADD_MOVE(i,i+7*rel-1);
              if (c<6) ADD_MOVE(i,i+7*rel+1);
              break;
            case superpawn:
              if (colors[i]==white) {
                if (r<6) ADD_MOVE(i,i+7);
                if (r<6 && c>0) ADD_MOVE(i,i+7-1);
                if (r<6 && c<6) ADD_MOVE(i,i+7+1);
                if (r>0) ADD_MOVE_NOCAP(i,i-7);
                if (r>1) ADD_MOVE_NOCAP(i,i-14);
                // TODO
              } else {
                if (r>0) ADD_MOVE(i,i-7);
                if (r>0 && c>0) ADD_MOVE(i,i-7-1);
                if (r>0 && c<6) ADD_MOVE(i,i-7+1);
                if (r<6) ADD_MOVE_NOCAP(i,i+7);
                if (r<5) ADD_MOVE_NOCAP(i,i+14);
                // TODO
              }
              if (c>0) ADD_MOVE(i,i-1);
              if (c<6) ADD_MOVE(i,i+1);
              break;
          }
        }
      }
    }
    return n;
  }
  // e.g. generate only captures
  // (or all moves, if in check)
  int gen_moves_quiescence(cmovet ml[]) const {
    return 0;
  }
  make_move_flags make_move(cmovet m) {
    if (m.dl) {
      colors[m.dl-1] = none;
      pieces[m.dl-1] = empty;
    }
    if (m.f != m.t) {
      if (m.fc && m.fm) {
        colors[(m.f+m.t)/2] = none;
        pieces[(m.f+m.t)/2] = empty;
      }
      colors[m.t] = colors[m.f];
      pieces[m.t] = pieces[m.f];
      colors[m.f] = none;
      pieces[m.f] = empty;

      if (m.fp) pieces[m.t] = superpawn;

      if (m.fc && m.cp==lion) {
        is_terminal = 1;
        result = to_move;
      }
    }

    monkey_stack[num_moves][white] = monkey_mover[white];
    monkey_stack[num_moves][black] = monkey_mover[black];
    if (m.fm) {
      monkey_mover[to_move] = m.t+1;
    } else {
      monkey_mover[to_move] = 0;
    }
    move_stack[num_moves++] = m;
    if (!m.fm) to_move = (ccolort)(to_move^3);
    hash_stack[num_moves] = compute_hash();

    // check for repetition
    // - make repetition loss as hack for monkey jump repetition
    for (int k=0; k<num_moves; ++k) {
      if (hash_stack[k] == hash_stack[num_moves]) {
        is_terminal = 1;
        //result = none;
        result = (ccolort)(to_move^3);
      }
    }

    return m.fm ? same_side_moves_again : ok_move;
  }
  make_move_flags unmake_move() {
    if (!num_moves) return invalid_move;
    cmovet m = move_stack[--num_moves];
    if (!m.fm) to_move = (ccolort)(to_move^3);
    is_terminal = 0;
    result = none;
    if (m.f != m.t) {
      colors[m.f] = colors[m.t];
      pieces[m.f] = pieces[m.t];
      colors[m.t] = none;
      pieces[m.t] = empty;
      if (m.fp) pieces[m.f] = pawn;
      if (m.fc) {
        if (m.fm) {
          colors[(m.f+m.t)/2] = (ccolort)(to_move^3);
          pieces[(m.f+m.t)/2] = (cpiecet)m.cp;
        } else {
          colors[m.t] = (ccolort)(to_move^3);
          pieces[m.t] = (cpiecet)m.cp;
        }
      }
    }
    if (m.dl) {
      colors[m.dl-1] = m.dc?white:black;
      pieces[m.dl-1] = (cpiecet)m.dp;
    }
    monkey_mover[white] = monkey_stack[num_moves][white];
    monkey_mover[black] = monkey_stack[num_moves][black];
    return ok_move;
  }
  bool terminal() const {
    return is_terminal;
  }
  hasht hash() const { return hash_stack[num_moves]; }
  hasht compute_hash() const {
    hasht h = to_move_hash[to_move];
    h ^= monkey_hash[monkey_mover[white]]^ monkey_hash[monkey_mover[black]];
    for (int i=0; i<7*7; ++i)
      h ^= piece_hash[i][colors[i]][pieces[i]];
    return h;
  }
  cmovet last_move() const { return move_stack[num_moves-1]; }
  void show(FILE* ff) const {
#if 0
    for (int r=6; r>=0; --r) {
      for (int c=0; c<7; ++c) {
        fprintf(ff, "%c",
            colors[r*7+c]==none?'.'
            : colors[r*7+c]==white?piece_abbr[pieces[r*7+c]]
            : tolower(piece_abbr[pieces[r*7+c]]));
      }
      fprintf(ff, "/");
    }
    fprintf(ff,"%c%i",to_move==white?'w':'b',num_moves);
    if (is_terminal) fprintf(ff, "%c", result==white?'W': result==black?'B': '-');
    fprintf(ff,"\n");
#else
    fprintf(ff, "     a     b     c     d     e     f     g   \n");
    fprintf(ff, "  +-----+-----+-----+-----+-----+-----+-----+\n");
    for (int r=6; r>=0; --r) {
      fprintf(ff, "%i | ",r+1);
      for (int c=0; c<7; ++c) {
        fprintf(ff, "%c", " wb"[colors[r*7+c]]);
        fprintf(ff, "%c",
          (r*7+c==monkey_mover[white]-1) ? '_' : (r*7+c==monkey_mover[black]-1) ? '^' : ' ');
        if (colors[r*7+c]) {
          fprintf(ff, "%c", piece_abbr[pieces[r*7+c]]);
        } else {
          fprintf(ff, " ");
        }
        if (r==3) fprintf(ff, c==6?" |" :" = ");
        else fprintf(ff, c==6?" |" : (c==2||c==3)?" . " :" + ");
      }
      if (r==6) fprintf(ff, "[%016llX]", hash());
      if (r==5) fprintf(ff, "[%016llX]", compute_hash());
      if (r==4) fprintf(ff, "[%2i]", num_moves);
      fprintf(ff, "\n");
    }
    fprintf(ff, "  +-----+-----+-----+-----+-----+-----+-----+\n");
    if (is_terminal) {
      fprintf(ff, "** GAME OVER: %s **\n",
          result==none?"1/2" : result==white?"1-0" : result==black?"0-1" : "???");
    } else {
      fprintf(ff, "%s to move\n", to_move==white?"White":to_move==black?"Black":"?");
    }
#endif
  }
};
const csquaret cboardt::init_squares[7*7] = {
  normal, normal, den, den, den, normal, normal,
  normal, normal, den, den, den, normal, normal,
  normal, normal, den, den, den, normal, normal,
  river, river, river, river, river, river, river,
  normal, normal, den, den, den, normal, normal,
  normal, normal, den, den, den, normal, normal,
  normal, normal, den, den, den, normal, normal,
};
const cpiecet cboardt::init_pieces[7*7] = {
  giraffe, monkey, elephant, lion, elephant, crocodile, zebra,
  pawn, pawn, pawn, pawn, pawn, pawn, pawn,
  empty, empty, empty, empty, empty, empty, empty,
  empty, empty, empty, empty, empty, empty, empty,
  empty, empty, empty, empty, empty, empty, empty,
  pawn, pawn, pawn, pawn, pawn, pawn, pawn,
  giraffe, monkey, elephant, lion, elephant, crocodile, zebra,
};
const ccolort cboardt::init_colors[7*7] = {
  white, white, white, white, white, white, white,
  white, white, white, white, white, white, white,
  none, none, none, none, none, none, none,
  none, none, none, none, none, none, none,
  none, none, none, none, none, none, none,
  black, black, black, black, black, black, black,
  black, black, black, black, black, black, black,
};

struct congo_game {
  typedef i_evalt evalt;
  typedef cmovet  movet;
  typedef cboardt boardt;
};

struct cevaluatort {
  static int piece_vals[NPIECES];
  
  void evaluate_moves_for_search(cboardt* b, cmovet* ml, i_evalt* scorel, int n) const {
    for (int i=0; i<n; ++i)
      scorel[i] = i;
  }
  i_evalt evaluate_relative(const cboardt* b, int ply, i_evalt alpha, i_evalt beta) const {
    if (b->terminal()) {
      if (!b->result) return i_evalt::STALEMATE_RELATIVE_IN_N(ply);
      if (b->result == b->to_move)
        return i_evalt::W_WIN_IN_N(ply);
      else
        return i_evalt::B_WIN_IN_N(ply);
    }
    int total = 0;
    for (int i=0; i<7*7; ++i)
      if (b->colors[i] != none)
        total += piece_vals[b->pieces[i]]*(b->colors[i]==white ? +1 : -1);
    i_evalt v = {((b->to_move==1) ? total : -total)};
    return v;
  }
  int global_search_extensions(
      const cboardt* b, int ply, i_evalt alpha, i_evalt beta, node_type type, int is_quiescence) const
  {
    return 0;
  }
  int local_search_extensions(
      const cboardt* b, int ply, i_evalt alpha, i_evalt beta, node_type type, int is_quiescence) const
  {
    return 0;
  }
  // e.g. not if in check
  int allow_stand_pat_quiescence(const cboardt* b) const {
    return 1;
  }
};
int cevaluatort::piece_vals[NPIECES] = {
  100000, 300, 400, 600, 500, 100, 200, 300, 0,
};

int main(int argc, char* argv[])
{
  srand48(-13);
  cboardt b;
  int n;
  cmovet ml[64];

  for (int i=0; !b.terminal() && i<40; ++i) {
    b.show(stdout);
    fstatus<cmovet,i_evalt>(stdout, "Move history: %M\n", b.move_stack, b.num_moves);
    n = b.gen_moves(ml);
    fstatus<cmovet,i_evalt>(stdout, "Possible moves: %M\n", ml, n);
    cmovet  m;
    {
      searcher<congo_game>  ss(b.to_move==white ? 400 : 400,1024*256,1024);
      cevaluatort e;
      cmovet pv[64];
      int pv_length = 0;
      ss.settings.show(stdout); printf("\n");
      fflush(stdout);
      i_evalt res = ss.search(&b, &e, pv, &pv_length);
      fstatus<cmovet,i_evalt>(stdout, "Search eval=%q pv=%M\n", res, pv, pv_length);
      m = pv[0];
    }
    fstatus<cmovet,i_evalt>(stdout, "<<< %m >>>\n", m);
    b.make_move(m);
  }

  b.show(stdout);
  n = b.gen_moves(ml);
  fstatus<cmovet,i_evalt>(stdout, "%M\n", ml, n);

  printf("\n=======\n");

  while (b.num_moves) {
    b.show(stdout);
    fstatus<cmovet,i_evalt>(stdout, "Move history: %M\n", b.move_stack, b.num_moves);
    fstatus<cmovet,i_evalt>(stdout, ">>> %m <<<\n", b.move_stack[b.num_moves-1]);
    b.unmake_move();
  }
  b.show(stdout);
  n = b.gen_moves(ml);
  fstatus<cmovet,i_evalt>(stdout, "%M\n", ml, n);

  return 0;
}

