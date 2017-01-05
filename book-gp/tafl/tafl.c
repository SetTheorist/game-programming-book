/* $Id: tafl.c,v 1.7 2009/12/12 21:34:04 apollo Exp apollo $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define  VALIDATE_MOVE  0
#define  VALIDATE_BOARD  0
#define  SHOW_MOVES    0

typedef unsigned char    uint8;
typedef unsigned short    uint16;
typedef unsigned int    uint32;
typedef unsigned long long  uint64;

/* a version of tafl/tablut:
 * 
 * (do bit-board version???)
 *
 * board is:
 *
 *  x..###..x
 *  ....#....
 *  .........
 *  #.......#
 *  ##..*..##
 *  #.......#
 *  .........
 *  ....#....
 *  x..###..x
 *
 * with starting positions:
 *
 *  x..WWW..x
 *  ....W....
 *  ....b....
 *  W...b...W
 *  WWbbkbbWW
 *  W...b...W
 *  ....b....
 *  ....W....
 *  x..WWW..x
 *
 * - pieces move like chess rooks
 * - goal is for black king 'k' to reach a corner square ('x')
 * - pieces are captured by "sandwiching between a piece and a wall ('x', "#', '*') or between 2 enemy pieces
 * - except that konakis '*' is friendly to black if king occupies
 * - king is captured if surrounded on 4 sides
 * - pieces cannot move into '#' squares but can move in it if already there
 * - none can move into '*' square
 * 
 *
 */

unsigned int read_clock(void) {
  struct timeval timeval;
  struct timezone timezone;
  gettimeofday(&timeval, &timezone);
  return (timeval.tv_sec * 1000000 + (timeval.tv_usec));
}

typedef enum colort {
  white, black, none
} colort;
typedef enum piecet {
  wman, bman, bking, empty
} piecet;
typedef enum squaret {
  cnt=0, frt=1, kng=2, crn=3
} squaret;

int steps[4] = {+11, +1, -11, -1};
int bsteps[4] = {+9, +1, -9, -1};

