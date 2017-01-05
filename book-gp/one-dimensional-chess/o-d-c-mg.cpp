#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

/* ************************************************************ */

enum side { None=0, WW=1, BB=2 };
enum result { WW_WIN=1000, BB_WIN=-1000, DRAW=0, INVALID=99001, UNKNOWN=99002 };
#define IS_WW_WIN(r_) (((r_)<=WW_WIN)&&((r_)>=(WW_WIN-100)))
#define MOVES_TO_WW_WIN(r_) (WW_WIN-(r_))
#define WW_WIN_IN(n_) ((result)(WW_WIN-(n_)))
#define IS_BB_WIN(r_) (((r_)>=BB_WIN)&&((r_)<=(BB_WIN+100)))
#define MOVES_TO_BB_WIN(r_) ((r_)-BB_WIN)
#define BB_WIN_IN(n_) ((result)(BB_WIN+(n_)))
#define IS_DRAW(r_) (((r_)<=(DRAW+100))&((r_)>=(DRAW-100)))
#define DRAW_IN(n_) ((result)(DRAW+(n_)))
#define IS_INVALID(r_) ((r_)==INVALID)
#define IS_UNKNOWN(r_) ((r_)==UNKNOWN)
#define IS_TERMINAL(r_) ((r_)<99000)
#define SWITCH_RESULT(r_) (IS_TERMINAL(r_)?\
                           (IS_DRAW(r_)?(r_):(result)-(r_)):(r_))
#define SWITCH_RESULT_BB(r_,s_) (((s_)==BB) ? SWITCH_RESULT(r_) : (r_))
#define PLY_UP_RESULT(r_) \
    ((result)(IS_WW_WIN(r_)?((r_)-1): \
              IS_BB_WIN(r_)?((r_)+1): \
              IS_DRAW(r_)?((r_)+0): \
              (r_)))
#define MAX_RESULT(r1_,r2_,s_) \
    (((IS_DRAW(r1_)&&IS_DRAW(r2_))||((s_)==WW)) \
     ? std::max((r1_),(r2_)) : std::min((r1_),(r2_)))

typedef unsigned long long int hash;

template <class TMove>
class Board {
public:
  virtual ~Board() { }
  virtual side ToMove()=0;
  virtual int Moves(std::vector<TMove>& m)=0;
  virtual bool MakeMove(TMove m)=0;
  virtual bool UnMakeMove(TMove m)=0;
  virtual int Show(FILE* f)=0;
  virtual result Result()=0;
  virtual hash Hash()=0;
  virtual result Evaluate()=0;
};

/* ************************************************************ */

