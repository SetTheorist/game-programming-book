/* $Id: tafl.c,v 1.7 2009/12/12 21:34:04 apollo Exp apollo $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "board.h"
#include "search.h"
#include "util.h"

static const int steps[4] = {+11, +1, -11, -1};
static const int bsteps[4] = {+9, +1, -9, -1};

static const int squares[81] = {
  crn, cnt, cnt, frt, frt, frt, cnt, cnt, crn,
  cnt, cnt, cnt, cnt, frt, cnt, cnt, cnt, cnt,
  cnt, cnt, cnt, cnt, cnt, cnt, cnt, cnt, cnt,

  frt, cnt, cnt, cnt, cnt, cnt, cnt, cnt, frt,
  frt, frt, cnt, cnt, kng, cnt, cnt, frt, frt,
  frt, cnt, cnt, cnt, cnt, cnt, cnt, cnt, frt,

  cnt, cnt, cnt, cnt, cnt, cnt, cnt, cnt, cnt,
  cnt, cnt, cnt, cnt, frt, cnt, cnt, cnt, cnt,
  crn, cnt, cnt, frt, frt, frt, cnt, cnt, crn,
};
static const piecet init_pieces[81] = {
  empty, empty, empty,  wman,  wman,  wman, empty, empty, empty,
  empty, empty, empty, empty,  wman, empty, empty, empty, empty,
  empty, empty, empty, empty,  bman, empty, empty, empty, empty,

   wman, empty, empty, empty,  bman, empty, empty, empty,  wman,
   wman,  wman,  bman,  bman, bking,  bman,  bman,  wman,  wman,
   wman, empty, empty, empty,  bman, empty, empty, empty,  wman,

  empty, empty, empty, empty,  bman, empty, empty, empty, empty,
  empty, empty, empty, empty,  wman, empty, empty, empty, empty,
  empty, empty, empty,  wman,  wman,  wman, empty, empty, empty,
};
static const int board_to_box[81] = {
  12, 13, 14, 15, 16, 17, 18, 19, 20,
  23, 24, 25, 26, 27, 28, 29, 30, 31,
  34, 35, 36, 37, 38, 39, 40, 41, 42,
  45, 46, 47, 48, 49, 50, 51, 52, 53,
  56, 57, 58, 59, 60, 61, 62, 63, 64,
  67, 68, 69, 70, 71, 72, 73, 74, 75,
  78, 79, 80, 81, 82, 83, 84, 85, 86,
  89, 90, 91, 92, 93, 94, 95, 96, 97,
  100,101,102,103,104,105,106,107,108
};
static const int box_to_board[121] = {
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1,  0,  1,  2,  3,  4,  5,  6,  7,  8, -1,
  -1,  9, 10, 11, 12, 13, 14, 15, 16, 17, -1,
  -1, 18, 19, 20, 21, 22, 23, 24, 25, 26, -1,
  -1, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1,
  -1, 36, 37, 38, 39, 40, 41, 42, 43, 44, -1,
  -1, 45, 46, 47, 48, 49, 50, 51, 52, 53, -1,
  -1, 54, 55, 56, 57, 58, 59, 60, 61, 62, -1,
  -1, 63, 64, 65, 66, 67, 68, 69, 70, 71, -1,
  -1, 72, 73, 74, 75, 76, 77, 78, 79, 80, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static hasht hash_values[81][3];
static hasht hash_to_move;

void init_hash() {
  int  i, j;
  for (i=0; i<81; ++i)
    for (j=0; j<3; ++j)
      hash_values[i][j] = gen_hash();
  hash_to_move = gen_hash();
}

hasht hash_board(const board* b) {
  int  i;
  hasht  hash = b->side==white ? hash_to_move : 0;
  for (i=0; i<81; ++i) {
    if (b->pieces[i]!=empty)
      hash = hash ^ hash_values[i][b->pieces[i]];
  }
  ((board*)b)->hash = hash;
  return hash;
}

int setup_board(board* b) {
  memset(b, '\x00', sizeof(board));
  memcpy(b->pieces, init_pieces, sizeof(init_pieces));
  b->side = white;
  b->xside = black;
  b->ply = 0;
  b->bking_loc = 40;
  b->hash_hist[b->ply] = hash_board(b);
  b->n_wman = 16;
  b->n_bman = 8;
  return 0;
}

int show_board(FILE* f, const board* b) {
  int  r, c;
  fprintf(f, " abcdefghi \n");
  fprintf(f, "+---------+   [%016llX]\n", hash_board(b));
  for (r=8; r>=0; --r) {
    fprintf(f, "|");
    for (c=0; c<9; ++c) {
      if (b->pieces[r*9+c]!=empty) {
        fprintf(f, "%c", "Wbk"[b->pieces[r*9+c]]);
      } else {
        fprintf(f, "%c", ".=#*"[squares[r*9+c]]);
      }
    }
    fprintf(f, "| %i", r+1);
    //fprintf(f, "          ");
    //for (c=0; c<9; ++c) fprintf(f, " %2i", b->grid[r*9+c]);
    fprintf(f, "\n");
  }
  fprintf(f, "+---------+  ");
  if (b->terminal) {
    if (b->result==white)
      fprintf(f, " ** White wins **\n");
    else if (b->result==black)
      fprintf(f, " ** Black wins **\n");
    else if (b->result==none)
      fprintf(f, " ** Draw **\n");
  } else if (b->side==white)
    fprintf(f, " White to move\n");
  else if (b->side==black)
    fprintf(f, " Black to move\n");
  return 0;
}

int show_move(FILE* f, movet m) {
  fprintf(f, " %c%i-%c%i",
    'a'+m.from%9, 1+m.from/9,
    'a'+m.to%9, 1+m.to/9);
  if (m.caps) {
    int  dir;
    for (dir=0; dir<4; ++dir)
      if (m.caps&(1<<dir))
        fprintf(f, "x%c", "NEWS"[dir]);
    for (dir=4; dir<8; ++dir)
      if (m.caps&(1<<dir))
        fprintf(f, "x%c!", "NEWS"[dir-4]);
  }
  if (m.flags & w_win)
    fprintf(f, "(1-0)");
  else if (m.flags & b_win)
    fprintf(f, "(0-1)");
  else if (m.flags & w_draw)
    fprintf(f, "(W:1/2)");
  else if (m.flags & b_draw)
    fprintf(f, "(B:1/2)");
  return 0;
}

movet null_move() {
  movet m = {0, 0, 0, nullmove};
  return m;
}
int gen_moves(const board* b, movet* ml) {
  int  f, t, n=0, dir, xdir, ncaps;

  for (f=0; f<81; ++f) {
    if ((b->pieces[f]==wman) && (b->side==white)) {
      for (dir=0; dir<4; ++dir) {
        int  fort = (squares[f]==frt);
        int  step = steps[dir];
        for(t=box_to_board[board_to_box[f]+step];t!=-1;t=box_to_board[board_to_box[t]+step]) {
          if (b->pieces[t]!=empty) break;
          if (!(squares[t]==cnt)&&!((squares[t]==frt)&&fort)) break;
          ml[n].from = f;
          ml[n].to = t;
          ml[n].caps = 0;
          /* captures */
          if (squares[t]!=frt) for (xdir=0; xdir<4; ++xdir) {
            int  xx=box_to_board[board_to_box[t]+steps[xdir]];
            if (xx==-1) continue;
            int  x2=box_to_board[board_to_box[xx]+steps[xdir]];
            if (x2==-1) continue;
            /* captures */
            if (((b->pieces[xx]==bman)&&(squares[xx]!=kng))
              && ((squares[x2]==frt)||(squares[x2]==kng)||(squares[x2]==crn)||(b->pieces[x2]==wman)))
              ml[n].caps |= 1<<xdir;
            /* king captures */
            if ((b->pieces[xx]==bking)&&(squares[xx]!=kng)) {
              int  xxdir, cnt=1;
              for (xxdir=0; xxdir<4; ++xxdir) {
                int  x3;
                if ((xxdir^2)!=xdir) {
                  x3=box_to_board[board_to_box[xx]+steps[xxdir]];
                  if ((x3==-1)||(squares[x3]==frt)||(squares[x3]==kng)||(b->pieces[x3]==wman))
                    ++cnt;
                }
              }
              if (cnt==4)
                ml[n].caps |= 1<<(4+xdir);
            }
          }
          ml[n].flags = (ml[n].caps&(0xF0)) ? w_win : 0;
          ++n;
          fort = fort && (squares[t]==frt);
        }
      }
    } else if ((b->pieces[f]==bman) && (b->side==black)) {
      for (dir=0; dir<4; ++dir) {
        int  step = steps[dir];
        for(t=box_to_board[board_to_box[f]+step];t!=-1;t=box_to_board[board_to_box[t]+step]) {
          if (b->pieces[t]!=empty) break;
          if (!(squares[t]==cnt)) break;
          ml[n].from = f;
          ml[n].to = t;
          ml[n].caps = 0;
          ncaps = 0;
          for (xdir=0; xdir<4; ++xdir) {
            int  xx=box_to_board[board_to_box[t]+steps[xdir]];
            if (xx==-1) continue;
            int  x2=box_to_board[board_to_box[xx]+steps[xdir]];
            if (x2==-1) continue;
            if (((b->pieces[xx]==wman)&&(squares[xx]!=frt))
              && ((squares[x2]==frt)||(squares[x2]==kng)||(squares[x2]==crn)
                  ||(b->pieces[x2]==bman)||(b->pieces[x2]==bking))) {
              ml[n].caps |= 1<<xdir;
              ++ncaps;
            }
          }
          ml[n].flags = (ncaps==b->n_wman) ? b_win : 0;
          ++n;
        }
      }
    } else if ((b->pieces[f]==bking) && (b->side==black)) {
      for (dir=0; dir<4; ++dir) {
        int  step = steps[dir];
        for(t=box_to_board[board_to_box[f]+step];t!=-1;t=box_to_board[board_to_box[t]+step]) {
          if (b->pieces[t]!=empty) break;
          if (!(squares[t]==cnt)&&!(squares[t]==crn)) break;
          ml[n].from = f;
          ml[n].to = t;
          ml[n].caps = 0;
          for (xdir=0; xdir<4; ++xdir) {
            int  xx=box_to_board[board_to_box[t]+steps[xdir]];
            if (xx==-1) continue;
            int  x2=box_to_board[board_to_box[xx]+steps[xdir]];
            if (x2==-1) continue;
            if ((b->pieces[xx]==wman)
              && ((squares[x2]==frt)||(squares[x2]==crn)||(squares[x2]==kng)
                  ||(b->pieces[x2]==bman)||(b->pieces[x2]==bking)))
              ml[n].caps |= 1<<xdir;
          }
          ml[n].flags = squares[t]==crn ? b_win : 0;
          ++n;
        }
      }
    }
  }

  return n;
}
int gen_moves_cap(const board* b, movet* ml) {
  int  f, t, n=0, dir, xdir, ncaps;

  for (f=0; f<81; ++f) {
    if ((b->pieces[f]==wman) && (b->side==white)) {
      for (dir=0; dir<4; ++dir) {
        int  fort = (squares[f]==frt);
        int  step = steps[dir];
        for(t=box_to_board[board_to_box[f]+step];t!=-1;t=box_to_board[board_to_box[t]+step]) {
          if (b->pieces[t]!=empty) break;
          if (!(squares[t]==cnt)&&!((squares[t]==frt)&&fort)) break;
          ml[n].from = f;
          ml[n].to = t;
          ml[n].caps = 0;
          /* captures */
          if (squares[t]!=frt) for (xdir=0; xdir<4; ++xdir) {
            int  xx=box_to_board[board_to_box[t]+steps[xdir]];
            if (xx==-1) continue;
            int  x2=box_to_board[board_to_box[xx]+steps[xdir]];
            if (x2==-1) continue;
            /* captures */
            if (((b->pieces[xx]==bman)&&(squares[xx]!=kng))
              && ((squares[x2]==frt)||(squares[x2]==kng)||(squares[x2]==crn)||(b->pieces[x2]==wman)))
              ml[n].caps |= 1<<xdir;
            /* king captures */
            if ((b->pieces[xx]==bking)&&(squares[xx]!=kng)) {
              int  xxdir, cnt=1;
              for (xxdir=0; xxdir<4; ++xxdir) {
                int  x3;
                if ((xxdir^2)!=xdir) {
                  x3=box_to_board[board_to_box[xx]+steps[xxdir]];
                  if ((x3==-1)||(squares[x3]==frt)||(squares[x3]==kng)||(b->pieces[x3]==wman))
                    ++cnt;
                }
              }
              if (cnt==4)
                ml[n].caps |= 1<<(4+xdir);
            }
          }
          ml[n].flags = (ml[n].caps&(0xF0)) ? w_win : 0;
          if (ml[n].caps) ++n;
          fort = fort && (squares[t]==frt);
        }
      }
    } else if ((b->pieces[f]==bman) && (b->side==black)) {
      for (dir=0; dir<4; ++dir) {
        int  step = steps[dir];
        for(t=box_to_board[board_to_box[f]+step];t!=-1;t=box_to_board[board_to_box[t]+step]) {
          if (b->pieces[t]!=empty) break;
          if (!(squares[t]==cnt)) break;
          ml[n].from = f;
          ml[n].to = t;
          ml[n].caps = 0;
          ncaps = 0;
          for (xdir=0; xdir<4; ++xdir) {
            int  xx=box_to_board[board_to_box[t]+steps[xdir]];
            if (xx==-1) continue;
            int  x2=box_to_board[board_to_box[xx]+steps[xdir]];
            if (x2==-1) continue;
            if (((b->pieces[xx]==wman)&&(squares[xx]!=frt))
              && ((squares[x2]==frt)||(squares[x2]==kng)||(squares[x2]==crn)
                  ||(b->pieces[x2]==bman)||(b->pieces[x2]==bking))) {
              ml[n].caps |= 1<<xdir;
              ++ncaps;
            }
          }
          ml[n].flags = (ncaps==b->n_wman) ? b_win : 0;
          if (ml[n].caps) ++n;
        }
      }
    } else if ((b->pieces[f]==bking) && (b->side==black)) {
      for (dir=0; dir<4; ++dir) {
        int  step = steps[dir];
        for(t=box_to_board[board_to_box[f]+step];t!=-1;t=box_to_board[board_to_box[t]+step]) {
          if (b->pieces[t]!=empty) break;
          if (!(squares[t]==cnt)&&!(squares[t]==crn)) break;
          ml[n].from = f;
          ml[n].to = t;
          ml[n].caps = 0;
          for (xdir=0; xdir<4; ++xdir) {
            int  xx=box_to_board[board_to_box[t]+steps[xdir]];
            if (xx==-1) continue;
            int  x2=box_to_board[board_to_box[xx]+steps[xdir]];
            if (x2==-1) continue;
            if ((b->pieces[xx]==wman)
              && ((squares[x2]==frt)||(squares[x2]==crn)||(squares[x2]==kng)
                  ||(b->pieces[x2]==bman)||(b->pieces[x2]==bking)))
              ml[n].caps |= 1<<xdir;
          }
          ml[n].flags = squares[t]==crn ? b_win : 0;
          if (ml[n].caps) ++n;
        }
      }
    }
  }

  return n;
}


