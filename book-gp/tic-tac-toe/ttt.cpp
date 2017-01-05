#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

/* ************************************************************ */

enum side { None=0, XX=1, OO=2 };
enum result { XX_WIN=1000, OO_WIN=-1000, DRAW=0, INVALID=99001, UNKNOWN=99002 };
#define IS_XX_WIN(r_) (((r_)<=XX_WIN)&&((r_)>=(XX_WIN-100)))
#define MOVES_TO_XX_WIN(r_) (XX_WIN-(r_))
#define XX_WIN_IN(n_) ((result)(XX_WIN-(n_)))
#define IS_OO_WIN(r_) (((r_)>=OO_WIN)&&((r_)<=(OO_WIN+100)))
#define MOVES_TO_OO_WIN(r_) ((r_)-OO_WIN)
#define OO_WIN_IN(n_) ((result)(OO_WIN+(n_)))
#define IS_DRAW(r_) (((r_)<=(DRAW+100))&((r_)>=(DRAW-100)))
#define DRAW_IN(n_) ((result)(DRAW+(n_)))
#define IS_INVALID(r_) ((r_)==INVALID)
#define IS_UNKNOWN(r_) ((r_)==UNKNOWN)
#define IS_TERMINAL(r_) ((r_)<99000)
#define SWITCH_RESULT(r_) (IS_TERMINAL(r_)?\
                           (IS_DRAW(r_)?(r_):(result)-(r_)):(r_))
#define SWITCH_RESULT_OO(r_,s_) (((s_)==OO) ? SWITCH_RESULT(r_) : (r_))
#define PLY_UP_RESULT(r_) \
    ((result)(IS_XX_WIN(r_)?((r_)-1): \
              IS_OO_WIN(r_)?((r_)+1): \
              IS_DRAW(r_)?((r_)+1):(r_)))
#define MAX_RESULT(r1_,r2_,s_) \
    (((IS_DRAW(r1_)&&IS_DRAW(r2_))||((s_)==XX)) \
     ? std::max((r1_),(r2_)) : std::min((r1_),(r2_)))

typedef unsigned long long int hash;

template <class TMove>
class Board {
public:
    virtual side ToMove()=0;
    virtual int Moves(std::vector<TMove>& m)=0;
    virtual bool MakeMove(TMove m)=0;
    virtual bool UnMakeMove(TMove m)=0;
    virtual int Show(FILE* f)=0;
    virtual result Result()=0;
    virtual hash Hash()=0;
};

/* ************************************************************ */

/*
 * 0 1 2
 * 3 4 5
 * 6 7 8
 */
#define XX1 ((XX<<(0*2))|(XX<<(1*2))|(XX<<(2*2)))
#define XX2 ((XX<<(0*2))|(XX<<(4*2))|(XX<<(8*2)))
#define XX3 ((XX<<(0*2))|(XX<<(3*2))|(XX<<(6*2)))
#define XX4 ((XX<<(1*2))|(XX<<(4*2))|(XX<<(7*2)))
#define XX5 ((XX<<(2*2))|(XX<<(4*2))|(XX<<(6*2)))
#define XX6 ((XX<<(2*2))|(XX<<(5*2))|(XX<<(8*2)))
#define XX7 ((XX<<(3*2))|(XX<<(4*2))|(XX<<(5*2)))
#define XX8 ((XX<<(6*2))|(XX<<(7*2))|(XX<<(8*2)))
#define OO1 ((OO<<(0*2))|(OO<<(1*2))|(OO<<(2*2)))
#define OO2 ((OO<<(0*2))|(OO<<(4*2))|(OO<<(8*2)))
#define OO3 ((OO<<(0*2))|(OO<<(3*2))|(OO<<(6*2)))
#define OO4 ((OO<<(1*2))|(OO<<(4*2))|(OO<<(7*2)))
#define OO5 ((OO<<(2*2))|(OO<<(4*2))|(OO<<(6*2)))
#define OO6 ((OO<<(2*2))|(OO<<(5*2))|(OO<<(8*2)))
#define OO7 ((OO<<(3*2))|(OO<<(4*2))|(OO<<(5*2)))
#define OO8 ((OO<<(6*2))|(OO<<(7*2))|(OO<<(8*2)))
#define BOARD_FULL(b_) (!((~(((b_)&0x15555)|(((b_)&0x2AAAA)>>1)))&0x15555))
#define MOVE(b_,m_,s_) ((b_)|((s_)<<(2*(m_))))
#define CAN_MOVE(b_,m_) (!((b_)&(3<<(2*(m_)))))

