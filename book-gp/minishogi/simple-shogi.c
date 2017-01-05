#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

int debug_dump = 0;

/*
 * 5x5 "mini-shogi" with drops and promotion
 *
 * uses single-array board with 'mailbox'
 */

unsigned int read_clock(void) {
  struct timeval timeval;
  struct timezone timezone;
  gettimeofday(&timeval, &timezone);
  return (timeval.tv_sec * 1000000 + (timeval.tv_usec));
}

typedef unsigned char  uint8;
typedef unsigned short  uint16;
typedef unsigned int  uint32;
typedef unsigned int  uint;
typedef unsigned long long  uint64;
typedef uint32  hasht;

typedef enum piecet {
  pawn, gold, silver, bishop, rook, king,
  tokin, promoted_silver, horse, dragon,
  empty
} piecet;
typedef enum colort {
  black, white, none
} colort;
char piece_name[] = {'P','G','S','B','R','K','T','N','H','D', '#'};

typedef enum state {
  black_win, white_win, draw, playing
} state;

piecet  promotes[10] = {tokin,-1,promoted_silver,horse,dragon,-1, -1,-1,-1,-1};
piecet  unpromotes[10] = {pawn,gold,silver,bishop,rook,king, pawn,silver,bishop,rook};

typedef struct board {
  piecet piece[25];
  colort color[25];
  colort  side;
  colort  xside;
  int    hand[2][5];
  int    nhand[2];
  int  ply;
  hasht  hash1;
  hasht  hash2;
  state  play;
} board;

#define  b_hand  25
#define  w_hand  26
#define  mf_cap  1
#define  mf_pro  2
#define  mf_nonpro  4
#define  mf_drop  8
#define  mf_whitemove  16
typedef union move {
  struct {
    uint f : 6; /* from */
    uint t : 6; /* to */
    uint p : 6; /* captured / dropped piece */
    uint m : 6; /* moved piece */
    uint8 flag;
  };
  uint32 i;
} move;

typedef enum ttable_flags {
  tt_valid=1, tt_exact=2, tt_upper=4, tt_lower=8
} ttable_flags;
typedef struct ttable_entry {
  hasht hash;
  int   score;
  int   flags;
  int   depth;
  move  best_move;
} ttable_entry;
int tt_hits;
int tt_false;
int tt_deep;
int tt_shallow;
int tt_used;
#define TTABLE_ENTRIES  (1024*256)
ttable_entry  ttable[TTABLE_ENTRIES];
void init_ttable() {
  memset(ttable, '\x00', sizeof(ttable));
  tt_hits = 0;
  tt_false = 0;
  tt_deep = 0;
  tt_shallow = 0;
  tt_used = 0;
}
void put_ttable(hasht hash1, hasht hash2, int score, move m, int flags, int depth) {
  const int loc = (unsigned int)hash1%TTABLE_ENTRIES;
  ttable[loc].hash = hash2;
  ttable[loc].score = score;
  ttable[loc].best_move = m;
  ttable[loc].flags = flags|tt_valid;
  ttable[loc].depth = depth;
}
ttable_entry* get_ttable(hasht hash1) {
  const int loc = (unsigned int)hash1%TTABLE_ENTRIES;
  return &ttable[loc];
}


hasht   hash_piece[2][2][10][25]; /* hash x side x piece x square */
hasht   hash_piece_hand[2][2][5][3]; /* hash x side x piece x num */
hasht  hash_side_white[2];

hasht gen_hash() { return (hasht)lrand48(); }
int
init_hash()
{
  int  h, i, j, k;
  for (h=0; h<2; ++h) {
    for (i=0; i<2; ++i)
      for (j=0; j<10; ++j)
        for (k=0; k<25; ++k)
          hash_piece[h][i][j][k] = gen_hash();
    for (i=0; i<2; ++i)
      for (j=0; j<5; ++j)
        for (k=0; k<3; ++k)
          hash_piece_hand[h][i][j][k] = gen_hash();
    hash_side_white[h] = gen_hash();
  }
  return 0;
}

int
hash_board(board* b)
{
  hasht  h1=0, h2=0;
  int  i, j;
  if (b->side == white) {
    h1 = hash_side_white[0];
    h2 = hash_side_white[1];
  }
  for (i=0; i<25; ++i)
  {
    if (b->piece[i]!=empty)
    {
      h1 = h1 ^ hash_piece[0][b->color[i]][b->piece[i]][i];
      h2 = h2 ^ hash_piece[1][b->color[i]][b->piece[i]][i];
    }
  }
  for (i=0; i<2; ++i) {
    for (j=0; j<5; ++j) {
      h1 = h1 ^ hash_piece_hand[0][i][j][b->hand[i][j]];
      h2 = h2 ^ hash_piece_hand[1][i][j][b->hand[i][j]];
    }
  }
  b->hash1 = h1;
  b->hash2 = h2;
  return 0;
}