class ODCBoard : public Board<int> {
public:
  static hash hash_table[3][4][64];
  static hash hash_table_tm[3];
  static void build_hash_tables() {
    memset(hash_table, 0, sizeof(hash_table));
    for (int i=0; i<3; ++i)
      for (int j=0; j<4; ++j)
        for (int k=0; k<64; ++k)
          for (int l=0; l<7; ++l)
            hash_table[i][j][k] ^= (lrand48()<<(5*l));
    for (int i=0; i<3; ++i)
          for (int l=0; l<7; ++l)
            hash_table_tm[i] ^= (lrand48()<<(5*l));
  }
  enum piece {Empty=0, King=1, Rook=2, Knight=3};
  enum move_flags {MF_Cap=0x80000000,};
  int move(int f, int t) {
    if (b_p[t]!=Empty)
      return (t<<8) | f | (b_p[t]<<16) | MF_Cap;
    else
      return (t<<8) | f;
  }
  ODCBoard(int n)
      : b_p(new piece[n]), b_s(new side[n]), n_b(n), to_move(WW) {
        memset(b_p, 0, sizeof(*b_p)*n);
        memset(b_s, 0, sizeof(*b_s)*n);
        b_s[0] = WW; b_p[0] = King;
        b_s[1] = WW; b_p[1] = Knight;
        b_s[2] = WW; b_p[2] = Rook;
        b_s[n-3] = BB; b_p[n-3] = Rook;
        b_s[n-2] = BB; b_p[n-2] = Knight;
        b_s[n-1] = BB; b_p[n-1] = King;
  }
  ~ODCBoard() {
    delete[] b_p;
    delete[] b_s;
  }
  side ToMove() { return to_move; }
  bool MakeMove(int m) {
    int fsq = m&0xFF;
    int tsq = (m>>8)&0xFF;
    b_s[tsq] = b_s[fsq];
    b_p[tsq] = b_p[fsq];
    b_s[fsq] = None;
    b_p[fsq] = Empty;
    to_move = (side)(to_move^3);
    return true;
  }
  bool UnMakeMove(int m) {
    int fsq = m&0xFF;
    int tsq = (m>>8)&0xFF;
    b_s[fsq] = b_s[tsq];
    b_p[fsq] = b_p[tsq];
    if (m & MF_Cap) {
      b_s[tsq] = to_move;
      b_p[tsq] = (piece)((m>>16)&0x0F);
    } else {
    b_s[tsq] = None;
    b_p[tsq] = Empty;
    }
    to_move = (side)(to_move^3);
    return true;
  }
  bool KingInCheck(side kic) {
    side tm = (side)(kic^3);
    for (int j,i=0; i<n_b; ++i) {
      if (b_s[i] == tm) {
        switch (b_p[i]) {
          case King:
            if ((i>0)     && (b_p[i-1]==King)) return true;
            if ((i<n_b-1) && (b_p[i+1]==King)) return true;
            // TODO: castle
            break;
          case Knight:
            if ((i>1)     && (b_s[i-2]==kic) && (b_p[i-2]==King)) return true;
            if ((i<n_b-2) && (b_s[i+2]==kic) && (b_p[i+2]==King)) return true;
            break;
          case Rook:
            j = i-1;
            while ((j>=0) && (b_s[j]==None)) --j;
            if ((j>=0) && (b_s[j]==kic) && (b_p[j]==King)) return true;

            j = i+1;
            while ((j<n_b) && (b_s[j]==None)) ++j;
            if ((j<n_b) && (b_s[j]==kic) && (b_p[j]==King)) return true;
            break;
          default: break;
        }
      }
    }
    return false;
  }
  bool IsMoveValid(int m) {
    MakeMove(m);
    bool in_check = KingInCheck((side)(to_move^3));
    UnMakeMove(m);
    return !in_check;
  }
  int Moves(std::vector<int>& m) {
    int n = 0;
    for (int i=0; i<n_b; ++i) {
      if (b_s[i] == to_move) {
        switch (b_p[i]) {
          case King:
            if ((i>0) && (b_s[i-1]!=to_move)) {
              int mv = move(i,i-1);
              if (IsMoveValid(mv)) {
                m.push_back(mv);
                ++n;
              }
            }
            if ((i<n_b-1) && (b_s[i+1]!=to_move)) {
              int mv = move(i,i+1);
              if (IsMoveValid(mv)) {
                m.push_back(mv);
                ++n;
              }
            }
            // TODO: castle
            break;
          case Rook:
            for (int j=i-1; j>=0; --j) {
              if (b_s[j]==None) {
                int mv = move(i,j);
                if (IsMoveValid(mv)) {
                  m.push_back(mv);
                  ++n;
                }
              } else {
                if (b_s[j]==(to_move^3)) {
                  int mv = move(i,j);
                  if (IsMoveValid(mv)) {
                    m.push_back(mv);
                    ++n;
                  }
                }
                break;
              }
            }
            for (int j=i+1; j<n_b; ++j) {
              if (b_s[j]==None) {
                int mv = move(i,j);
                if (IsMoveValid(mv)) {
                  m.push_back(mv);
                  ++n;
                }
              } else {
                if (b_s[j]==(to_move^3)) {
                  int mv = move(i,j);
                  if (IsMoveValid(mv)) {
                    m.push_back(mv);
                    ++n;
                  }
                }
                break;
              }
            }
            break;
          case Knight:
            if ((i>1) && (b_s[i-2]!=to_move)) {
              int mv = move(i,i-2);
              if (IsMoveValid(mv)) {
                m.push_back(mv);
                ++n;
              }
            }
            if ((i<n_b-2) && (b_s[i+2]!=to_move)) {
              int mv = move(i,i+2);
              if (IsMoveValid(mv)) {
                m.push_back(mv);
                ++n;
              }
            }
            break;
          default: break;
        }
      }
    }
    return n;
  }
  result Result() {
    bool wk=false, bk=false;
    for (int i=0; i<n_b; ++i)
      if (b_p[i]==King) {
        if (b_s[i]==WW)
          wk = true;
        else
          bk = true;
      }
    if ( wk && !bk) return WW_WIN;
    if (!wk &&  bk) return BB_WIN;
    if (!wk && !bk) return DRAW;
    std::vector<int> dummy;
    if (to_move==WW) {
      if (KingInCheck(BB)) return WW_WIN;
      else if (KingInCheck(WW)) {
        if (Moves(dummy)==0) return BB_WIN;
      }
    } else {
      if (KingInCheck(WW)) return BB_WIN;
      else if (KingInCheck(BB)) {
        if (Moves(dummy)==0) return WW_WIN;
      }
    }
    return UNKNOWN;
  }
  result Evaluate() {
    result res = Result();
    if (IS_TERMINAL(res)) return res;
    res = (result)0;
    for (int i=0; i<n_b; ++i) {
      switch (b_p[i]) {
        case Knight: res = (result)((b_s[i]==WW) ? (res+10) : (res-10)); break;
        case Rook: res = (result)((b_s[i]==WW) ? (res+30) : (res-30)); break;
        case King: break;
        case Empty: break;
      }
    }
    return res;
  }
  char PieceChar(piece p, side s) {
    switch (p) {
      case King: return (s==WW)?'K':'k';
      case Knight: return (s==WW)?'N':'n';
      case Rook: return (s==WW)?'R':'r';
      case Empty: return '.';
      default: return '?';
    }
  }
  int Show(FILE* f) {
    fprintf(f, "[");
    for (int i=0; i<n_b; ++i) {
      fprintf(f, "%c", PieceChar(b_p[i], b_s[i]));
    }
    fprintf(f, "]");
    fprintf(f, " <%s to move>  {%i}\n", to_move==WW ? "W" : "B", Result());
    return 0;
  }
  int ShowMove(FILE* f, int m) {
    fprintf(f, "%c%i%s%i",
            PieceChar(b_p[m&0xFF], b_s[m&0xFF]),
            1+(m&0xFF), ((m&MF_Cap)?"x":"-"), 1+((m>>8)&0xFF));
    return 0;
  }
  hash Hash() {
    hash h = 0LL;
    for (int i=0; i<n_b; ++i)
      h ^= hash_table[b_s[i]][b_p[i]][i];
    h ^= hash_table_tm[to_move];
    return h;
  }
//private:
  piece* b_p;
  side* b_s;
  int n_b;
  side to_move;
};
hash ODCBoard::hash_table[3][4][64];
hash ODCBoard::hash_table_tm[3];