class TTTBoard : public Board<int> {
public:
  TTTBoard() : b(0), to_move(XX) { }
  side ToMove() { return to_move; }
  bool MakeMove(int m) {
    b |= (to_move<<(2*m));
    to_move = (side)(to_move^3);
    return true;
  }
  bool UnMakeMove(int m) {
    b &= ~(3<<(2*m));
    to_move = (side)(to_move^3);
    return true;
  }
  int Moves(std::vector<int>& m) {
    int n = 0;
    for (int i=0; i<9; ++i)
      if (CAN_MOVE(b,i)) {
        m.push_back(i);
        ++n;
      }
    return n;
  }
  result XWin() {
    if ((b&XX1)==XX1) return XX_WIN;
    if ((b&XX2)==XX2) return XX_WIN;
    if ((b&XX3)==XX3) return XX_WIN;
    if ((b&XX4)==XX4) return XX_WIN;
    if ((b&XX5)==XX5) return XX_WIN;
    if ((b&XX6)==XX6) return XX_WIN;
    if ((b&XX7)==XX7) return XX_WIN;
    if ((b&XX8)==XX8) return XX_WIN;
    return UNKNOWN;
  }
  result OWin() {
    if ((b&OO1)==OO1) return OO_WIN;
    if ((b&OO2)==OO2) return OO_WIN;
    if ((b&OO3)==OO3) return OO_WIN;
    if ((b&OO4)==OO4) return OO_WIN;
    if ((b&OO5)==OO5) return OO_WIN;
    if ((b&OO6)==OO6) return OO_WIN;
    if ((b&OO7)==OO7) return OO_WIN;
    if ((b&OO8)==OO8) return OO_WIN;
    return UNKNOWN;
  }
  result Result() {
    bool xw = IS_XX_WIN(XWin());
    bool ow = IS_OO_WIN(OWin());
    if (xw && ow) return INVALID;
    else if (xw) return XX_WIN;
    else if (ow) return OO_WIN;
    else if (BOARD_FULL(b)) return DRAW;
    else return UNKNOWN;
  }
  int Show(FILE* f) {
    for (int i=0; i<3; ++i) {
      fprintf(f, "    ");
      for (int j=0; j<3; ++j) {
        int n = i*3 + j;
        if (b & (XX << (n*2))) fprintf(f, "X");
        else if (b & (OO << (n*2))) fprintf(f, "O");
        else fprintf(f, ".");
        fprintf(f, " ");
      }
      fprintf(f, "\n");
    }
    fprintf(f, "<%s to move>\n", to_move==XX ? "X" : "O");
    return 0;
  }
  hash Hash() {
    hash h = 0LL;
    for (int i=0; i<9; ++i)
      h = 3LL*h + (hash)((b>>(2*i))&3);
    return h;
  }
  void FromHash(hash h) {
    int parity = 0;
    b = 0;
    for (int i=0; i<9; ++i) {
      if (h%3) parity ^= 1;
      b = (b<<2) | (h%3);
      h /= 3;
    }
    to_move = parity ? OO : XX;
  }
//private:
  int b;
  side to_move;
};

/* ************************************************************ */

enum tt_type { UNKNOWN_BD=0, EXACT_BD, UPPER_BD, LOWER_BD };
struct ttentry {
  result res;
  tt_type type;
};

