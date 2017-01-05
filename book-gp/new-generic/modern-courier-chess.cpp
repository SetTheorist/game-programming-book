#include <stdio.h>
#include <stdlib.h>
#include <string.h>



enum colort {
  none=0, white=1, black=2, NCOLOR, border=3
};
enum piecet {
  empty=0, pawn, fers, courier, knight, bishop, rook, queen, king,
  NPIECE
};

struct movet {
  union {
    int i;
    struct {
      // null-move has f==t
      unsigned w : 1; //  0       white-to-move?
      unsigned p : 4; //  1-- 4   moving-piece
      unsigned f : 7; //  5--11   from-square
      unsigned t : 7; // 12--18   to-square
      unsigned c : 4; // 19--22   captured-piece
      unsigned p : 4; // 23--27   promoted-to-piece
      unsigned d : 1; // 28       double-move? (king/ferz)
      unsigned x : 4; // 29--31   <unused>
    };
  };
};

// TODO: maybe better to use piece-list (a la chu-shogi)?
struct boardt {
  static colort  init_colors[8*12];
  static piecet  init_pieces[8*12];

  colort  colors[8*12];
  piecet  pieces[8*12];

  int     num_moves;
  movet   move_stack[1024];
  hasht   hash_stack[1024];
};