int squares[81] = {
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
piecet init_pieces[81] = {
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
int board_to_box[81] = {
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
int box_to_board[121] = {
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

typedef  uint64  hasht;

typedef enum flagst {
  w_win=1, b_win=2, w_draw=4, b_draw=8
} flagst;
#define  draw  (w_draw|b_draw)

typedef struct move {
  uint8  from;
  uint8  to;
  uint8  caps;
  uint8  flags;
} move;

typedef struct board {
  piecet  pieces[81];
  colort  side;
  colort  xside;
  int    bking_loc;
  int    n_wman;
  int    n_bman;
  hasht  hash;
  int    grid[81];
  int    ply;
  move  hist[1024];
  hasht  hash_hist[1024];
} board;

typedef enum ttent_flags {
  tt_valid=1, tt_exact=2, tt_upper=4, tt_lower=8
} ttent_flags;

typedef struct ttent {
  hasht  hash;
  int    score;
  int    flags;
  move  best_move;
  int    depth;
} ttent;

hasht  hash_values[81][3];
hasht  hash_to_move;

hasht gen_hash()
{
  hasht  h = 0;
  int  i;
  for (i=0; i<64; ++i)
    h = h ^ ((hasht)(lrand48())<<i) ^ ((hasht)(lrand48())>>i);
  return h;
}

int
init_hash()
{
  int  i, j;
  for (i=0; i<81; ++i)
    for (j=0; j<3; ++j)
      hash_values[i][j] = gen_hash();
  hash_to_move = gen_hash();
  return 0;
}

hasht
hash_board(board* b)
{
  int  i;
  hasht  hash = b->side==white ? hash_to_move : 0;
  for (i=0; i<81; ++i) {
    if (b->pieces[i]!=empty)
      hash = hash ^ hash_values[i][b->pieces[i]];
  }
  b->hash = hash;
  return hash;
}

#define  NTT  (1024*1024)
ttent  ttable[NTT];

int
init_ttable()
{
  memset(ttable, 0, sizeof(ttable));
  return 0;
}

inline int
put_ttable(hasht hash, int score, move m, int flags, int depth)
{
  int  loc = hash%NTT;
  if (loc<0) printf("!Error in add_ttable()!\n");
  /* most-recent: replaces without checking
   * deepest: replaces only if as deep or deeper
   * --- inital testing seems to say most-recent gives fewer nodes
   */
//  if (!(ttable[loc].flags&tt_valid) || (ttable[loc].depth>=depth))
  {
    ttable[loc].hash = hash;
    ttable[loc].score = score;
    ttable[loc].best_move = m;
    ttable[loc].flags = flags|tt_valid;
    ttable[loc].depth = depth;
  }
  return 0;
}
inline ttent*
get_ttable(hasht hash)
{
  return &ttable[hash%NTT];
}

int
init_board(board* b)
{
  memcpy(b->pieces, init_pieces, sizeof(init_pieces));
  b->side = white;
  b->xside = black;
  b->ply = 0;
  b->bking_loc = 40;
  b->hash_hist[b->ply] = hash_board(b);
  b->n_wman = 16;
  b->n_bman = 8;
  memset(b->hist, 0, sizeof(b->hist));
  return 0;
}

int
show_board(FILE* f, board* b)
{
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
    fprintf(f, "| %i\n", r+1);
  }
  fprintf(f, "+---------+  ");
  if (b->side==white)
    fprintf(f, " White to move\n");
  else if (b->side==black)
    fprintf(f, " Black to move\n");
  else if ((b->side==none) && (b->xside==white))
    fprintf(f, " ** White wins **\n");
  else if ((b->side==none) && (b->xside==black))
    fprintf(f, " ** Black wins **\n");
  else if ((b->side==none) && (b->xside==none))
    fprintf(f, " ** Draw **\n");
  return 0;
}

int
show_move(FILE* f, move m)
{
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

int
gen_moves(board* b, move* ml)
{
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
int
count_moves(board* b)
{
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
int
check3rep(board* b)
{
  int  i, c=1;
  hasht  h = b->hash_hist[b->ply-1];
  for (i=0; i<b->ply-1; ++i)
    if (b->hash_hist[i]==h)
      ++c;
  return c>=3;
}

int
validate_board(board* b, const char* chp)
{
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

move
make_move(board* b, move m)
{
  //printf("((((MAKE:");show_move(stdout, m);printf("))))");
  if (b->side==none) { printf("<NOBODY TO MOVE IN MAKE_MOVE?!>\n"); return m; }

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

  if (1) if (check3rep(b))
  {
    //printf("===3===\n");
    if (b->side==white)
      m.flags |= w_draw;
    else
      m.flags |= b_draw;
    b->hist[b->ply-1] = m;
  }

  if (0) if (b->n_wman==0) {
    m.flags |= b_win;
    b->hist[b->ply-1] = m;
  }

  if (m.flags&w_win) {
    b->side = none;
    b->xside = white;
  } else if (m.flags&b_win) {
    b->side = none;
    b->xside = black;
  } else if (m.flags&draw) {
    b->side = none;
    b->xside = none;
  } else if (b->side==white) {
    b->side = black;
    b->xside = white;
  } else if (b->side==black) {
    b->side = white;
    b->xside = black;
  } else {
    /* TODO: */
  }

  if (VALIDATE_MOVE) validate_board(b, "make_move");

  //if (m.flags&draw) {printf("((((MADE:");show_move(stdout, m);printf("))))\n");}
  return m;
}

int
unmake_move(board* b, move m)
{
  //if (m.flags&draw) {printf("((((UNMAKE:");show_move(stdout, m);printf("))))");}
  colort  moved = ((b->side==black) || (m.flags&(w_win|w_draw))) ? white : black;
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
  if (VALIDATE_MOVE) validate_board(b, "unmake_move");
  return 0;
}

inline int
unmake(board* b)
{
  return unmake_move(b, b->hist[b->ply-1]);
}

int
grid_init(board* b)
{
  int  i;
  for (i=0; i<81; ++i) {
    b->grid[i] = squares[i]==crn ? 0 : 99;
  }
  return 0;
}
int
grid_iter(board* b, int n)
{
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
int
grid_spread(board* b)
{
  int  count=0;
  grid_init(b);
  while (grid_iter(b,count))
    ++count;
  return count;
}

static const int piece_value[3] = {100,-50,-1200};
#define  white_mob_value  0
#define  black_mob_value  1

int  nodes_evaluated;
int  use_mobility = 0;
int
evaluate(board* b, int ply)
{
  ++nodes_evaluated;
  if (b->side==none) {
    if (b->xside==white) return 1000000 - 10*ply;
    else if (b->xside==black) return -1000000 + 10*ply;
    else if (b->xside==none) return 0;
  }

  int  sum = piece_value[bking] + piece_value[wman]*(b->n_wman) + piece_value[bman]*(b->n_bman);

  grid_spread(b);
  sum -= (99 / b->grid[b->bking_loc]) - 1;

  if (use_mobility) {
    colort save_side = b->side;
    b->side = white;
    sum += white_mob_value * count_moves(b);
    b->side = black;
    sum -= black_mob_value * count_moves(b);
    b->side = save_side;
  }

  return sum;
}

/* sort moves by score value (in stupid fashion) */
void
sort_moves(int n, move* ml, int* scorel)
{
  int  i, j;
  for (i=0; i<n; ++i) {
    for (j=i+1; j<n; ++j) {
      if (scorel[i] < scorel[j]) {
        int ts = scorel[i];
        move tm = ml[i];

        scorel[i] = scorel[j];
        ml[i] = ml[j];

        scorel[j] = ts;
        ml[j] = tm;
      }
    }
  }
}
/* just bring the best score from ml[i]..ml[n-1] to position i (swap it) */
inline void
sort_it(int i, int n, move* ml, int* scorel)
{
  int  best_score = scorel[i];
  int  besti = i;
  int  j;
  for (j=i+1; j<n; ++j) {
    if (scorel[j]>best_score) {
      best_score = scorel[j];
      besti = j;
    }
  }
  if (besti!=i) {
    int ts = scorel[i];
    move tm = ml[i];

    scorel[i] = scorel[besti];
    ml[i] = ml[besti];

    scorel[besti] = ts;
    ml[besti] = tm;
  }
}

move  pv[64][64];

int
search_minmax(board* b, int depth, int ply)
{
  if (depth<=0||b->side==none)
    return evaluate(b, ply);

  move  ml[256];
  int    n, i;
  int    best_eval = b->side==white ? -1000001 : 1000001;
  n = gen_moves(b, ml);
  if (n==0) {printf("<ZERO MOVES IN MINMAX!>\n");fflush(stdout);return b->side==white?-1000001:1000001;}

  for (i=0; i<n; ++i) {
    if (VALIDATE_BOARD) validate_board(b, "search_minmax.1");
    ml[i] = make_move(b, ml[i]);
    if (VALIDATE_BOARD) validate_board(b, "search_minmax.2");
    int val = search_minmax(b, depth-100, ply+1);
    if (VALIDATE_BOARD) validate_board(b, "search_minmax.3");
    unmake(b);
    if (VALIDATE_BOARD) validate_board(b, "search_minmax.4");
    if (b->side==white) {
      if (val>best_eval)
        best_eval = val;
    } else {
      if (val<best_eval)
        best_eval = val;
    }
  }

  return best_eval;
}
int
search_negamax(board* b, int depth, int ply)
{
  /* NB that all non-white & non-black sides are draws==0 */
  if (b->side==none)
    return b->xside==black ? evaluate(b, ply) : -evaluate(b, ply);
  if (depth<=0)
    return b->side==white ? evaluate(b, ply) : -evaluate(b, ply);

  move  ml[256];
  int    n, i;
  int    best_eval = -1000001;
  n = gen_moves(b, ml);
  if (n==0) {printf("<ZERO MOVES IN NEGAMAX!>\n");fflush(stdout);return -1000001;}

  for (i=0; i<n; ++i) {
    if (VALIDATE_BOARD) validate_board(b,"search_negamax");
    ml[i] = make_move(b, ml[i]);
    if (VALIDATE_BOARD) validate_board(b,"search_negamax");
    int val = -search_negamax(b, depth-100, ply+1);
    if (VALIDATE_BOARD) validate_board(b,"search_negamax");
    unmake(b);
    if (VALIDATE_BOARD) validate_board(b,"search_negamax");
    if (val>best_eval)
      best_eval = val;
  }

  return best_eval;
}
int
search_alphabeta(board* b, int depth, int alpha, int beta, int ply)
{
  /* NB that all (non-white & non-black) sides are draws==0 */
  if (b->side==none)
    return b->xside==black ? evaluate(b, ply) : -evaluate(b, ply);
  if (depth<=0)
    return b->side==white ? evaluate(b, ply) : -evaluate(b, ply);

  move  ml[256];
  int    n, i;
  n = gen_moves(b, ml);
  if (n==0) {printf("<ZERO MOVES IN ALPHABETA!>\n");fflush(stdout);return -1000001;}

  for (i=0; i<n; ++i) {
    ml[i] = make_move(b, ml[i]);
    int val = -search_alphabeta(b, depth-100, -beta, -alpha, ply+1);
    unmake(b);
    if (val>alpha) {
      alpha = val;
      /* cutoff */
      if (alpha>=beta) break;
      memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*(depth-100)/100);
      pv[ply][ply] = ml[i];
    }
  }

  return alpha;
}

int tt_hit;
int tt_false;
int tt_deep;
int tt_shallow;
int tt_used;
int
search_abtt(board* b, int depth, int alpha, int beta, int ply)
{
  /* NB that all (non-white & non-black) sides are draws==0 */
  if (b->side==none)
    return b->xside==black ? evaluate(b, ply) : -evaluate(b, ply);
  if (depth<=0)
    return b->side==white ? evaluate(b, ply) : -evaluate(b, ply);

  move  ml[256];
  int    n, i;

  hash_board(b);
  ttent*  tt = get_ttable(b->hash);
  if (tt->flags & tt_valid) {
    ++tt_hit;
    if (tt->hash!=b->hash) {
      ++tt_false;
      goto skip_tt;
    }
    if (tt->depth<depth) {
      ++tt_shallow;
      goto skip_tt;
    }
    if (tt->depth>depth) {
      ++tt_deep;
      goto skip_tt;
    }
    ++tt_used;
    return tt->score;
  }
skip_tt:


  n = gen_moves(b, ml);
  if (n==0) {printf("<ZERO MOVES IN ABTT!>\n");fflush(stdout);return -1000001;}

  hasht  hpre = b->hash;
  //hasht  hpre_ = hash_board(b);
  //if (hpre!=hpre_) { printf("$<%016llX!=%016llX>$", hpre, hpre_); }
  
  for (i=0; i<n; ++i) {
    ml[i] = make_move(b, ml[i]);
    int val = -search_abtt(b, depth-100, -beta, -alpha, ply+1);
    unmake(b);
    if (val>alpha) {
      alpha = val;
      /* cutoff */
      if (alpha>=beta) break;
      memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*(depth-100)/100);
      pv[ply][ply] = ml[i];
    }
  }
  hasht  hpost = b->hash;
  if (hpre!=hpost) { printf("!<%016llX!=%016llX>!", hpre, hpost); }
  //hasht  hpost_ = hash_board(b);
  //if (hpost!=hpost_) { printf("@<%016llX!=%016llX>@", hpre, hpre_); }

  if (alpha<beta) {
    put_ttable(b->hash, alpha, pv[ply][ply], tt_exact, depth);
  }
  return alpha;
}
int
search_abttx(board* b, int depth, int alpha, int beta, int ply)
{
  /* NB that all (non-white & non-black) sides are draws==0 */
  if (b->side==none)
    return b->xside==black ? evaluate(b, ply) : -evaluate(b, ply);
  if (depth<=0)
    return b->side==white ? evaluate(b, ply) : -evaluate(b, ply);

  move  ml[256];
  int    scorel[256];
  int    n, i;
  move  goodm;
  int    have_goodm = 0;

  hash_board(b);
  ttent*  tt = get_ttable(b->hash);
  if (tt->flags & tt_valid) {
    ++tt_hit;
    if (tt->hash!=b->hash) {
      ++tt_false;
      goto skip_tt;
    }
    if (tt->depth<depth) {
      ++tt_shallow;
      have_goodm = 1;
      goodm = tt->best_move;
      goto skip_tt;
    }
    if (tt->depth>depth) {
      ++tt_deep;
      //goto skip_tt;
    }
    ++tt_used;
    return tt->score;
  }
skip_tt:

  n = gen_moves(b, ml);
  if (n==0) {printf("<ZERO MOVES IN ABTTX!>\n");fflush(stdout);return -1000001;}
  if (n>256) {printf("<TOO MANY MOVES IN ABTTX!  INTERNAL INCONSISTENCY!>\n");fflush(stdout);return -1000001;}
  /* initialize scores by capture value */
  for (i=0; i<n; ++i) scorel[i] = (ml[i].caps&0xF0) ? (10000+i) : (ml[i].caps&0x0F) ? (100+i) : 0;
  //sort_moves(n, ml, scorel);

  if (have_goodm) {
    /* bonus score to transition-table iteration good move so it's used first */
    for (i=0; i<n; ++i)
    {
      if ((goodm.from==ml[i].from) && (goodm.to==ml[i].to)) {
        scorel[i] += 1000000;
        break;
      }
    }
  }

  hasht  hpre = b->hash;
  for (i=0; i<n; ++i) {
    sort_it(i, n, ml, scorel);
    ml[i] = make_move(b, ml[i]);
    int val = -search_abttx(b, depth-100, -beta, -alpha, ply+1);
    unmake(b);
    if (val>alpha) {
      alpha = val;
      /* cutoff */
      if (alpha>=beta) break;
      memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*(depth-100)/100);
      pv[ply][ply] = ml[i];
    }
  }
  hasht  hpost = b->hash;

  if (hpre!=hpost) { printf("!<%016llX!=%016llX>!", hpre, hpost); }
  if (alpha<beta) {
    put_ttable(b->hash, alpha, pv[ply][ply], tt_exact, depth);
  }

  return alpha;
}

move
search(board* b, int depth, int* eval)
{
  move  ml[256];
  move  bestm={0,0,0,0};
  int    bestv = b->side==white ? -1000001 : 1000001;
  int    i, n;

  if (VALIDATE_BOARD) validate_board(b, "search");

  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH1!>\n");

  for (i=0; i<n; ++i) {
    ml[i] = make_move(b, ml[i]);
    int  v = search_minmax(b, depth, 1);
    unmake(b);
    if (SHOW_MOVES) { printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%6==5) printf("\n"); }

    if (b->side==white) {
      if (v>bestv) {
        bestm = ml[i];
        bestv = v;
      }
    } else {
      if (v<bestv) {
        bestm = ml[i];
        bestv = v;
      }
    }
  }
  if (SHOW_MOVES) printf("\n");
  
  *eval = bestv;
  return bestm;
}

move
search2(board* b, int depth, int* eval)
{
  move  ml[256];
  move  bestm={0,0,0,0};
  int    bestv = -1000001;
  int    i, n;

  if (VALIDATE_BOARD) validate_board(b, "search2");

  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH2!>\n");

  for (i=0; i<n; ++i) {
    ml[i] = make_move(b, ml[i]);
    int  v = -search_negamax(b, depth, 1);
    unmake(b);
    if (SHOW_MOVES) { printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%6==5) printf("\n"); }

    if (v>bestv) {
      bestm = ml[i];
      bestv = v;
    }
  }
  if (SHOW_MOVES) printf("\n");
  
  *eval = bestv;
  return bestm;
}

move
search3(board* b, int depth, int* eval)
{
  move  ml[256];
  move  bestm={0,0,0,0};
  int    bestv = -1000001;
  int    i, n;

  if (VALIDATE_BOARD) validate_board(b, "search3");

  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH3!>\n");

  for (i=0; i<n; ++i) {
    ml[i] = make_move(b, ml[i]);
    int  v = -search_alphabeta(b, depth, -1000001, -bestv, 1);
    unmake(b);
    if (SHOW_MOVES) { printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%6==5) printf("\n"); }

    if (v>bestv) {
      bestm = ml[i];
      bestv = v;
      memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(depth-100)/100);
      pv[0][0] = ml[i];
    }
  }
  if (SHOW_MOVES) printf("\n");
  
  *eval = bestv;
  return bestm;
}
move
search3tt(board* b, int depth, int* eval)
{
  move  ml[256];
  move  bestm={0,0,0,0};
  int    bestv = -1000001;
  int    i, n;

  if (VALIDATE_BOARD) validate_board(b, "search3tt");
  
  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH3TT!>\n");

  hasht hpre = b->hash;
  for (i=0; i<n; ++i) {
    ml[i] = make_move(b, ml[i]);
    int  v = -search_abtt(b, depth, -1000001, -bestv, 1);
    unmake(b);
    if (b->hash!=hpre) printf("<!>");
    if (SHOW_MOVES) { printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%6==5) printf("\n"); }

    if (v>bestv) {
      bestm = ml[i];
      bestv = v;
      memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(depth-100)/100);
      pv[0][0] = ml[i];
    }
  }
  if (SHOW_MOVES) printf("\n");
  
  *eval = bestv;
  return bestm;
}