int nodes_searched;
ttentry ttable[19683]; // 3^9
int tthits;
int tthits_exact;
int tthits_upper;
int tthits_lower;
void reset_ttable() {
  tthits = 0;
  tthits_exact = 0;
  tthits_upper = 0;
  tthits_lower = 0;
  memset(ttable, 0, sizeof(ttable));
}
template <class TMove, bool tt>
result minimax(Board<TMove>& b) {
  hash h;
  if (tt) {
    h = b.Hash();
    if (ttable[h].type == EXACT_BD) {
      ++tthits;
      return ttable[h].res;
    }
  }
  result eval = b.Result();
  ++nodes_searched;
  if (IS_TERMINAL(eval)) {
    if (tt) { ttable[h].res = eval; ttable[h].type = EXACT_BD; }
    return eval;
  }
  result best_v;
  if (b.ToMove() == XX) {
    std::vector<TMove> moves;
    best_v = OO_WIN_IN(-1);
    int num_m = b.Moves(moves);
    for (int m=0; m<num_m; ++m) {
      b.MakeMove(moves[m]);
      result res = minimax<TMove,tt>(b);
      b.UnMakeMove(moves[m]);
      res = PLY_UP_RESULT(res);
      if (res > best_v) best_v = res;
    }
  } else {
    std::vector<TMove> moves;
    best_v = XX_WIN_IN(-1);
    int num_m = b.Moves(moves);
    for (int m=0; m<num_m; ++m) {
      b.MakeMove(moves[m]);
      result res = minimax<TMove,tt>(b);
      b.UnMakeMove(moves[m]);
      res = PLY_UP_RESULT(res);
      if (res < best_v) best_v = res;
    }
  }
  if (tt) { ttable[h].res = best_v; ttable[h].type = EXACT_BD; }
  return best_v;
}
template <class TMove, bool tt>
result negamax(Board<TMove>& b) {
  hash h;
  if (tt) {
    h = b.Hash();
    if (ttable[h].type == EXACT_BD) {
      ++tthits;
      return SWITCH_RESULT_OO(ttable[h].res, b.ToMove());
    }
  }
  result eval = b.Result();
  eval = SWITCH_RESULT_OO(eval, b.ToMove());
  ++nodes_searched;
  if (IS_TERMINAL(eval)) {
    if (tt) {
      ttable[h].res = SWITCH_RESULT_OO(eval, b.ToMove());
      ttable[h].type = EXACT_BD;
    }
    return eval;
  }
  std::vector<TMove> moves;
  result best_v = (result)(OO_WIN-1);
  int num_m = b.Moves(moves);
  for (int m=0; m<num_m; ++m) {
    b.MakeMove(moves[m]);
    result res = negamax<TMove,tt>(b);
    b.UnMakeMove(moves[m]);
    res = SWITCH_RESULT(res);
    res = PLY_UP_RESULT(res);
    if (res > best_v) best_v = res;
  }
  if (tt) {
    ttable[h].res = SWITCH_RESULT_OO(best_v, b.ToMove());
    ttable[h].type = EXACT_BD;
  }
  return best_v;
}
template <class TMove, bool tt>
result alphabeta(Board<TMove>& b, result alpha, result beta) {
  hash h;
  if (tt) {
    h = b.Hash();
    if (ttable[h].type != UNKNOWN_BD) {
      ++tthits;
      switch (ttable[h].type) {
        case EXACT_BD:
          ++tthits_exact;
          return SWITCH_RESULT_OO(ttable[h].res, b.ToMove());
        case LOWER_BD:
          ++tthits_lower;
          // todo: switch?
          if (ttable[h].res > alpha) alpha = ttable[h].res;
          break;
        case UPPER_BD:
          ++tthits_upper;
          // todo: switch?
          if (ttable[h].res < beta) beta = ttable[h].res;
          break;
        default: break;
      }
      if (alpha >= beta) return alpha;
    }
  }
  result eval = (b.ToMove()==XX) ? b.Result() : SWITCH_RESULT(b.Result());
  ++nodes_searched;
  if (IS_TERMINAL(eval)) {
    if (tt) {
      ttable[h].res = SWITCH_RESULT_OO(eval, b.ToMove());
      ttable[h].type = EXACT_BD;
    }
    return eval;
  }
  std::vector<TMove> moves;
  int num_m = b.Moves(moves);
  result best_v = OO_WIN_IN(-1);
  for (int m=0; m<num_m; ++m) {
    b.MakeMove(moves[m]);
    result res = alphabeta<TMove,tt>(b, (result)-beta, (result)-alpha);
    b.UnMakeMove(moves[m]);
    res = SWITCH_RESULT(res);
    res = PLY_UP_RESULT(res);
    if (res > best_v) best_v = res;
    if (best_v > alpha) alpha = best_v;
    if (alpha >= beta) break;
  }
  if (tt) {
    if (best_v <= alpha) {
      ttable[h].res = (b.ToMove()==XX) ? best_v : SWITCH_RESULT(best_v);
      ttable[h].type = LOWER_BD;
    } else if (best_v >= beta) {
      ttable[h].res = (b.ToMove()==XX) ? best_v : SWITCH_RESULT(best_v);
      ttable[h].type = UPPER_BD;
    } else {
      ttable[h].res = (b.ToMove()==XX) ? best_v : SWITCH_RESULT(best_v);
      ttable[h].type = EXACT_BD;
    }
  }
  return best_v;
}