int board_to_box[25] = {
   8,  9, 10, 11, 12,
  15, 16, 17, 18, 19,
  22, 23, 24, 25, 26,
  29, 30, 31, 32, 33,
  36, 37, 38, 39, 40,
};
int box_to_board[49] = {
  -1, -1, -1, -1, -1, -1, -1,
  -1,  0,  1,  2,  3,  4, -1,
  -1,  5,  6,  7,  8,  9, -1,
  -1, 10, 11, 12, 13, 14, -1,
  -1, 15, 16, 17, 18, 19, -1,
  -1, 20, 21, 22, 23, 24, -1,
  -1, -1, -1, -1, -1, -1, -1,
};
/*pawn, gold, silver, bishop, rook, king, tokin, promoted_silver, horse, dragon*/
int move_step_num[10] = {1, 6, 5, 0, 0, 8, 6, 6, 4, 4};
int move_step_offset[10][8] = {
  {  7,  0,  0,  0,  0,  0,  0,  0}, /* pawn */
  {  6,  7,  8,  1, -7, -1,  0,  0}, /* gold */
  {  6,  7,  8, -6, -8,  0,  0,  0}, /* silver */
  {  0,  0,  0,  0,  0,  0,  0,  0}, /* bishop */
  {  0,  0,  0,  0,  0,  0,  0,  0}, /* rook */
  {  6,  7,  8,  1, -6, -7, -8, -1}, /* king */
  {  6,  7,  8,  1, -7, -1,  0,  0}, /* tokin */
  {  6,  7,  8,  1, -7, -1,  0,  0}, /* promoted_silver */
  {  7,  1, -7, -1,  0,  0,  0,  0}, /* horse */
  {  6,  8, -8, -6,  0,  0,  0,  0}, /* dragon */
};
int move_slide_offset[10][5] = {
  {  0,  0,  0,  0,  0},
  {  0,  0,  0,  0,  0},
  {  0,  0,  0,  0,  0},
  {  6,  8, -6, -8,  0}, /* bishop */
  {  7,  1, -7, -1,  0}, /* rook */
  {  0,  0,  0,  0,  0},
  {  0,  0,  0,  0,  0},
  {  0,  0,  0,  0,  0},
  {  6,  8, -6, -8,  0}, /* horse */
  {  7,  1, -7, -1,  0}, /* dragon */
};
int promotion[25] = {
  white, white, white, white, white,
  none, none, none, none, none,
  none, none, none, none, none,
  none, none, none, none, none,
  black, black, black, black, black
};

#define  MOVE(mx,fx,tx,dx,vx,flx,nx) do { if (!caps_only || ((flx)&mf_cap)) { \
  (mx).f=(fx); \
  (mx).t=(tx); \
  (mx).p=(dx); \
  (mx).m=(vx); \
  (mx).flag=(flx); \
  ++(nx); \
  } } while (0)

int
generate_moves(const board* b, move* ml, int caps_only)
{
  int num = 0;
  int f, t, i;
  int mult = b->side==black ? +1 : -1;
  int flg = b->side==black ? 0 : mf_whitemove;
  /* generate drops
   *  drop onto empty squares
   *  can't drop pawns onto promotion squares
   *  TODO: disallow pawn drops giving checkmate ...
   */
  if (!caps_only && b->nhand[b->side])
  {
    for (i=0; i<5; ++i)
    {
      if (b->hand[b->side][i]) {
        f = b->side==black ? b_hand : w_hand;
        for (t=0; t<25; ++t)
          if ((b->piece[t]==empty) && ((i!=pawn)||(promotion[t]!=b->side)))
            MOVE(ml[num],f,t,i,i,flg|mf_drop,num);
      }
    }
  }
  for (f=0; f<25; ++f) {
    if ((b->piece[f]!=empty) && (b->side == b->color[f])) {
      const int mvd = b->piece[f];
      /* generate step moves */
      for (i=0; i<move_step_num[mvd]; ++i) {
        t = box_to_board[board_to_box[f] + mult*move_step_offset[mvd][i]];
        if (t<0) continue;
        if ((b->piece[t]==empty) || (b->color[t]==b->xside)) {
          int  flags=0, p=0;
          if (b->piece[t]!=empty) { flags|=mf_cap; p=b->piece[t]; }
          if ( ((promotion[f]==b->side) || (promotion[t]==b->side)) && (promotes[mvd]!=-1) )
          {
            if (mvd != pawn) MOVE(ml[num],f,t,p,mvd,flg|flags|mf_nonpro,num);
            MOVE(ml[num],f,t,p,mvd,flg|flags|mf_pro,num);
          } else {
            MOVE(ml[num],f,t,p,mvd,flg|flags,num);
          }
        }
      }
      /* generate slide moves */
      int offset;
      for (i=0; (offset=move_slide_offset[mvd][i]); ++i) {
        t = f;
        for (;;) {
          t = box_to_board[board_to_box[t] + offset];
          if (t==-1) break;
          int fp = (((promotion[f]==b->side)||(promotion[t]==b->side))&&(promotes[mvd]!=-1)) ? mf_pro : 0;
          if (b->piece[t]==empty) {
            MOVE(ml[num],f,t,0,mvd,flg|fp,num);
            if (fp&mf_pro)
              MOVE(ml[num],f,t,0,mvd,(flg|fp|mf_nonpro)&~mf_pro,num);
          } else if (b->color[t]==b->xside) {
            MOVE(ml[num],f,t,b->piece[t],mvd,flg|fp|mf_cap,num);
            if (fp&mf_pro)
              MOVE(ml[num],f,t,b->piece[t],mvd,(flg|fp|mf_cap|mf_nonpro)&~mf_pro,num);
            break;
          } else {
            break;
          }
        }
      }
    }
  }
  return num;
}