/* ************************************************************ */

enum tt_type { UNKNOWN_BD=0, EXACT_BD, UPPER_BD, LOWER_BD };
struct ttentry {
  hash lock;
  result res;
  tt_type type;
  int depth;
};

int nodes_searched;
#define TTN (1024*1024)
ttentry ttable[TTN];
int tthits;
int tthits_false;
int tthits_shallow;
int tthits_exact;
int tthits_upper;
int tthits_lower;
void reset_ttable() {
  tthits = 0;
  tthits_false = 0;
  tthits_shallow = 0;
  tthits_exact = 0;
  tthits_upper = 0;
  tthits_lower = 0;
  memset(ttable, 0, sizeof(ttable));
}
template <class TMove>
result minimax(Board<TMove>& b, int depth, bool tt=false) {
  hash h;
  if (tt) {
    h = b.Hash();
    if (ttable[h%TTN].type == EXACT_BD) {
      if (ttable[h%TTN].lock == h) {
        if (ttable[h%TTN].depth >= depth) {
          ++tthits;
          return ttable[h%TTN].res;
        } else {
          ++tthits_shallow;
        }
      } else {
        ++tthits_false;
      }
    }
  }
  result eval = b.Result();
  ++nodes_searched;
  if (IS_TERMINAL(eval)) {
    if (tt) {
      ttable[h%TTN].lock = h;
      ttable[h%TTN].depth = depth;
      ttable[h%TTN].res = eval;
      ttable[h%TTN].type = EXACT_BD;
    }
    return eval;
  }
  if (depth == 0) return b.Evaluate();
  result best_v;
  if (b.ToMove() == WW) {
    std::vector<TMove> moves;
    best_v = BB_WIN_IN(-1);
    int num_m = b.Moves(moves);
    if (num_m==0) return DRAW;
    for (int m=0; m<num_m; ++m) {
      b.MakeMove(moves[m]);
      result res = minimax(b, depth-1, tt);
      b.UnMakeMove(moves[m]);
      res = PLY_UP_RESULT(res);
      if (res > best_v) best_v = res;
    }
  } else {
    std::vector<TMove> moves;
    best_v = WW_WIN_IN(-1);
    int num_m = b.Moves(moves);
    if (num_m==0) return DRAW;
    for (int m=0; m<num_m; ++m) {
      b.MakeMove(moves[m]);
      result res = minimax(b, depth-1, tt);
      b.UnMakeMove(moves[m]);
      res = PLY_UP_RESULT(res);
      if (res < best_v) best_v = res;
    }
  }
  if (tt) {
    ttable[h%TTN].lock = h;
    ttable[h%TTN].depth = depth;
    ttable[h%TTN].res = best_v;
    ttable[h%TTN].type = EXACT_BD;
  }
  return best_v;
}
template <class TMove>
result negamax(Board<TMove>& b, int depth, bool tt=false) {
  hash h;
  if (tt) {
    h = b.Hash();
    if (ttable[h%TTN].type == EXACT_BD) {
      if (ttable[h%TTN].lock == h) {
        if (ttable[h%TTN].depth >= depth) {
          ++tthits;
          return SWITCH_RESULT_BB(ttable[h%TTN].res, b.ToMove());
        } else {
          ++tthits_shallow;
        }
      } else {
        ++tthits_false;
      }
    }
  }
  result eval = b.Result();
  eval = SWITCH_RESULT_BB(eval, b.ToMove());
  ++nodes_searched;
  if (IS_TERMINAL(eval)) {
    if (tt) {
      ttable[h%TTN].lock = h;
      ttable[h%TTN].depth = depth;
      ttable[h%TTN].res = SWITCH_RESULT_BB(eval, b.ToMove());
      ttable[h%TTN].type = EXACT_BD;
    }
    return eval;
  }
  if (depth == 0) return SWITCH_RESULT_BB(b.Evaluate(), b.ToMove());
  std::vector<TMove> moves;
  result best_v = (result)(BB_WIN-1);
  int num_m = b.Moves(moves);
  if (num_m==0) return DRAW;
  for (int m=0; m<num_m; ++m) {
    b.MakeMove(moves[m]);
    result res = negamax(b, depth-1, tt);
    b.UnMakeMove(moves[m]);
    res = SWITCH_RESULT(res);
    res = PLY_UP_RESULT(res);
    if (res > best_v) best_v = res;
  }
  if (tt) {
    ttable[h%TTN].lock = h;
    ttable[h%TTN].depth = depth;
    ttable[h%TTN].res = SWITCH_RESULT_BB(best_v, b.ToMove());
    ttable[h%TTN].type = EXACT_BD;
  }
  return best_v;
}
template <class TMove>
result alphabeta(Board<TMove>& b, int depth, result alpha, result beta,
                 bool tt=false) {
  hash h;
  if (tt) {
    h = b.Hash();
    if (ttable[h%TTN].type != UNKNOWN_BD) {
      if (ttable[h%TTN].lock==h) {
        if (ttable[h%TTN].depth >= depth) {
          ++tthits;
          switch (ttable[h%TTN].type) {
            case EXACT_BD:
              ++tthits_exact;
              return SWITCH_RESULT_BB(ttable[h%TTN].res, b.ToMove());
            case LOWER_BD:
              ++tthits_lower;
              // todo: switch?
              if (ttable[h%TTN].res > alpha) alpha = ttable[h%TTN].res;
              break;
            case UPPER_BD:
              ++tthits_upper;
              // todo: switch?
              if (ttable[h%TTN].res < beta) beta = ttable[h%TTN].res;
              break;
            default: break;
          }
          if (alpha >= beta) return alpha;
        } else {
          ++tthits_shallow;
        }
      } else {
        ++tthits_false;
      }
    }
  }
  result eval = (b.ToMove()==WW) ? b.Result() : SWITCH_RESULT(b.Result());
  //b.Show(stdout); printf("*** %i ***\n", eval);
  ++nodes_searched;
  if (IS_TERMINAL(eval)) {
    if (tt) {
      ttable[h%TTN].lock = h;
      ttable[h%TTN].depth = depth;
      ttable[h%TTN].res = SWITCH_RESULT_BB(eval, b.ToMove());
      ttable[h%TTN].type = EXACT_BD;
    }
    return eval;
  }
  if (depth == 0) return SWITCH_RESULT_BB(b.Evaluate(), b.ToMove());
  std::vector<TMove> moves;
  int num_m = b.Moves(moves);
  if (num_m==0) return DRAW;
  result best_v = BB_WIN_IN(-1);
  for (int m=0; m<num_m; ++m) {
    b.MakeMove(moves[m]);
    result res = alphabeta(b, depth-1, (result)-beta, (result)-alpha, tt);
    b.UnMakeMove(moves[m]);
    res = SWITCH_RESULT(res);
    res = PLY_UP_RESULT(res);
    if (res > best_v) best_v = res;
    if (best_v > alpha) alpha = best_v;
    if (alpha >= beta) break;
  }
  if (tt) {
    ttable[h%TTN].lock = h;
    ttable[h%TTN].depth = depth;
    if (best_v <= alpha) {
      ttable[h%TTN].res = (b.ToMove()==WW) ? best_v : SWITCH_RESULT(best_v);
      ttable[h%TTN].type = LOWER_BD;
    } else if (best_v >= beta) {
      ttable[h%TTN].res = (b.ToMove()==WW) ? best_v : SWITCH_RESULT(best_v);
      ttable[h%TTN].type = UPPER_BD;
    } else {
      ttable[h%TTN].res = (b.ToMove()==WW) ? best_v : SWITCH_RESULT(best_v);
      ttable[h%TTN].type = EXACT_BD;
    }
  }
  return best_v;
}

