
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long long hasht;

namespace tori_shogi {

enum piecet {
  empty, phoenix, crane, falcon, eagle, pheasant, swallow, goose, quail_l, quail_r,
  NPIECE
};
enum colort {
  none, white, black,
  NCOLOR
};

static const char* kanji[] = {
  "   ", "X鵬", "C鶴", "F鷹", "E鵰", "P雉", "S燕", "G鴈", "l鶉", "r鶉"
};

struct movet {
  union {
    int i;
    struct {
      unsigned df : 1; // drop?
      unsigned cf : 1; // capture?
      unsigned pf : 1; // promote?
      unsigned wf : 1; // white?
      unsigned fl : 6; // from loc / hand slot
      unsigned tl : 6; // to loc
      unsigned mp : 4; // piece
      unsigned cp : 4; // captured piece
    };
  };
};

struct board {
  static colort init_color[7*7];
  static piecet init_piece[7*7];

  colort  color[7*7];
  piecet  piece[7*7];
  int     num_hand[NCOLOR];
  piecet  hand[NCOLOR][32];

  colort  to_move;
  int     is_terminal;
  colort  result;

  int     num_moves;
  movet   move_stack[256];
  hasht   hash_stack[256];

  hasht   piece_hash[7*7][NCOLOR][NPIECE];
  hasht   to_move_hash[NCOLOR];

  board() {
    init_hash();
    memcpy(color, init_color, sizeof(color));
    memcpy(piece, init_piece, sizeof(piece));
    memset(num_hand, '\x00', sizeof(num_hand));
    memset(hand, '\x00', sizeof(hand));
    to_move = white;
    is_terminal = 0;
    result = none;
    num_moves = 0;
    hash_stack[num_moves] = compute_hash();
  }

  hasht   hash() const { return hash_stack[num_moves]; }
  hasht   compute_hash() const {
  }
  void    init_hash() {
    for (int i=0; i<7*7; ++i)
      for (j=0; j<NCOLOR; ++j)
        for (k=0; k<NPIECE; ++k)
          piece_hash[i][j][k] = gen_hash();
    for (int i=0; i<NCOLOR; ++i)
      to_move_hash[i] = gen_hash();
  }

  int gen_moves(movet ml[]) {
    int n = 0;
    for (int i=0; i<num_hand[to_move]; ++i) {
      if (hand[to_move][i] == swallow) {
        // TODO: forbid dropping swallows for mate
        // TODO: don't allow dropping in final row
        // TODO: don't allow dropping with 2 already
      } else {
        for (int j=0; j<7*7; ++j) {
          if (color[j]==none) {
            ADD_DROP(
          }
        }
      }
    }
    for (int i=0; i<7*7; ++i) {
      if (color[i]==to_move) {
        switch (piece[i]) {
          default: break;
          case phoenix:
            break;
          case crane:
            break;
          case falcon:
            break;
          case eagle:
            break;
          case pheasant:
            break;
          case swallow:
            break;
          case goose:
            break;
          case quail_l:
            break;
          case quail_r:
            break;
        }
      }
    }
    return n;
  }
  int make_move(movet m) {
    return -1;
  }
  int unmake_move() {
    return -1;
  }
  int show(FILE* ff) {
    for (int r=6; r>=0; --r) {
      fprintf(ff, " | ");
      for (int c=0; c<7; ++c) {
        fprintf(ff, "%c %s ", " wb"[color[r*7+c]], kanji[piece[r*7+c]]);
      }
      fprintf(ff, "| ");
      if (r==5 || r==4) {
        fprintf(ff, "%c:", "WB"[r-4]);
        for (int i=0; i<num_hand[r-3]; ++i)
          fprintf(ff, " %s", kanji[hand[r-3][i]]);
      }
      fprintf(ff, "\n");
    }
  }
};
colort board::init_color[7*7] = {
  white, white, white, white, white, white, white, 
  none,  none,  none,  white, none,  none,  none,
  white, white, white, white, white, white, white, 
  none,  black, none,  none,  none,  white, none,
  black, black, black, black, black, black, black, 
  none,  none,  none,  black, none,  none,  none,
  black, black, black, black, black, black, black, 
};
piecet board::init_piece[7*7] = {
  quail_l, pheasant, crane, phoenix, crane, pheasant, quail_r,
  empty, empty, empty, falcon, empty, empty, empty,
  swallow, swallow, swallow, swallow, swallow, swallow, swallow,
  empty, swallow, empty, empty, empty, swallow, empty,
  swallow, swallow, swallow, swallow, swallow, swallow, swallow,
  empty, empty, empty, falcon, empty, empty, empty,
  quail_r, pheasant, crane, phoenix, crane, pheasant, quail_l,
};

struct game {
  typedef tori_shogi::board   boardt;
  typedef tori_shogi::movet   movet;
  typedef i_evalt       evalt;
};

}

int main(int argc, char* argv[]) {
  tori_shogi::board b;
  b.hand[tori_shogi::white][b.num_hand[tori_shogi::white]++] = tori_shogi::crane;
  b.show(stdout);
  return 0;
}