const piecet initial_board_piece[27] = {
  king, gold, silver, bishop, rook,
  pawn, empty, empty, empty, empty,
  empty, empty, empty, empty, empty,
  empty, empty, empty, empty, pawn,
  rook, bishop, silver, gold, king,
  empty,empty
};
const colort initial_board_color[27] = {
  black, black, black, black, black,
  black, none, none, none, none,
  none, none, none, none, none,
  none, none, none, none, white,
  white, white, white, white, white,
  black,white
};

int
init_board(board* b)
{
  memcpy(b->piece, initial_board_piece, sizeof(initial_board_piece));
  memcpy(b->color, initial_board_color, sizeof(initial_board_color));
  b->side = black;
  b->xside = white;
  b->ply = 1;
  b->nhand[0] = b->nhand[1] = 0;
  memset(b->hand, 0, sizeof(b->hand));
  b->play = playing;
  hash_board(b);
  return 0;
}

const piecet sample_board_piece[27] = {
  king , gold , rook , bishop, empty,
  pawn , empty, empty, empty, empty,
  empty, empty, silver, empty, empty,
  silver, rook, empty, empty, pawn,
  empty, empty, empty, empty, king,
  empty,empty
};
const colort sample_board_color[27] = {
  black, white, black, black, none ,
  black, none , none , none , none ,
  none , none , black, none , none ,
  white, white, none , none , white,
  none , none , none , none , white,
  black, white
};
int
sample_board(board* b)
{
  memcpy(b->piece, sample_board_piece, sizeof(sample_board_piece));
  memcpy(b->color, sample_board_color, sizeof(sample_board_color));
  b->side = black;
  b->xside = white;
  b->ply = 1;
  b->nhand[0] = b->nhand[1] = 1;
  memset(b->hand, 0, sizeof(b->hand));
  b->hand[black][gold] = 1;
  b->hand[white][bishop] = 1;
  b->play = playing;
  hash_board(b);
  return 0;
}

/* TODO: come up with better values here! */
/* pawn, gold, silver, bishop, rook,
   king, tokin, promoted_silver, horse, dragon, */
int piece_value[10] = {
  100, 300, 200, 400, 500,
  1000, 150, 250, 500, 600
};
int piece_value_hand[5] = {
  100/2, 300/2, 200/2, 400/2, 500/2,
};
#define  win_value  +100000000
#define  loss_value  -100000000
#define  draw_value  0
int  nodes_evaluated;
int
evaluate_rel(board* b, int ply)
{
  int sgn = (b->side == black) ? +1 : -1;
  int  sum=0, i;
  ++nodes_evaluated;

  /* terminal */
  if (b->play!=playing) {
    if (b->play==black_win) return sgn*(win_value - ply);
    else if (b->play==white_win) return sgn*(loss_value + ply);
    else /*if (b->play==draw)*/ return sgn*(draw_value);
  }

  /* material */
  for (i=0; i<25; ++i) {
    if (b->piece[i]!=empty) {
      if (b->color[i] == black)
        sum += piece_value[b->piece[i]];
      else
        sum -= piece_value[b->piece[i]];
    }
  }
  for (i=0; i<5; ++i) {
    sum += b->hand[black][i] * piece_value_hand[i];
    sum -= b->hand[white][i] * piece_value_hand[i];
  }

  /* king tropism */
  int bk=0, wk=0;
  for (i=0; i<25; ++i) {
    if (b->piece[i]==king) {
      if (b->color[i]==black)
        bk = i;
      else
        wk = i;
    }
  }
  for (i=0; i<25; ++i) {
    if (b->piece[i]!=empty && b->piece[i]!=king) {
      if (b->color[i]==black)
        sum += 5 * (10 - abs((wk%5)-(i%5)) - abs((wk/5)-(i/5)));
      else
        sum -= 5 * (10 - abs((bk%5)-(i%5)) - abs((bk/5)-(i/5)));
    }
  }

  /* randomness */
  sum += (b->hash1 % 7) - (b->hash2 % 7);

  return sgn * sum;
}

int
make_move(board* b, move m)
{
  if (m.flag & mf_drop)
  {
    b->piece[m.t] = m.p;
    b->color[m.t] = b->side;
    --b->nhand[b->side];
    --b->hand[b->side][m.p];
  }
  else
  {
    if (m.flag & mf_cap) {
      if (m.p==king)
      {
        if (b->side==black)
          b->play = black_win;
        else
          b->play = white_win;
      } else {
        ++b->hand[b->side][unpromotes[m.p]];
        ++b->nhand[b->side];
      }
    }

    b->piece[m.t] = b->piece[m.f];
    b->color[m.t] = b->side;
    b->piece[m.f] = empty;
    b->color[m.f] = none;
    if (m.flag&mf_pro)
    {
      b->piece[m.t] = promotes[b->piece[m.t]];
    }
  }

  if (b->side==black) {
    b->side = white;
    b->xside = black;
  } else {
    b->side = black;
    b->xside = white;
  }
  ++b->ply;
  hash_board(b);
  return 0;
}