int count_moves(const board* b) {
  int  f, t, n=0, dir;

  for (f=0; f<81; ++f) {
    if ((b->pieces[f]==wman) && (b->side==white)) {
      for (dir=0; dir<4; ++dir) {
        int  fort = (squares[f]==frt);
        int  step = steps[dir];
        for(t=box_to_board[board_to_box[f]+step];t!=-1;t=box_to_board[board_to_box[t]+step]) {
          if (b->pieces[t]!=empty) break;
          if (!(squares[t]==cnt)&&!((squares[t]==frt)&&fort)) break;
          ++n;
          fort = fort && (squares[t]==frt);
        }
      }
    } else if ((b->pieces[f]==bman) && (b->side==black)) {
      for (dir=0; dir<4; ++dir) {
        int  step = steps[dir];
        for(t=box_to_board[board_to_box[f]+step];t!=-1;t=box_to_board[board_to_box[t]+step]) {
          if (b->pieces[t]!=empty) break;
          if (!(squares[t]==cnt)) break;
          ++n;
        }
      }
    } else if ((b->pieces[f]==bking) && (b->side==black)) {
      for (dir=0; dir<4; ++dir) {
        int  step = steps[dir];
        for(t=box_to_board[board_to_box[f]+step];t!=-1;t=box_to_board[board_to_box[t]+step]) {
          if (b->pieces[t]!=empty) break;
          if (!(squares[t]==cnt)&&!(squares[t]==crn)) break;
          ++n;
        }
      }
    }
  }

  return n;
}

