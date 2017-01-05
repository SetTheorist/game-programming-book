#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef unsigned char uint8;
typedef unsigned int uint32;
typedef unsigned int uint;

enum Side {
  s_none = 0,
  s_white,
  s_black,
};

enum Piece {
  p_empty = 0,
  p_king,
  p_gold,
  p_silver,
  p_rook,
  p_bishop,
  p_pawn,
  p_promoted_silver,
  p_dragon,
  p_horse,
  p_tokin,
};
Piece promotion_table[11] = {
  p_empty, p_empty, p_empty, p_promoted_silver, p_dragon,
  p_horse, p_tokin, p_empty, p_empty, p_empty,
  p_empty,
};

union Move {
  uint32 i;
  struct {
    uint from_r : 4;
    uint from_c : 4;
    uint to_r : 4;
    uint to_c : 4;
    uint cap : 4;
    uint promotion_from : 4;
    uint promotion_to : 4;
  };
};

union Square {
  uint32 i;
  struct {
    uint8 side;
    uint8 piece;
  };
};

struct Board {
  Square squares[5][5];
  int held[2][7];
  Side to_move;
  Move move_stack[256];
  int num_moves;

  Board() {
    Initialize();
  }
  void Initialize() {
    memset(this, '\x00', sizeof(*this));
    to_move = s_white;
    squares[0][0].side = s_white;
    squares[0][1].side = s_white;
    squares[0][2].side = s_white;
    squares[0][3].side = s_white;
    squares[0][4].side = s_white;
    squares[1][0].side = s_white;
    squares[0][0].piece = p_king;
    squares[0][1].piece = p_gold;
    squares[0][2].piece = p_silver;
    squares[0][3].piece = p_bishop;
    squares[0][4].piece = p_rook;
    squares[1][0].piece = p_pawn;
    squares[4][0].side = s_black;
    squares[4][1].side = s_black;
    squares[4][2].side = s_black;
    squares[4][3].side = s_black;
    squares[4][4].side = s_black;
    squares[3][4].side = s_black;
    squares[4][4].piece = p_king;
    squares[4][3].piece = p_gold;
    squares[4][2].piece = p_silver;
    squares[4][1].piece = p_bishop;
    squares[4][0].piece = p_rook;
    squares[3][4].piece = p_pawn;
  }
  void Display() {
    static const char* piece_names[3][11] = {
      {"   ", " k ", " g ", " s ", " r ", " b ", " p ", " s*", " r*", " b*", " p*"},
      {"W..", "WK ", "WG ", "WS ", "WR ", "WB ", "WP ", "WS+", "WR+", "WB+", "WP+"},
      {"b..", "bk ", "bg ", "bs ", "br ", "bb ", "bp ", "bS+", "bR+", "bb+", "bp+"},
    };
    printf("+---+---+---+---+---+\n");
    for (int r=4; r>=0; --r) {
      printf("|");
      for (int c=0; c<=4; ++c) {
        printf("%s|",
            piece_names[squares[r][c].side][squares[r][c].piece]);
      }
      printf("\n+---+---+---+---+---+\n");
    }
  }
  inline bool AddPseudoMove(int from_r, int from_c, int to_r, int to_c, Move move_list[], int& n) {
    if ((to_r >= 0) && (to_r < 5) && (to_c >= 0) && (to_c < 5)
        && squares[to_r][to_c].side != to_move) {
      Move move;
      move.i = 0;
      move.from_r = from_r;
      move.from_c = from_c;
      move.to_r = to_r;
      move.to_c = to_c;
      if (squares[to_r][to_c].side != s_none)
        move.cap = squares[move.to_r][move.to_c].piece;
      move_list[n++] = move;
      if (((to_move==s_white) && (to_r==4)) || ((to_move==s_black) && (to_r==0))) {
        move.promotion_from = squares[from_r][from_c].piece;
        move.promotion_to = promotion_table[move.promotion_from];
        move_list[n++] = move;
      }
      return true;
    }
    return false;
  }
  int GenerateMoves(Move move_list[]) {
    int n = 0;
    for (int r=0; r<5; ++r)
    {
      for (int c=0; c<5; ++c)
      {
        if (squares[r][c].side == to_move) {
          switch (squares[r][c].piece) {
            case p_king:
              AddPseudoMove(r, c, r+1, c-1, move_list, n);
              AddPseudoMove(r, c, r+1, c+1, move_list, n);
              AddPseudoMove(r, c, r-1, c+1, move_list, n);
              AddPseudoMove(r, c, r-1, c-1, move_list, n);
              AddPseudoMove(r, c, r+1, c  , move_list, n);
              AddPseudoMove(r, c, r-1, c  , move_list, n);
              AddPseudoMove(r, c, r  , c+1, move_list, n);
              AddPseudoMove(r, c, r  , c-1, move_list, n);
              break;
            case p_gold:
            case p_tokin:
            case p_promoted_silver:
              break;
            case p_silver:
              break;
            case p_dragon:
            case p_rook:
              break;
            case p_horse:
              AddPseudoMove(r, c, r+1, c  , move_list, n);
              AddPseudoMove(r, c, r-1, c  , move_list, n);
              AddPseudoMove(r, c, r  , c+1, move_list, n);
              AddPseudoMove(r, c, r  , c-1, move_list, n);
            case p_bishop:
              break;
            case p_pawn:
              AddPseudoMove(r, c, r+(to_move==s_white ? +1 : -1), c, move_list, n);
              break;
            default:
              break;
          }
        }
      }
    }
    return n;
  };
};

int main(int argc, const char* argv[]) {
  Board b;
  b.squares[3][0].side = s_white;
  b.squares[3][0].piece = p_pawn;
  b.Display();
  Move  move_list[256];
  int n;
  n = b.GenerateMoves(move_list);
  printf("n = %i\n", n);
  for (int i=0; i<n; ++i) {
    printf("(%i,%i)->(%i,%i)", move_list[i].from_r, move_list[i].from_c, move_list[i].to_r, move_list[i].to_c);
    if (move_list[i].cap) printf("x");
    if (move_list[i].promotion_to) printf("+");
    printf("\n");
  }
  return 0;
}