int
unmake_move(board* b, move m)
{
  b->play = playing;
  if (m.f<25) {
    b->piece[m.f] = (m.flag&mf_pro) ? unpromotes[b->piece[m.t]] : b->piece[m.t];
    b->color[m.f] = b->xside;
  }
  
  if (m.flag & mf_drop) {
    b->piece[m.t] = empty;
    b->color[m.t] = none;
    ++b->nhand[b->xside];
    ++b->hand[b->xside][m.p];
  } else if (m.flag & mf_cap) {
    b->piece[m.t] = m.p;
    b->color[m.t] = b->side;;
    if (m.p!=king) {
      --b->nhand[b->xside];
      --b->hand[b->xside][unpromotes[m.p]];
    }
  } else {
    b->piece[m.t] = empty;
    b->color[m.t] = none;
  }

  if (b->side==black) {
    b->side = white;
    b->xside = black;
  } else {
    b->side = black;
    b->xside = white;
  }
  --b->ply;
  hash_board(b);
  return 0;
}


int pv_length[64];
move  pv[64][64];

int use_tt_move = 0;

/* no move chosen if depth<=0 */
int
search_alphabeta_tt(board* b, int ply, int depth, int alpha, int beta, int quiescence)
{
  move  ml[256];
  int   n, v, i;
  int   have_tt_move = 0;
  move  tt_move;

  if (debug_dump) {
    printf("%*ssearch_alphabeta_tt(..., ply=%i, alpha=%i, beta=%i, q=%i)\n", ply*4, "", ply, alpha, beta, quiescence);
  }
  
  if (b->play!=playing) {
    pv_length[ply] = 0;
    return evaluate_rel(b, ply);
  }
  if (depth<=0) {
    pv_length[ply] = 0;
    if (quiescence)
      return search_alphabeta_quiesce(b, ply, alpha, beta);
    else
      return evaluate_rel(b, ply);
  }

  ttable_entry* tt = get_ttable(b->hash1);
  if (tt->flags & tt_valid) {
    ++tt_hits;
    if (tt->hash != b->hash2) {
      ++tt_false;
    } else if (tt->depth < depth) {
      ++tt_shallow;
      have_tt_move = 1;
      tt_move = tt->best_move;
    } else if (tt->depth > depth) {
      ++tt_deep;
      have_tt_move = 1;
      tt_move = tt->best_move;
    } else if (tt->flags & tt_exact) {
      ++tt_used;
      pv[ply][0] = tt->best_move;
      pv_length[ply] = 1;
      return tt->score;
    }
  }

  n = generate_moves(b, ml, 0);
  if (n<=0) {
    printf("#No moves#\n");
    pv_length[ply] = 0;
    return evaluate_rel(b, ply);
  }

  if (use_tt_move && have_tt_move) {
    for (i=0; i<n; ++i) {
      if (ml[i].i == tt_move.i) {
        move t = ml[0];
        ml[0] = ml[i];
        ml[i] = ml[0];
      }
    }
  }

  int store_tt = 0;
  pv[ply][0] = ml[0];
  for (i=0; i<n; ++i) {
    make_move(b,ml[i]);
    v = -search_alphabeta_tt(b, ply+1, depth-100, -beta, -alpha, quiescence);
    unmake_move(b,ml[i]);

    if (v>alpha) {
      store_tt = 1;
      alpha = v;
      if (alpha >= beta)
        return alpha;
      memcpy(&pv[ply][1], &pv[ply+1][0], sizeof(move)*pv_length[ply+1]);
      pv[ply][0] = ml[i];
      pv_length[ply] = pv_length[ply+1]+1;
    }
  }
  
  if (store_tt && (alpha<beta))
    put_ttable(b->hash1, b->hash2, alpha, pv[ply][0], tt_exact, depth);
  return alpha;
}