void test(ODCBoard& b, int depth) {
  int r;
  printf("---------------- depth=%i ------------------------\n", depth);
  nodes_searched = 0;
  r = minimax(b, depth);
  printf("Minimax:        Result=%i  nodes=%i\n", r, nodes_searched);
  nodes_searched = 0;
  reset_ttable();
  r = minimax(b, depth, true);
  printf("Minimax(TT):    Result=%i  nodes=%i  hits=%i  (false=%i, shallow=%i)\n",
         r, nodes_searched, tthits, tthits_false, tthits_shallow);

  nodes_searched = 0;
  r = negamax(b, depth);
  r = SWITCH_RESULT_BB(r, b.ToMove());
  printf("Negamax:        Result=%i  nodes=%i\n", r, nodes_searched);
  nodes_searched = 0;
  reset_ttable();
  r = negamax(b, depth, true);
  r = SWITCH_RESULT_BB(r, b.ToMove());
  printf("Negamax(TT):    Result=%i  nodes=%i  hits=%i  (false=%i, shallow=%i)\n",
         r, nodes_searched, tthits, tthits_false, tthits_shallow);

  nodes_searched = 0;
  r = alphabeta(b, depth, BB_WIN_IN(-1), WW_WIN_IN(-1));
  if (b.ToMove()==BB) r = SWITCH_RESULT(r);
  printf("Alphabeta:      Result=%i  nodes=%i\n", r, nodes_searched);
  nodes_searched = 0;
  reset_ttable();
  r = alphabeta(b, depth, BB_WIN_IN(-1), WW_WIN_IN(-1), true);
  r = SWITCH_RESULT_BB(r, b.ToMove());
  printf("Alphabeta(TT):  Result=%i  nodes=%i  hits=%i  (false=%i, shallow=%i)\n",
         r, nodes_searched, tthits, tthits_false, tthits_shallow);
  printf("  upper_hits=%i lower_hits=%i exact_hits=%i\n",
         tthits_upper, tthits_lower, tthits_exact);
}
void test_ab(ODCBoard& b, int depth) {
  int r;
  printf("---------------- test_ab(b=..., depth=%i) ------------------------", depth);
  b.Show(stdout);
  r = alphabeta(b, depth, BB_WIN_IN(-1), WW_WIN_IN(-1), true);
  r = SWITCH_RESULT_BB(r, b.ToMove());
  printf("Alphabeta(TT):  Result=%i  nodes=%i  hits=%i  (false=%i, shallow=%i)\n",
         r, nodes_searched, tthits, tthits_false, tthits_shallow);
  printf("  upper_hits=%i lower_hits=%i exact_hits=%i\n",
         tthits_upper, tthits_lower, tthits_exact);
}
void find_w_strat(ODCBoard& b, int ind, int depth) {
  if (depth<=0) return;
  std::vector<int> w_moves;
  int num_wm = b.Moves(w_moves);
  int best_m;
  result best_v = BB_WIN_IN(-1);
  for (int i=0; i<num_wm; ++i) {
    b.MakeMove(w_moves[i]);
    result res = alphabeta(b, 12, BB_WIN_IN(-1), WW_WIN_IN(-1), true);
    res = SWITCH_RESULT_BB(res, b.ToMove());
    b.UnMakeMove(w_moves[i]);
    if (res > best_v) {
      best_m = w_moves[i];
      best_v = res;
    }
  }
  if (num_wm) {
    printf("%*s W:", ind*4, "");
    b.ShowMove(stdout, best_m);
    printf(" [%i] --> ", best_v);
    b.MakeMove(best_m);
    b.Show(stdout);
  } else {
    printf("%*s", ind*4, ""); b.Show(stdout);
    return;
  }

  std::vector<int> b_moves;
  int num_bm = b.Moves(b_moves);
  for (int i=0; i<num_bm; ++i) {
    printf("%*s B:", ind*4+2, "");
    b.ShowMove(stdout, b_moves[i]);
    printf(" --> ");
    b.MakeMove(b_moves[i]);
    b.Show(stdout);
    find_w_strat(b, ind+1, depth-1);
    b.UnMakeMove(b_moves[i]);
  }
  b.UnMakeMove(best_m);
}