move
search4(board* b, int depth, int* eval)
{
  move  ml[256];
  int    scorel[256];
  move  bestm={0,0,0,0};
  int    bestv = -1000001;
  int    i, n, d;
  int    pre, post;

  if (VALIDATE_BOARD) validate_board(b, "search4");

  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH4!>\n");

  /* initialize scores by capture value */
  for (i=0; i<n; ++i) scorel[i] = (ml[i].caps&0xF0) ? (10000+i) : (ml[i].caps&0x0F) ? (100+i) : 0;

  for (d=100; d<=depth; d+=100) {
    bestv = -1000001;
    pre = nodes_evaluated;
    for (i=0; i<n; ++i) {
      sort_it(i, n, ml, scorel);
      ml[i] = make_move(b, ml[i]);
      int  v = -search_alphabeta(b, d, -1000001, -bestv, 1);
      unmake(b);
      if (SHOW_MOVES) {
        printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%6==5) printf("\n");
      }

      if (v>bestv) {
        bestm = ml[i];
        bestv = v;
        memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(d-100)/100);
        pv[0][0] = ml[i];
      }
      scorel[i] = v;
    }
    if (SHOW_MOVES) printf("\n");
    post = nodes_evaluated;
    printf("{{Nodes for iter %i = %i}}\n", d, post-pre);
    fflush(stdout);
  }
  *eval = bestv;
  return bestm;
}
move
search4tt(board* b, int depth, int* eval)
{
  move  ml[256];
  int    scorel[256];
  move  bestm={0,0,0,0};
  int    bestv = -1000001;
  int    i, n, d;
  int    pre, post;

  if (VALIDATE_BOARD) validate_board(b, "search4tt");

  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH4TT!>\n");

  /* initialize scores by capture value */
  for (i=0; i<n; ++i) scorel[i] = (ml[i].caps&0xF0) ? (10000+i) : (ml[i].caps&0x0F) ? (100+i) : 0;

  for (d=100; d<=depth; d+=100) {
    bestv = -1000001;
    pre = nodes_evaluated;
    for (i=0; i<n; ++i) {
      sort_it(i, n, ml, scorel);
      ml[i] = make_move(b, ml[i]);
      int  v = -search_abtt(b, d, -1000001, -bestv, 1);
      unmake(b);
      if (SHOW_MOVES) {
        printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%6==5) printf("\n");
      }

      if (v>bestv) {
        bestm = ml[i];
        bestv = v;
        memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(d-100)/100);
        pv[0][0] = ml[i];
      }
      scorel[i] = v;
    }
    if (SHOW_MOVES) printf("\n");
    post = nodes_evaluated;
    printf("{{Nodes for iter %i = %i}}\n", d, post-pre);
    fflush(stdout);
  }
  *eval = bestv;
  return bestm;
}
move
search4ttx(board* b, int depth, int* eval)
{
  move  ml[256];
  int    scorel[256];
  move  bestm={0,0,0,0};
  int    bestv = -1000001;
  int    i, n, d;
  int    pre, post;

  if (VALIDATE_BOARD) validate_board(b, "search4tt");

  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH4TT!>\n");

  /* initialize scores by capture value */
  for (i=0; i<n; ++i) scorel[i] = (ml[i].caps&0xF0) ? (10000+i) : (ml[i].caps&0x0F) ? (100+i) : 0;

  for (d=100; d<=depth; d+=100) {
    bestv = -1000001;
    pre = nodes_evaluated;
    for (i=0; i<n; ++i) {
      sort_it(i, n, ml, scorel);
      ml[i] = make_move(b, ml[i]);
      int  v = -search_abttx(b, d, -1000001, -bestv, 1);
      unmake(b);
      if (SHOW_MOVES) {
        printf("{"); show_move(stdout, ml[i]); printf(" :%i}", v); if (i%6==5) printf("\n");
      }

      if (v>bestv) {
        bestm = ml[i];
        bestv = v;
        memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(d-100)/100);
        pv[0][0] = ml[i];
      }
      /* save value for sorting next iteration */
      scorel[i] = v;
    }
    if (SHOW_MOVES) printf("\n");
    post = nodes_evaluated;
    printf("{{Nodes for iter %i = %i}}\n", d, post-pre);
    fflush(stdout);
  }
  *eval = bestv;
  return bestm;
}