int sort_quiesce = 0;
int qply = 0;
int qfutprune = 0;
int qtt = 0;
int
search_alphabeta_quiesce(board* b, int ply, int alpha, int beta)
{
  move  ml[256], tt_move, best_move;
  int   n, v, i, static_eval;
  int   have_tt_move = 0;
  if (ply>qply) qply=ply;

  static_eval = evaluate_rel(b, 100+ply);

  if (debug_dump) {
    printf("%*ssearch_alphabeta_quiesce(..., ply=%i, alpha=%i, beta=%i)\n", ply*4, "", ply, alpha, beta);
    printf("%*sstatic_eval=%i\n", ply*4+2, "", static_eval);
  }
  
  if (b->play!=playing) {
    if (debug_dump) {
      printf("%*sGameover return = %i\n", ply*4+2, "", static_eval);
    }
    return static_eval;
  }

  /* "stand-pat" */
  if (static_eval>alpha) {
    alpha = static_eval;
    if (alpha >= beta) {
      if (debug_dump) {
        printf("%*sStandpat return = %i\n", ply*4+2, "", alpha);
      }
      return alpha;
    }
  }

  if (!qtt) goto skip_tt;
  ttable_entry* tt = get_ttable(b->hash1);
  if (tt->flags & tt_valid) {
    ++tt_hits;
    if (tt->hash != b->hash2) {
      ++tt_false;
      goto skip_tt;
    }
    if (tt->depth != -1) {
      ++tt_deep;
      have_tt_move = 1;
      tt_move = tt->best_move;
      goto skip_tt;
    }
    if (tt->flags & tt_exact)
    {
      ++tt_used;
      return tt->score;
    }
  }
skip_tt:

  n = generate_moves(b, ml, 1);
  if (sort_quiesce) {
    int k1, k2;
    //for (k1=0; k1<n; ++k1) { show_move(stdout, ml[k1]); printf(" "); } printf("\n");
    for (k1=0; k1<n; ++k1) {
      int best_k = k1;
      int best_v = 100*piece_value[ml[k1].p] - piece_value[ml[k1].m]
        + ((have_tt_move && (ml[k1].i==best_move.i)) ? 1000000 : 0);
      for (k2=k1+1; k2<n; ++k2) {
        int val = 100*piece_value[ml[k2].p] - piece_value[ml[k2].m]
          + ((have_tt_move && (ml[k2].i==best_move.i)) ? 1000000 : 0);
        if (val > best_v) {
          best_k = k2;
          best_v = val;
        }
      }
      move t = ml[k1];
      ml[k1] = ml[best_k];
      ml[best_k] = t;
    }
    //for (k1=0; k1<n; ++k1) { show_move(stdout, ml[k1]); printf(" "); } printf("\n");
  }

  int store_tt = 0;
  best_move = ml[0];
  for (i=0; i<n; ++i) {
    if (qfutprune && (static_eval+piece_value[ml[i].p] < alpha)) continue;
    if (debug_dump) {
      printf("%*sMake move: ", ply*4+2, ""); show_move(stdout, ml[i]); printf("\n");
    }
    make_move(b,ml[i]);
    v = -search_alphabeta_quiesce(b, ply+1, -beta, -alpha);
    unmake_move(b,ml[i]);
    if (debug_dump) {
      printf("%*ssubtree value=%i\n", ply*4+2, "", v);
    }
    if (v>alpha) {
      store_tt = 1;
      alpha = v;
      if (alpha >= beta) {
        if (debug_dump) {
          printf("%*sCutoff return = %i\n", ply*4+2, "", alpha);
        }
        return alpha;
      }
      best_move = ml[i];
    }
  }
  if (qtt && store_tt && (alpha<beta))
    put_ttable(b->hash1, b->hash2, alpha, best_move, tt_exact, -1);
  if (debug_dump) {
    printf("%*sNormal return = %i\n", ply*4+2, "", alpha);
  }
  return alpha;
}

int
show_board(FILE* f, board* b)
{
  int  r, c, rc, i, j;

  fprintf(f, "   E  D  C  B  A  \n");
  fprintf(f, " +---------------+    [%08X %08X]\n", b->hash1, b->hash2);
  for (r=4; r>=0; --r) {
    fprintf(f, " |");
    for (c=0; c<5; ++c) {
      rc = r*5+c;
      if (b->piece[rc]!=empty) {
        if (b->color[rc] == black)
          fprintf(f, " %c ", piece_name[b->piece[rc]]);
        else
          fprintf(f, " %c ", piece_name[b->piece[rc]]+'a'-'A');
      } else {
        fprintf(f, " . ");
      }
    }
    fprintf(f, "| (%i)", 5-r);

    if (r==1) {
      fprintf(f, "  Black(%i)[ ", b->nhand[black]);
      for (i=0; i<5; ++i) {
        if (b->hand[black][i]) {
          for (j=0; j<b->hand[black][i]; ++j)
            fprintf(f, "%c", piece_name[i]);
          fprintf(f, " ");
        }
      }
      fprintf(f, "]");
    } else if (r==2) {
      fprintf(f, "  White(%i)[ ", b->nhand[white]);
      for (i=0; i<5; ++i) {
        if (b->hand[white][i]) {
          for (j=0; j<b->hand[white][i]; ++j)
            fprintf(f, "%c", piece_name[i]+'a'-'A');
          fprintf(f, " ");
        }
      }
      fprintf(f, "]");
    } else if (r==4) {
      fprintf(f, " {%i}", evaluate_rel(b,0));
    }
    fprintf(f, "\n");
  }
  fprintf(f, " +---------------+\n");

  if (b->play==playing) {
    if (b->side==black)
      fprintf(f, " Black to move\n");
    else
      fprintf(f, " White to move\n");
  } else {
    if (b->play==black_win)
      fprintf(f, " Black wins\n");
    else if (b->play==white_win)
      fprintf(f, " White wins\n");
    else 
      fprintf(f, " Draw\n");
  }

  return 0;
}

