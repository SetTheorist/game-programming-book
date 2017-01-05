
#include "eval.h"
#include "gen-search.h"
#include "util.h"

namespace jetan {
enum colort {
  none=0, white=1, black=2
};
enum piecet {
  empty=0, chief, princess, flier, dwar, padwar, warrior, thoat, panthan
};
struct movet {
  union {
    int i;
    struct {
      unsigned w  : 1;  //  0     white?
      unsigned f  : 7;  //  1- 7  from loc
      unsigned t  : 7;  //  8-14  to loc
      unsigned p  : 4;  // 15-18  piece
      unsigned fc : 1;  // 19     capture?
      unsigned cp : 4;  // 20-23  captured piece
      unsigned steps : 6; // 24-29  steps path (where relevant) [OR make 64-bit move with explicit squares???]
    };
  };
  movet(int i_=0) : i(i_) { }
  int showf(FILE* ff) const {
  }
};

struct board {
  enum { f_princess_escape_w=1, f_princess_escape_b=2 };

  colort  colors[10*10];
  piecet  pieces[10*10];

  colort  to_move;
  int     is_terminal;
  colort  result;
  int     flags;

  int   num_moves;
  movet move_stack[256];
  int   flag_stack[256];

  int gen_moves(movet ml[]) const {
    // hard part is reducing # of moves by combining equivalent moves
    // (e.g. chief - only final location matters, but path is important to find reachable squares...
  }

  int chief_moves(movet ml[], int ff) const {
    static const int ds[8] = {-1, +7-1, +7, +7+1, +1, -7+1, -7, -7-1};
    static const int checked[9] = {3*7+3, 3*7+3-1, 3*7+3+7-1, 3*7+3+7, 3*7+3+7+1, 3*7+3+1, 3*7+3-7+1, 3*7+3-7, 3*7+3-7-1};
    int reachability[7*7];
    memset(reachability, '\x00', sizeof(reachability));
    reachability[3*7+3] = 1;
    int fr=ff/10, fc=ff%10;
    for (int rr=0; rr<7; ++rr)
      for (int cc=0; cc<7; ++cc)
        if (fr+rr-3<0 || fr+rr-3>=10 || fc+cc-3<0 || fc+cc-3>=10)
          reachability[rr*7+cc] = -1;
  }
};

}


int main(int argc, char* argv[]) {
  return 0;
}