int main(int argc, char* argv[]) {
  setvbuf(stdout, NULL, _IONBF, 0);
  ODCBoard::build_hash_tables();
  //int board_size = 17;
  int board_size = 7;

  if (1) {
    ODCBoard b(board_size);
    nodes_searched = 0;
    reset_ttable();
    std::vector<int> om;
    for (int i=0; i<25; ++i) {
      std::vector<int> moves;
      b.Show(stdout);
      int n = b.Moves(moves);
      for (int i=0; i<n; ++i) {
        b.ShowMove(stdout, moves[i]);
        printf(" ");
      }
      if (n==0) printf("<no moves>");
      printf("\n");
      if (n==0) break;
      //b.MakeMove(moves[11%n]);
      //om.push_back(moves[11%n]);
      b.MakeMove(moves[2%n]);
      om.push_back(moves[2%n]);
    }
    printf("\n");
    b.Show(stdout);
    printf("\n");
    for (int i=om.size()-1; i>=0; --i) {
      b.UnMakeMove(om[i]);
      b.Show(stdout);
    }
  }

  if (0) {
    ODCBoard b(board_size);
    reset_ttable();
  }

  if (0) {
    ODCBoard b(board_size);
    nodes_searched = 0;
    reset_ttable();
    find_w_strat(b, 0, 4);
  }

  if (1) {
    ODCBoard b(board_size);
    nodes_searched = 0;
    reset_ttable();
    for (int d=2; d<20; ++d)
      test(b, d);
  }

  if (1) {
    ODCBoard b(board_size);
    nodes_searched = 0;
    reset_ttable();
    //for (int d=2; d<10; ++d)
    for (int d=2; d<25; ++d)
    {
      b.Show(stdout);
      test_ab(b, d);
    }
  }

  return 0;
}