/* NB: only checks if last board position is 3rd-time repeat */
int check3rep(board* b) {
  int  i, c=1;
  hasht  h = b->hash_hist[b->ply-1];
  for (i=0; i<b->ply-1; ++i)
    if (b->hash_hist[i]==h)
      ++c;
  return c>=3;
}

int validate_board(const board* b) {
  return validate_board_chp(b, "");
}
int validate_board_chp(const board* b, const char* chp) {
  int  nw=0, nb=0, i;
  for (i=0; i<81; ++i) {
    if (b->pieces[i]==wman) ++nw;
    else if (b->pieces[i]==bman) ++nb;
    else if (b->pieces[i]==bking) { if (b->bking_loc!=i) printf("<BKing>"); }
  }
  if (nw!=b->n_wman) {
    printf("<%s:WMan(n:%i)!=#:%i>", chp, b->n_wman, nw);
  }
  if (nb!=b->n_bman) {
    printf("<%s:BMan(n:%i)!=#:%i>", chp, b->n_bman, nb);
  }
  return 0;
}

int make_move(board* b, movet m) {
  if (b->terminal) printf("! TERMINAL BOARD IN MAKE_MOVE !\n");
  if (m.flags & nullmove) {
    if (b->side==white) {
      b->side = black;
      b->xside = white;
    } else if (b->side==black) {
      b->side = white;
      b->xside = black;
    }
    b->hash_hist[b->ply] = hash_board(b);
    b->hist[b->ply] = m;
    ++b->ply;
    return 0;
  }

  if (b->side==white) {
    if (b->pieces[m.from]!=wman) {
      printf("<WTM %i!?:", b->pieces[m.from]); show_move(stdout, m); printf("-------------------->\n");
      show_board(stdout, b);
      printf("<---------------------------------------->\n");
    }
  } else if (b->side==black) {
    if ((b->pieces[m.from]!=bman)&&(b->pieces[m.from]!=bking)) {
      printf("<BTM %i!?:", b->pieces[m.from]); show_move(stdout, m); printf("-------------------->\n");
      show_board(stdout, b);
      printf("<---------------------------------------->\n");
    }
  }

  b->pieces[m.to] = b->pieces[m.from];
  b->pieces[m.from] = empty;
  if (b->pieces[m.to]==bking) { b->bking_loc=m.to; }
  if (m.caps) {
    int d;
    for (d=0; d<4; ++d)
      if (m.caps & (1<<d))
      {
        b->pieces[m.to + bsteps[d]] = empty;
        if (b->side==white)
          --b->n_bman;
        else
          --b->n_wman;
      }
    for (d=0; d<4; ++d)
      if (m.caps & (1<<(4+d)))
        b->pieces[m.to + bsteps[d]] = empty;
  }

  b->hash_hist[b->ply] = hash_board(b);
  b->hist[b->ply] = m;
  ++b->ply;

  if (check3rep(b)) {
    //printf("===3===\n");
    if (b->side==white)
      m.flags |= w_draw;
    else
      m.flags |= b_draw;
    b->hist[b->ply-1] = m;
  }

  if (m.flags&w_win) {
    b->terminal = 1;
    b->result = white;
  } else if (m.flags&b_win) {
    b->terminal = 1;
    b->result = black;
  } else if (m.flags&draw) {
    b->terminal = 1;
    b->result = none;
  }
  if (b->side==white) {
    b->side = black;
    b->xside = white;
  } else if (b->side==black) {
    b->side = white;
    b->xside = black;
  }
  /* TODO: BUG? HASH FOR TO-MOVE?!?! */

  if (VALIDATE_MOVE) validate_board_chp(b, "make_move");

  return 0;
}