int
main(int argc, char* argv[])
{
  board  b;
  int  i, n, d;
  int  depths[2] = {400,500};

  srand48(-13);
  init_hash();

  init_board(&b);

  for(i=0; ; ++i) {
    unsigned int bt, et;
    move  m;
    int  val;
    show_board(stdout, &b);
    //if ((i%5==4)&&(depths[0]<400)) { depths[0]+=100; depths[1]+=100; }
    //for (d=0; d<5; ++d) 
    d = 0;
    {
      use_mobility = (b.side==white);
      nodes_evaluated = 0;
      tt_hit = tt_false = tt_deep = tt_shallow = tt_used = 0;
      init_ttable();
      bt = read_clock();
      switch (d) {
      case 0:
        printf(" --- Alpha-beta(tt+id++)(%i) ---\n", depths[b.side]);
        m = search4ttx(&b, depths[b.side], &val);
        break;
      case 1:
        printf(" --- Alpha-beta(tt+id)(%i) ---\n", depths[b.side]);
        m = search4tt(&b, depths[b.side], &val);
        break;
      case 2:
        printf(" --- Alpha-beta(tt)(%i) ---\n", depths[b.side]);
        m = search3tt(&b, depths[b.side], &val);
        break;
      case 3:
        printf(" --- Alpha-beta(id)(%i) ---\n", depths[b.side]);
        m = search4(&b, depths[b.side], &val);
        break;
      case 4:
        printf(" --- Alpha-beta(simple)(%i) ---\n", depths[b.side]);
        m = search3(&b, depths[b.side], &val);
        break;
      }
      et = read_clock();
      for (n=0; n<depths[b.side]/100; ++n) show_move(stdout, pv[0][n]); printf("\n");
      printf("[%8in, %.4fs : %9.2fnps || tt:hit=%i false=%i deep=%i shallow=%i used=%i] %i:%c",
        nodes_evaluated, (double)(et-bt)*1e-6, 1e6*(double)nodes_evaluated/(double)(et-bt),
        tt_hit, tt_false, tt_deep, tt_shallow, tt_used,
        i, "WBN"[b.side]
        );
      show_move(stdout, m); printf(" {{%i}}}\n", val);
      fflush(stdout);
    }
    m = make_move(&b, m);
    if (b.side==none) break;
    if (check3rep(&b)) {
      printf("\n!!!!! 3-time repetition not caught properly! !!!!!!\n");
      break;
    }
  }
  show_board(stdout, &b);
  printf("\n [[[%i]]]\n", evaluate(&b, 0));

  return 0;
}