int
dump_move(FILE* f, move m)
{
  fprintf(f, "[%i->%ix%i:%2X]", m.f, m.t, m.p, m.flag);
  return 0;
}
int
show_move(FILE* f, move m)
{
  if (m.flag & mf_drop) {
    fprintf(f, "%c", piece_name[m.p]+((m.flag & mf_whitemove) ? ('a'-'A') : 0));
    fprintf(f, "@");
  } else {
    fprintf(f, "%c", piece_name[m.m]+((m.flag & mf_whitemove) ? ('a'-'A'): 0));
    fprintf(f, "%c%i", 'A'+(4-(m.f%5)), 5 - m.f/5);
    fprintf(f, (m.flag & mf_cap) ? "x" : "-");
    if (m.flag & mf_cap)
      fprintf(f, "%c", piece_name[m.p]+((m.flag & mf_whitemove) ? 0 : ('a'-'A')));
  }
  fprintf(f, "%c%i", 'A'+(4-(m.t%5)), 5 - m.t/5);
  if (m.flag & mf_pro)
    fprintf(f, "+");
  else if (m.flag & mf_nonpro)
    fprintf(f, "=");
  if (m.p == king)
    fprintf(f, "#");
  return 0;
}

int
main(int argc, char* argv[])
{

  board  b;
  int    i, j;

  setvbuf(stdout, NULL, _IONBF, 0);
  srand48(-13);
  init_hash();

  if (1)
  {
  init_board(&b);
  show_board(stdout, &b);
  for (i=0; b.play==playing; ++i)
  {
    {
      move  ml[256];
      int n = generate_moves(&b, ml, 0);
      int j;
      printf("--- Moves:");
      for (j=0; j<n; ++j)
      {
        printf(" ");
        show_move(stdout, ml[j]);
        if (!((j+1)%10) && (j!=n-1)) printf("\n          ");
      }
      printf("\n");
    }
    {
      move  ml[256];
      int n = generate_moves(&b, ml, 1);
      int j;
      printf("--- Captures:");
      for (j=0; j<n; ++j)
      {
        printf(" ");
        show_move(stdout, ml[j]);
        if (!((j+1)%10) && (j!=n-1)) printf("\n             ");
      }
      printf("\n");
    }
    int black_depth = 500, white_depth = 500;
    {
      int  ev, bt, et, tt;
      nodes_evaluated = 0;
      use_tt_move = 1;
      bt = read_clock();
      init_ttable();
      ev = search_alphabeta_tt(&b, 0, (b.side==black) ? black_depth : white_depth, (loss_value-1), (win_value+1), 0);
      et = read_clock();
      tt = et - bt;
      printf("%i%s", (b.ply+1)/2, (b.ply%2)?".":"...");
      show_move(stdout, pv[0][0]);
      printf(" AlBeTT/1:  {%7i}  [nodes=%9i; time=%8.4fs; speed=%9.1fnps]\n", ev, nodes_evaluated,
          (double)tt*1e-6, nodes_evaluated/((double)tt*1e-6));
      for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
      printf("  TT: hits=%i false=%i shallow=%i deep=%i used=%i\n",
          tt_hits, tt_false, tt_shallow, tt_deep, tt_used);
    }
    {
      int  ev, bt, et, tt = 0;
      nodes_evaluated = 0;
      use_tt_move = 1;
      bt = read_clock();
      init_ttable();
      et = read_clock();
      tt += et - bt;
      int depth, max_depth = (b.side==black) ? black_depth : white_depth;
      for (depth=100; depth<=max_depth; depth+=100) {
        bt = read_clock();
        ev = search_alphabeta_tt(&b, 0, depth, (loss_value-1), (win_value+1), 0);
        et = read_clock();
        tt += et - bt;
        printf("\t{{%10i}} ", ev);
        for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
      }
      printf("%i%s", (b.ply+1)/2, (b.ply%2)?".":"...");
      show_move(stdout, pv[0][0]);
      printf(" ABTT/ID:   {%7i}  [nodes=%9i; time=%8.4fs; speed=%9.1fnps]\n", ev, nodes_evaluated,
          (double)tt*1e-6, nodes_evaluated/((double)tt*1e-6));
      for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
      printf("  TT: hits=%i false=%i shallow=%i deep=%i used=%i\n",
          tt_hits, tt_false, tt_shallow, tt_deep, tt_used);
    }
    {
      int  ev, bt, et, tt = 0;
      nodes_evaluated = 0;
      qply = 0;
      qtt = 0;
      use_tt_move = 1;
      sort_quiesce = 1;
      qfutprune = 1;
      bt = read_clock();
      init_ttable();
      et = read_clock();
      tt += et - bt;
      int depth, max_depth = (b.side==black) ? black_depth : white_depth;
      for (depth=100; depth<=max_depth; depth+=100) {
        bt = read_clock();
        ev = search_alphabeta_tt(&b, 0, depth, (loss_value-1), (win_value+1), 1);
        et = read_clock();
        tt += et - bt;
        printf("\t{{%10i}} ", ev);
        for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
      }
      printf("%i%s", (b.ply+1)/2, (b.ply%2)?".":"...");
      show_move(stdout, pv[0][0]);
      printf(" ABTT/ID/Q+ {%7i}  [nodes=%9i; time=%8.4fs; speed=%9.1fnps]\n", ev, nodes_evaluated,
          (double)tt*1e-6, nodes_evaluated/((double)tt*1e-6));
      for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
      printf("  TT: hits=%i false=%i shallow=%i deep=%i used=%i\n",
          tt_hits, tt_false, tt_shallow, tt_deep, tt_used);
      printf(" max ply reached = %i\n", qply);
    }
    {
      int  ev, bt, et, tt = 0;
      nodes_evaluated = 0;
      qply = 0;
      qtt = 1;
      use_tt_move = 1;
      sort_quiesce = 1;
      qfutprune = 1;
      bt = read_clock();
      init_ttable();
      et = read_clock();
      tt += et - bt;
      int depth, max_depth = (b.side==black) ? black_depth : white_depth;
      for (depth=100; depth<=max_depth; depth+=100) {
        bt = read_clock();
        ev = search_alphabeta_tt(&b, 0, depth, (loss_value-1), (win_value+1), 1);
        et = read_clock();
        tt += et - bt;
        printf("\t{{%10i}} ", ev);
        for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
      }
      printf("%i%s", (b.ply+1)/2, (b.ply%2)?".":"...");
      show_move(stdout, pv[0][0]);
      printf(" ABTT/ID/Qtt+ {%7i}  [nodes=%9i; time=%8.4fs; speed=%9.1fnps]\n", ev, nodes_evaluated,
          (double)tt*1e-6, nodes_evaluated/((double)tt*1e-6));
      for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
      printf("  TT: hits=%i false=%i shallow=%i deep=%i used=%i\n",
          tt_hits, tt_false, tt_shallow, tt_deep, tt_used);
      printf(" max ply reached = %i\n", qply);
    }
    {
      int  ev, bt, et, tt = 0;
      nodes_evaluated = 0;
      qply = 0;
      qtt = 1;
      use_tt_move = 1;
      sort_quiesce = 1;
      qfutprune = 1;
      bt = read_clock();
      init_ttable();
      et = read_clock();
      tt += et - bt;
      int depth, max_depth = (b.side==black) ? black_depth : white_depth;
      int window_low = loss_value - 1;
      int window_high = win_value + 1;
      for (depth=100; depth<=max_depth; depth+=100) {
        bt = read_clock();
        ev = search_alphabeta_tt(&b, 0, depth, window_low, window_high, 1);
        et = read_clock();
        tt += et - bt;
        printf("\t[%10i,%10i] {{%10i}} (%8.4fs) ", window_low, window_high, ev, ((et-bt)*1e-6));
        for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
        if (ev <= window_low) {
          window_low = loss_value - 1;
        } else if (ev >= window_high) {
          window_high = win_value + 1;
          depth -= 100;
        } else {
          window_low = ev - 50;
          window_high = ev + 50;
        }
      }
      printf("%i%s", (b.ply+1)/2, (b.ply%2)?".":"...");
      show_move(stdout, pv[0][0]);
      printf(" ABTT/ID2/Qtt+/Asp(50) {%7i}  [nodes=%9i; time=%8.4fs; speed=%9.1fnps]\n", ev, nodes_evaluated,
          (double)tt*1e-6, nodes_evaluated/((double)tt*1e-6));
      for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
      printf("  TT: hits=%i false=%i shallow=%i deep=%i used=%i\n",
          tt_hits, tt_false, tt_shallow, tt_deep, tt_used);
      printf(" max ply reached = %i\n", qply);
    }
    {
      int  ev, bt, et, tt = 0;
      nodes_evaluated = 0;
      qply = 0;
      qtt = 1;
      use_tt_move = 1;
      sort_quiesce = 1;
      qfutprune = 1;
      bt = read_clock();
      init_ttable();
      et = read_clock();
      tt += et - bt;
      int depth, max_depth = (b.side==black) ? black_depth : white_depth;
      int window_low = loss_value - 1;
      int window_high = win_value + 1;
      for (depth=100; depth<=max_depth; depth+=100) {
        bt = read_clock();
        ev = search_alphabeta_tt(&b, 0, depth, window_low, window_high, 1);
        et = read_clock();
        tt += et - bt;
        printf("\t[%10i,%10i] {{%10i}} (%8.4fs) ", window_low, window_high, ev, ((et-bt)*1e-6));
        for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
        if (ev <= window_low) {
          window_low = loss_value - 1;
          depth -= 100;
        } else if (ev >= window_high) {
          window_high = win_value + 1;
          depth -= 100;
        } else {
          window_low = ev - 10;
          window_high = ev + 10;
        }
      }
      printf("%i%s", (b.ply+1)/2, (b.ply%2)?".":"...");
      show_move(stdout, pv[0][0]);
      printf(" ABTT/ID2/Qtt+/Asp(10) {%7i}  [nodes=%9i; time=%8.4fs; speed=%9.1fnps]\n", ev, nodes_evaluated,
          (double)tt*1e-6, nodes_evaluated/((double)tt*1e-6));
      for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
      printf("  TT: hits=%i false=%i shallow=%i deep=%i used=%i\n",
          tt_hits, tt_false, tt_shallow, tt_deep, tt_used);
      printf(" max ply reached = %i\n", qply);
    }
    {
      int  ev, bt, et, tt = 0;
      nodes_evaluated = 0;
      qply = 0;
      qtt = 1;
      use_tt_move = 1;
      sort_quiesce = 1;
      qfutprune = 1;
      bt = read_clock();
      init_ttable();
      et = read_clock();
      tt += et - bt;
      int depth, max_depth = (b.side==black) ? black_depth : white_depth;
      int window_low = loss_value - 1;
      int window_high = win_value + 1;
      for (depth=100; depth<=max_depth; depth+=100) {
        bt = read_clock();
        ev = search_alphabeta_tt(&b, 0, depth, window_low, window_high, 1);
        et = read_clock();
        tt += et - bt;
        printf("\t[%10i,%10i] {{%10i}} (%8.4fs) ", window_low, window_high, ev, ((et-bt)*1e-6));
        for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
        if (ev <= window_low) {
          window_low = loss_value - 1;
          depth -= 100;
        } else if (ev >= window_high) {
          window_high = win_value + 1;
          depth -= 100;
        } else {
          window_low = ev - 5;
          window_high = ev + 5;
        }
      }
      printf("%i%s", (b.ply+1)/2, (b.ply%2)?".":"...");
      show_move(stdout, pv[0][0]);
      printf(" ABTT/ID2/Qtt+/Asp(5) {%7i}  [nodes=%9i; time=%8.4fs; speed=%9.1fnps]\n", ev, nodes_evaluated,
          (double)tt*1e-6, nodes_evaluated/((double)tt*1e-6));
      for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
      printf("  TT: hits=%i false=%i shallow=%i deep=%i used=%i\n",
          tt_hits, tt_false, tt_shallow, tt_deep, tt_used);
      printf(" max ply reached = %i\n", qply);
    }
    {
      int  ev, bt, et, tt = 0;
      nodes_evaluated = 0;
      qply = 0;
      qtt = 0;
      use_tt_move = 1;
      sort_quiesce = 1;
      qfutprune = 1;
      bt = read_clock();
      init_ttable();
      et = read_clock();
      tt += et - bt;
      int depth, max_depth = (b.side==black) ? black_depth : white_depth;
      int window_low = loss_value - 1;
      int window_high = win_value + 1;
      for (depth=100; depth<=max_depth; depth+=100) {
        bt = read_clock();
        ev = search_alphabeta_tt(&b, 0, depth, window_low, window_high, 1);
        et = read_clock();
        tt += et - bt;
        printf("\t[%10i,%10i] {{%10i}} (%8.4fs) ", window_low, window_high, ev, ((et-bt)*1e-6));
        for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
        if (ev <= window_low) {
          window_low = loss_value - 1;
          depth -= 100;
        } else if (ev >= window_high) {
          window_high = win_value + 1;
          depth -= 100;
        } else {
          window_low = ev - 5;
          window_high = ev + 5;
        }
      }
      printf("%i%s", (b.ply+1)/2, (b.ply%2)?".":"...");
      show_move(stdout, pv[0][0]);
      printf(" ABTT/ID2/Q+/Asp(5) {%7i}  [nodes=%9i; time=%8.4fs; speed=%9.1fnps]\n", ev, nodes_evaluated,
          (double)tt*1e-6, nodes_evaluated/((double)tt*1e-6));
      for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
      printf("  TT: hits=%i false=%i shallow=%i deep=%i used=%i\n",
          tt_hits, tt_false, tt_shallow, tt_deep, tt_used);
      printf(" max ply reached = %i\n", qply);
    }
    {
      int  ev, bt, et, tt = 0;
      nodes_evaluated = 0;
      qply = 0;
      qtt = 1;
      use_tt_move = 1;
      sort_quiesce = 1;
      qfutprune = 1;
      bt = read_clock();
      init_ttable();
      et = read_clock();
      tt += et - bt;
      int depth, max_depth = (b.side==black) ? black_depth : white_depth;
      for (depth=100; depth<=max_depth; depth+=200) {
        bt = read_clock();
        ev = search_alphabeta_tt(&b, 0, depth, (loss_value-1), (win_value+1), 1);
        et = read_clock();
        tt += et - bt;
        printf("\t{{%10i}} ", ev);
        for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
      }
      printf("%i%s", (b.ply+1)/2, (b.ply%2)?".":"...");
      show_move(stdout, pv[0][0]);
      printf(" ABTT/ID2/Qtt+ {%7i}  [nodes=%9i; time=%8.4fs; speed=%9.1fnps]\n", ev, nodes_evaluated,
          (double)tt*1e-6, nodes_evaluated/((double)tt*1e-6));
      for (j=0; j<pv_length[0]; ++j) { printf(" "); show_move(stdout, pv[0][j]); } printf("\n");
      printf("  TT: hits=%i false=%i shallow=%i deep=%i used=%i\n",
          tt_hits, tt_false, tt_shallow, tt_deep, tt_used);
      printf(" max ply reached = %i\n", qply);
    }
    make_move(&b, pv[0][0]);
    printf("\n");
    show_board(stdout, &b);
  }
  }

  return 0;
}

/* $Id: simple-shogi.c,v 1.3 2009/11/28 05:56:49 apollo Exp apollo $ */