int num_passes = 0;
result rttable[19683]; // 3^9
void retrograde() {
  bool changed = true;
  TTTBoard b;
  for (int i=0; i<19683; ++i) {
    b.FromHash((hash)i);
    rttable[i] = b.Result();
  }
  while (changed) {
    fprintf(stderr, ".");
    ++num_passes;
    changed = false;
    for (int i=0; i<19683; ++i) {
      if (IS_UNKNOWN(rttable[i])) {
        bool exists_o_win = false;
        bool exists_x_win = false;
        bool exists_draw = false;
        bool exists_unknown = false;
        b.FromHash((hash)i);
        std::vector<int> moves;
        // scan all successors
        int num_m = b.Moves(moves);
        result best_v = (b.ToMove()==XX) ? OO_WIN_IN(-1) : XX_WIN_IN(-1);
        for (int m=0; m<num_m; ++m) {
          b.MakeMove(moves[m]);
          result res = rttable[b.Hash()];
          b.UnMakeMove(moves[m]);
          res = PLY_UP_RESULT(res);

          if (IS_OO_WIN(res)) exists_o_win = true;
          if (IS_XX_WIN(res)) exists_x_win = true;
          if (IS_DRAW(res)) exists_draw = true;
          if (IS_UNKNOWN(res)) exists_unknown = true;
          best_v = MAX_RESULT(best_v,res,b.ToMove());
        }
        {
          result res = UNKNOWN;
          if (b.ToMove()==XX) {
            if (exists_x_win || !exists_unknown) res = best_v;
          } else {
            if (exists_o_win || !exists_unknown) res = best_v;
          }
          if (!IS_UNKNOWN(res)) {
            rttable[b.Hash()] = res;
            changed = true;
          }
        }
      }
    }
  }
}

void test(TTTBoard& b) {
  int r;
  printf("----------------------------------------\n");
  nodes_searched = 0;
  r = minimax<int,false>(b);
  printf("Minimax:        Result=%i  nodes=%i\n", r, nodes_searched);
  nodes_searched = 0;
  reset_ttable();
  r = minimax<int,true>(b);
  printf("Minimax(TT):    Result=%i  nodes=%i  hits=%i\n",
         r, nodes_searched, tthits);

  nodes_searched = 0;
  r = negamax<int,false>(b);
  if (b.ToMove()==OO) r = SWITCH_RESULT(r);
  printf("Negamax:        Result=%i  nodes=%i\n", r, nodes_searched);
  nodes_searched = 0;
  reset_ttable();
  r = negamax<int,true>(b);
  if (b.ToMove()==OO) r = SWITCH_RESULT(r);
  printf("Negamax(TT):    Result=%i  nodes=%i  hits=%i\n",
         r, nodes_searched, tthits);

  nodes_searched = 0;
  r = alphabeta<int,false>(b, OO_WIN_IN(-1), XX_WIN_IN(-1));
  if (b.ToMove()==OO) r = SWITCH_RESULT(r);
  printf("Alphabeta:      Result=%i  nodes=%i\n", r, nodes_searched);
  nodes_searched = 0;
  reset_ttable();
  r = alphabeta<int,true>(b, OO_WIN_IN(-1), XX_WIN_IN(-1));
  if (b.ToMove()==OO) r = SWITCH_RESULT(r);
  printf("Alphabeta(TT):  Result=%i  nodes=%i  hits=%i\n",
         r, nodes_searched, tthits);
  printf("  upper_hits=%i lower_hits=%i exact_hits=%i\n",
         tthits_upper, tthits_lower, tthits_exact);

  printf("Retrograde:     Result=%i\n\n", rttable[b.Hash()]);
}
void validate_retro() {
  for (int i=0; i<19683; ++i) {
    TTTBoard b;
    b.FromHash((hash)i);
    if (IS_INVALID(b.Result())) continue;
    result r1 = alphabeta<int,false>(b, OO_WIN_IN(-1), XX_WIN_IN(-1));
    if (b.ToMove()==OO) r1 = SWITCH_RESULT(r1);
    result r2 = rttable[i];
    if ((IS_XX_WIN(r1)!=IS_XX_WIN(r2))
        ||(IS_OO_WIN(r1)!=IS_OO_WIN(r2))
        ||(IS_DRAW(r1)!=IS_DRAW(r2)))
      printf("%i: r1=%i, r2=%i\n", i, r1, r2);
  }
}
int main(int argc, char* argv[]) {
  TTTBoard b;

  int r;
  nodes_searched = 0;
  for (int i=0; i<100; ++i)
    r = negamax<int,false>(b);
  printf("Negamax:        Result=%i  nodes=%i\n", r, nodes_searched);

  return 0;

  retrograde();
  printf("Retrograde analysis: %i passes (* 19683 = %i)\n",
         num_passes, num_passes*(19683));
  validate_retro();

  test(b);
  for (int i=0; i<9; ++i) {
    if ((i%3)<(i/3)) continue;
    b.MakeMove(i);
    test(b);
    b.UnMakeMove(i);
  }
  for (int i=0; i<9; ++i) {
    if ((i%3)<(i/3)) continue;
    b.MakeMove(i);
    for (int j=0; j<9; ++j) {
      if (i!=j) {
        b.MakeMove(j);
        test(b);
        b.UnMakeMove(j);
      }
    }
    b.UnMakeMove(i);
  }
  return 0;
}