static int unmake(board* b, movet m) {
  colort  moved = (b->side==black) ? white : black;
  b->terminal = 0;
  b->pieces[m.from] = b->pieces[m.to];
  b->pieces[m.to] = empty;
  if (b->pieces[m.from]==bking) { b->bking_loc=m.from; }
  if (m.caps) {
    int d;
    for (d=0; d<4; ++d)
      if (m.caps & (1<<d))
      {
        b->pieces[m.to + bsteps[d]] = moved==white ? bman : wman;
        if (moved==black)
          ++b->n_wman;
        else
          ++b->n_bman;
      }
    for (d=0; d<4; ++d)
      if (m.caps & (1<<(4+d)))
        b->pieces[m.to + bsteps[d]] = bking;
  }
  if (moved==white) {
    b->side = white;
    b->xside = black;
  } else if (moved==black) {
    b->side = black;
    b->xside = white;
  } else {
    /* TODO */
  }
  --b->ply;
  hash_board(b);
  if (VALIDATE_MOVE) validate_board_chp(b, "unmake_move");
  return 0;
}

inline int unmake_move(board* b) {
  return unmake(b, b->hist[b->ply-1]);
}

static int grid_init(board* b) {
  int  i;
  for (i=0; i<81; ++i) {
    b->grid[i] = squares[i]==crn ? 0 : 99;
  }
  return 0;
}
static int grid_iter(board* b, int n) {
  int  f, t, dir, changed=0;
  for (f=0; f<81; ++f) {
    if (b->grid[f]==n) {
      for (dir=0; dir<4; ++dir) {
        for (t=box_to_board[board_to_box[f]+steps[dir]]; (t!=-1); t=box_to_board[board_to_box[t]+steps[dir]])
        {
          if (!((b->pieces[t]==empty)||(b->pieces[t]==bking))) break;
            if (squares[t]!=cnt) break;
          if (b->grid[f]+1<b->grid[t])
          {
            b->grid[t] = b->grid[f]+1;
            changed = 1;
          }
        }
      }
    }
  }
  return changed;
}
static int grid_spread(board* b) {
  int  count=0;
  grid_init(b);
  while (grid_iter(b,count))
    ++count;
  return count;
}

static const int piece_value[3] = {100,-50,-1200};
#define  white_mob_value  0
#define  black_mob_value  1

int terminal(const board* b) {
  return b->terminal;
}

int evaluate_relative(const evaluator* e, const board* b, int ply) {
  return b->side==white ? evaluate(e,b,ply) : -evaluate(e,b,ply);
}
int evaluate(const evaluator* e, const board* b_, int ply) {
  board* b = (board*)b_;
  if (b->terminal) {
    if (b->result==white) return 1000000 - 10*ply;
    else if (b->result==black) return -1000000 + 10*ply;
    else if (b->result==none) return 0;
  }

  int  sum = piece_value[bking] + piece_value[wman]*(b->n_wman) + piece_value[bman]*(b->n_bman);

  grid_spread(b);
  sum -= (99 / b->grid[b->bking_loc]) - 1;

  if (e->use_mobility) {
    colort save_side = b->side;
    b->side = white;
    sum += white_mob_value * count_moves(b);
    b->side = black;
    sum -= black_mob_value * count_moves(b);
    b->side = save_side;
  }

  return sum;
}

int simple_evaluator(evaluator* e) {
  memset(e, '\x00', sizeof(evaluator));
  e->use_mobility = 1;
  return 0;
}
int evaluate_moves_for_search(const evaluator* e, const board* b,
                              const movet* ml, evalt* vl, int nm) {
  memset(vl, '\x00', sizeof(evalt)*nm);
  return 0;
}
int evaluate_moves_for_quiescence(const evaluator* e, const board* b,
                                  const movet* ml, evalt* vl, int nm) {
  memset(vl, '\x00', sizeof(evalt)*nm);
  return 0;
}
