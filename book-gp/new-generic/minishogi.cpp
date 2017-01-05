#include <ctype.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/*
 * 5x5 "mini-shogi" with drops and promotion
 *
 * uses single-array board with 'mailbox'
 */

namespace minishogi
{

static const int debug_dump = 0;
static const int check_incremental_hash = 0;

typedef double     weightt;

#define   b_hand  25
#define   w_hand  26
#define   mf_cap          1
#define   mf_pro          2
#define   mf_nonpro       4
#define   mf_drop         8
#define   mf_whitemove    16
#define   mf_special_move 32
#define   TTABLE_ENTRIES    (1024*1024*16)
#define   MAX_PV_LENGTH     64
#define   MAX_GAME_LENGTH   256
#define   COUNT_DOWN_NODES  (16*1024)

static const weightt win_value  = +100000000;
static const weightt loss_value = -100000000;
static       weightt draw_value = -10;

enum piecet
{
  pawn, gold, silver, bishop, rook, king,
  tokin, promoted_silver, horse, dragon,
  empty,
  badpiece = -1
};
enum colort
{
  black, white, none
};
static const char piece_name[] = {'P','G','S','B','R','K','T','N','H','D', '#'};

typedef enum state {
  playing, black_win, white_win, draw
} state;

typedef enum special_move {
  null_move
} special_move;

static const piecet  promotes[10] = {tokin,badpiece,promoted_silver,horse,dragon,badpiece, badpiece,badpiece,badpiece,badpiece};
static const piecet  unpromotes[10] = {pawn,gold,silver,bishop,rook,king, pawn,silver,bishop,rook};

typedef struct board {
  piecet piece[25];
  colort color[25];
  colort  side;
  colort  xside;
  int    hand[2][5];
  int    nhand[2];
  int  ply;
  hasht  hash;
  state  play;
  hasht  drawhash[MAX_GAME_LENGTH];
} board;

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

typedef struct time_allotment {
  double  standard;
  double  panic;
  double  hard_limit;
} time_allotment;

typedef struct time_control {
  double  base;
  double  increment;
  double  remaining;
  int     ply;
} time_control;

typedef enum ttable_flags {
  tt_valid=1, tt_exact=2, tt_upper=4, tt_lower=8, tt_have=16
} ttable_flags;

typedef struct ttable_entry {
  hasht hash;
  int   score;
  int   flags;
  int   depth;
  move  best_move;
} ttable_entry;

static ttable_entry  ttable[TTABLE_ENTRIES];

static int pv_length[MAX_PV_LENGTH];
static move  pv[MAX_PV_LENGTH][MAX_PV_LENGTH];

typedef enum proved_result {
  unproved, proved_win, proved_loss, proved_draw
} proved_result;

typedef struct book_entry {
  hasht   hash;
  int     eval;
  char    fen[64];
  struct  book_entry* parent;
  int     times_played;
  int     times_won;
  int     times_drawn;
  int     times_lost;
  proved_result   proved_value;
  int     children_expanded;
  int     num_moves;
  move    moves[256];
  struct  book_entry* children[256];
} book_entry;



typedef struct search_settings {
  int   is_human;
  int   seed;
  int   max_depth;
  int   use_null_move;
  int   null_move_reduction;
  int   adaptive_null_move_level;
  int   check_draws;
  int   use_quiescence;
  int   sort_quiesce;
  int   qfutprune;
  int   in_check_no_pat;
  int   use_pvs;
  int   use_qtt;
  int   use_qtt_deep;
  int   use_tt;
  int   use_tt_move;
  int   use_tt_bounds;
  int   use_tt_bounds_crafty;
  int   use_tt_deep;
  const double* evaluation_weights;
  const book_entry* opening_book;
  time_control    control;
  time_allotment  allotment;
} search_settings;
static search_settings the_settings;

typedef struct search_stats {
  /* statistics */
  uint64  nodes_evaluated;
  uint64  preq_nodes;
  int     ply;
  int     qply;
  uint64  tt_hits;
  uint64  tt_false;
  uint64  tt_deep;
  uint64  tt_shallow;
  uint64  tt_used_exact;
  uint64  tt_used_lower;
  uint64  tt_used_upper;
  /* search progress */
  int     count_down;
  double  start_time;
  /* control flags */
  int     stop_flag;
  int     panic_stop_only;
} search_stats;
static search_stats the_stats;

typedef struct game_entry {
  move          pv[MAX_PV_LENGTH];
  int           pv_length;
  int           eval;
  double        time;           
  search_stats  stats;
} game_entry;

typedef struct game {
  game_entry  moves[MAX_GAME_LENGTH];
  int         num_moves;
  uint64      total_nodes[2];
  double      total_time[2];
  board       initial_board;
  board       current_board;
  time_control  controls[2];
} game;


static int show_board(FILE* f, const board* b, int indent);
static int show_move(FILE* f, move m);
static char* move_to_string(move m, char* buff);
static char* board_to_fen(const board* b, char* buff);
static int fen_to_board(board* b, const char* buff);
static int search_alphabeta_tt(board* b, int ply, int depth, int alpha, int beta, int allow_null_move, int is_pv);
static int search_alphabeta_quiesce(board* b, int ply, int alpha, int beta);
static void init_stats(search_stats* s);
static void show_stats(FILE* f, const search_stats* s);
static void init_settings(search_settings* s, int seed, int depth);
static int init_game(game* g, board* b);
static int add_game_entry(game* g, move* pv, int pv_length, int eval, double time, search_stats* ss);
static int show_game_entry(FILE* f, const game_entry* ge, int ply);
static int show_game(FILE* f, const game* g, int all_boards);
static int make_null_move(board* b);
static int unmake_null_move(board* b);
static int make_move(board* b, move m);
static int unmake_move(board* b, move m);
static int evaluate_rel(const board* b, int ply);
static book_entry* make_full_book__rec(board* b, int depth);
static book_entry* make_full_book(int depth);
static int show_time_control(FILE* f, const time_control* control);
static int show_time_allotment(FILE* f, const time_allotment* allotment);
static int show_settings(FILE* f, const search_settings* ss);
static state play_game(search_settings* settings_black, search_settings* settings_white, int seed, game* g, int display_progress,
    int num_forced_moves, move* forced_moves, const char* initial_board, const book_entry* book);
static double eval_transform(double eval, double beta);
static book_entry* make_entry_from_position(board* b);
static int book_move(const book_entry* book, int num_moves, const move moves[], move* chosen_move);

static int show_settings(FILE* f, const search_settings* ss)
{
  if (ss->is_human)
  {
    fprintf(f, "Puny human");
    return 0;
  }

  fprintf(f, "Depth(%i)", ss->max_depth);

  if (ss->use_pvs) fprintf(f, "PVS()");

  if (ss->use_null_move)
  {
    fprintf(f, "Nmp(%i", ss->null_move_reduction);
    if (ss->adaptive_null_move_level)
      fprintf(f, "/%i", ss->adaptive_null_move_level);
    fprintf(f, ")");
  }

  if (ss->use_quiescence)
  {
    fprintf(f, "Qu(");
    if (ss->sort_quiesce) fprintf(f, "Sort");
    if (ss->qfutprune) fprintf(f, "Futil");
    if (ss->use_qtt) fprintf(f, "TT");
    if (ss->use_qtt_deep) fprintf(f, "Deep");
    if (ss->in_check_no_pat) fprintf(f, "Check");
    fprintf(f, ")");
  }
  if (ss->use_tt) {
    fprintf(f, "Tt(");
    if (ss->use_tt_move) fprintf(f, "Move");
    if (ss->use_tt_bounds) fprintf(f, "Bounds");
    if (ss->use_tt_bounds_crafty) fprintf(f, "Crafty");
    if (ss->use_tt_deep) fprintf(f, "Deep");
    fprintf(f, ")");
  }
  if (ss->check_draws) fprintf(f, "Draws()");

  if (ss->opening_book) fprintf(f, "Book(%08X)", (uint32)ss->opening_book);

  fprintf(f, "0x%08X", (uint32)ss->evaluation_weights);

  return 0;
}

static int init_game(game* g, board* b)
{
  memset(g, '\x00', sizeof(game));
  g->num_moves = 0;
  memcpy(&g->initial_board, b, sizeof(board));
  memcpy(&g->current_board, b, sizeof(board));
  return 0;
}
static int add_game_entry(game* g, move* pv, int pv_length, int eval, double time, search_stats* ss)
{
  game_entry* ge = &g->moves[g->num_moves++];
  memcpy(ge->pv, pv, pv_length*sizeof(move));
  ge->pv_length = pv_length;
  ge->eval = eval;
  ge->time = time;
  memcpy(&ge->stats, ss, sizeof(search_stats));
  g->total_nodes[g->current_board.side] += ss->nodes_evaluated;
  g->total_time[g->current_board.side] += time;
  g->controls[g->current_board.side].remaining -= time;
  g->controls[g->current_board.side].remaining += g->controls[g->current_board.side].increment;
  make_move(&g->current_board, pv[0]);
  return 0;
}

static int show_game_entry(FILE* f, const game_entry* ge, int ply)
{
  int i;
  for (i=0; i<ge->pv_length; ++i)
  {
    char buff[32];
    if (i==0)
      fprintf(f, "%i%s", (ply+i+1)/2, ((ply+i)%2 ? "." : "..."));
    else if ((ply+i)%2)
      fprintf(f, "%i%s", (ply+i+1)/2, ".");
    fprintf(f, "%s ", move_to_string(ge->pv[i], buff));
    if (i==0)
      fprintf(f, "( ");
  }
  fprintf(f, ")\n      {eval:%i  time:%.2fs  nps:%.1f  ",
      ge->eval, ge->time, (double)ge->stats.nodes_evaluated/ge->time);
  show_stats(f, &ge->stats);
  fprintf(f, "}\n");
  return 0;
}
static int show_game(FILE* f, const game* g, int all_boards)
{
  int i;

  fprintf(f, "\n========================================\n");
  show_board(f, &g->initial_board, 0);
  if (g->num_moves == 0)
    return 0;

  board temp_b;
  if (all_boards)
    memcpy(&temp_b, &g->initial_board, sizeof(board));

  for (i=0; i<g->num_moves; ++i)
  {
    if (all_boards && i)
      show_board(f, &temp_b, 0);
    show_game_entry(f, &g->moves[i], g->initial_board.ply + i);
    if (all_boards)
    {
      int j;
      for (j=0; j<g->moves[i].pv_length; ++j)
        make_move(&temp_b, g->moves[i].pv[j]);
      show_board(f, &temp_b, 10);
      for (j=g->moves[i].pv_length-1; j>=1; --j)
        unmake_move(&temp_b, g->moves[i].pv[j]);
    }
  }
  show_board(f, &g->current_board, 0);
  fprintf(f, " - Black  Nodes:%'14llu  Time:%8.2f  ", g->total_nodes[black], g->total_time[black]);
  show_time_control(f, &g->controls[black]); fprintf(f, "\n");
  fprintf(f, " - White  Nodes:%'14llu  Time:%8.2f  ", g->total_nodes[white], g->total_time[white]);
  show_time_control(f, &g->controls[white]); fprintf(f, "\n");
  return 0;
}


static void init_settings(search_settings* s, int seed, int depth)
{
  memset(s, '\x00', sizeof(search_settings));
  s->is_human = 0;
  s->max_depth = depth;
  s->seed = seed;
  s->sort_quiesce = 1;
  s->qfutprune = 1;
  s->use_quiescence = 1;
  s->use_null_move = 1;
  s->null_move_reduction = 200;
  s->adaptive_null_move_level = 0;
  s->use_pvs = 0;
  s->use_tt = 1;
  s->use_tt_move = 1;
  s->use_tt_bounds = 1;
  s->use_tt_bounds_crafty = 1;
  s->use_tt_deep = 1;
  s->use_qtt = 0;
  s->use_qtt_deep = 0;
  s->check_draws = 1;
  s->in_check_no_pat = 1;
}

static void init_stats(search_stats* s)
{
  memset(s, '\x00', sizeof(search_stats));
  s->nodes_evaluated = 0ULL;
  s->preq_nodes = 0ULL;
  s->ply = 0;
  s->qply = 0;
  s->tt_hits = 0;
  s->tt_false = 0;
  s->tt_deep = 0;
  s->tt_shallow = 0;
  s->tt_used_exact = 0;
  s->tt_used_lower = 0;
  s->tt_used_upper = 0;
}

static void show_stats(FILE* f, const search_stats* s) {
  fprintf(f, "Ply:%i/%i Nodes:%'llu/'%llu(%.0f%%) TT:[hits=%'llu false=%'llu(%.1f%%) shallow=%'llu deep=%'llu used=%'llu <%'llu >%'llu]",
          s->ply, s->qply, s->nodes_evaluated, s->preq_nodes, (double)s->nodes_evaluated*100.0/(double)s->preq_nodes,
          s->tt_hits, s->tt_false, s->tt_false*100.0/s->tt_hits,
          s->tt_shallow, s->tt_deep, s->tt_used_exact, s->tt_used_upper, s->tt_used_lower);
}

static void init_ttable() {
  memset(ttable, '\x00', sizeof(ttable));
}

static void put_ttable(hasht hash, int score, move m, int flags, int depth) {
  const int loc = (unsigned int)(hash%TTABLE_ENTRIES);
  ttable[loc].hash = hash;
  ttable[loc].score = score;
  ttable[loc].best_move = m;
  ttable[loc].flags = flags|tt_valid;
  ttable[loc].depth = depth;
}

static ttable_entry* get_ttable(hasht hash) {
  const int loc = (unsigned int)(hash%TTABLE_ENTRIES);
  return &ttable[loc];
}

static hasht   hash_piece[2][10][25]; /* side x piece x square */
static hasht   hash_piece_hand[2][5][3]; /* side x piece x num */
static hasht   hash_side_white;

static hasht gen_hash() {
  //return (hasht)lrand48();
  return ((hasht)lrand48())
      ^ ((hasht)lrand48()<<5) ^ ((hasht)lrand48()>>5)
      ^ ((hasht)lrand48()<<15) ^ ((hasht)lrand48()>>15)
      ^ ((hasht)lrand48()<<30) ^ ((hasht)lrand48()<<45)
      ^ ((hasht)lrand48()<<60) ^ ((hasht)lrand48()<<32);
}
static int init_hash(int seed)
{
  srand48(seed);
  int  i, j, k;
  for (i=0; i<2; ++i)
    for (j=0; j<10; ++j)
      for (k=0; k<25; ++k)
        hash_piece[i][j][k] = gen_hash();
  for (i=0; i<2; ++i)
    for (j=0; j<5; ++j)
      for (k=0; k<3; ++k)
        hash_piece_hand[i][j][k] = gen_hash();
  hash_side_white = gen_hash();
  return 0;
}

static void hash_board(board* b)
{
  hasht  hh = 0;
  int  i, j;
  if (b->side == white) {
    hh = hash_side_white;
  }
  for (i=0; i<25; ++i)
    if (b->piece[i]!=empty)
      hh = hh ^ hash_piece[b->color[i]][b->piece[i]][i];
  for (i=0; i<2; ++i)
    for (j=0; j<5; ++j)
      hh = hh ^ hash_piece_hand[i][j][b->hand[i][j]];
  b->hash = hh;
}

static const int board_to_box[25] = {
   8,  9, 10, 11, 12,
  15, 16, 17, 18, 19,
  22, 23, 24, 25, 26,
  29, 30, 31, 32, 33,
  36, 37, 38, 39, 40,
};
static const int box_to_board[49] = {
  -1, -1, -1, -1, -1, -1, -1,
  -1,  0,  1,  2,  3,  4, -1,
  -1,  5,  6,  7,  8,  9, -1,
  -1, 10, 11, 12, 13, 14, -1,
  -1, 15, 16, 17, 18, 19, -1,
  -1, 20, 21, 22, 23, 24, -1,
  -1, -1, -1, -1, -1, -1, -1,
};
/*pawn, gold, silver, bishop, rook, king, tokin, promoted_silver, horse, dragon*/
static const int move_step_num[10] = {1, 6, 5, 0, 0, 8, 6, 6, 4, 4};
static const int move_step_offset[10][8] = {
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
static const int move_slide_offset[10][5] = {
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
static const int promotion[25] = {
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

static int generate_moves(const board* b, move* ml, int caps_only)
{
  int num = 0;
  int f, t, i;
  int mult = b->side==black ? +1 : -1;
  int flg = b->side==black ? 0 : mf_whitemove;

  if (b->play != playing) return 0;

  /* generate drops
   *  drop onto empty squares
   *  can't drop pawns onto promotion squares
   *  TODO: disallow pawn drops giving checkmate ...
   *  TODO: disallow pawn drops with pawns
   */
  if (!caps_only && b->nhand[b->side])
  {
    for (i=0; i<5; ++i)
    {
      if (b->hand[b->side][i]) {
        f = b->side==black ? b_hand : w_hand;
        if (i==pawn)
        {
          for (t=0; t<25; ++t)
            if ((b->piece[t]==empty) && (promotion[t]!=b->side))
              MOVE(ml[num],f,t,i,i,flg|mf_drop,num);
        }
        else
        {
          for (t=0; t<25; ++t)
            if (b->piece[t]==empty)
              MOVE(ml[num],f,t,i,i,flg|mf_drop,num);
        }
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
          if ( ((promotion[f]==b->side) || (promotion[t]==b->side)) && (promotes[mvd]!=badpiece) )
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
          int fp = (((promotion[f]==b->side)||(promotion[t]==b->side))&&(promotes[mvd]!=badpiece)) ? mf_pro : 0;
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

static const piecet initial_board_piece[27] = {
  king, gold, silver, bishop, rook,
  pawn, empty, empty, empty, empty,
  empty, empty, empty, empty, empty,
  empty, empty, empty, empty, pawn,
  rook, bishop, silver, gold, king,
  empty,empty
};
static const colort initial_board_color[27] = {
  black, black, black, black, black,
  black, none, none, none, none,
  none, none, none, none, none,
  none, none, none, none, white,
  white, white, white, white, white,
  black,white
};

static int init_board(board* b)
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
  b->drawhash[0] = b->hash;
  return 0;
}

static void periodic_check(const board* b, int ply) {
  the_stats.count_down = COUNT_DOWN_NODES;
  double et = read_clock();
  double tt = et - the_stats.start_time;
  if (tt > the_settings.allotment.panic)
  {
    printf("!");
    the_stats.stop_flag = 2;
  }
  else if (!the_stats.panic_stop_only && (tt > the_settings.allotment.standard))
  {
    printf("?");
    the_stats.stop_flag = 1;
  }
}

/* ************************************************************ */
/* Evaluation */
/* ************************************************************ */
#define NUM_WEIGHTS (10*25+5+1)
static const weightt initial_weights__piece_value[10] = {
  /* pawn, gold, silver, bishop, rook, king, tokin, promoted_silver, horse, dragon, */
  100, 300, 200, 400, 500, 1000, 150, 250, 500, 600,
};
static const weightt initial_weights__piece_value_hand[5] = {
  /* pawn, gold, silver, bishop, rook, */
  100*11/12, 300*11/12, 200*11/12, 400*11/12, 500*11/12,
};
static const weightt initial_weights__king_tropism[1] = {
  10,
};
static weightt initial_weights[NUM_WEIGHTS];

static const weightt* piece_value = initial_weights;
static const weightt* piece_value_hand = initial_weights + 10*25;
static const weightt* king_tropism = initial_weights + 10*25 + 5;

static int setup_initial_weights()
{
  int i, j;
  double* dp = initial_weights;
  for (i=0; i<25; ++i)
    for (j=0; j<10; ++j)
      *dp++ = initial_weights__piece_value[j];
  for (j=0; j<5; ++j)
    *dp++ = initial_weights__piece_value_hand[j];
  for (j=0; j<1; ++j)
    *dp++ = initial_weights__king_tropism[j];
  return 0;
}

#define PIECE_VALUE_BLACK(rc,p) (piece_value[(   (rc))*10 + (p)])
#define PIECE_VALUE_WHITE(rc,p) (piece_value[(24-(rc))*10 + (p)])

static int set_evaluation_weights(const weightt* weights)
{
  piece_value = weights;
  piece_value_hand = weights + 10*25;
  king_tropism = weights + 10*25 + 5;
  return 0;
}
static int evaluate_rel(const board* b, int ply)
{
  int i;
  const int sgn = (b->side == black) ? +1 : -1;
  weightt sum = 0;

  ++the_stats.nodes_evaluated;
  if (--the_stats.count_down < 0)
    periodic_check(b,ply);

  /* terminal */
  if (b->play!=playing)
  {
    if (b->play==black_win) return sgn*(win_value - ply);
    else if (b->play==white_win) return sgn*(loss_value + ply);
    else /*if (b->play==draw)*/ return sgn*(draw_value);
  }

  /* find kings */
  int bk=0, wk=0;
  for (i=0; i<25; ++i)
  {
    if (b->piece[i]==king)
    {
      if (b->color[i]==black)
        bk = i;
      else
        wk = i;
    }
  }

  /* material */
  for (i=0; i<25; ++i) {
    if (b->piece[i]!=empty) {
      if (b->color[i] == black)
        sum += PIECE_VALUE_BLACK(i,b->piece[i]);
      else
        sum -= PIECE_VALUE_WHITE(i,b->piece[i]);
    }
  }
  for (i=0; i<5; ++i)
  {
    sum += b->hand[black][i] * piece_value_hand[i];
    sum -= b->hand[white][i] * piece_value_hand[i];
  }

  /* king tropism */
  for (i=0; i<25; ++i)
  {
    if (b->piece[i]!=empty && b->piece[i]!=king)
    {
      if (b->color[i] == black)
      {
        int d1 = ((wk%5)-(i%5));
        int d2 = ((wk/5)-(i/5));
        sum -= (king_tropism[0] * sqrt(d1*d1 + d2*d2));
      }
      else
      {
        int d1 = ((bk%5)-(i%5));
        int d2 = ((bk/5)-(i/5));
        sum += (king_tropism[0] * sqrt(d1*d1 + d2*d2));
      }
    }
  }

  /* randomness */
  sum += ((weightt)(b->hash % 101) - 50.0)/10.0;

  return (int)(sgn * sum);
}

/* ************************************************************ */
/* Move */
/* ************************************************************ */

static int make_null_move(board* b)
{
  ++b->ply;
  if (b->side == white) {
    b->side = black;
    b->xside = white;
  } else {
    b->side = white;
    b->xside = black;
  }
  b->hash = b->hash ^ hash_side_white;
  b->drawhash[b->ply-1] = b->hash;
  return 0;
}

static int unmake_null_move(board* b)
{
  --b->ply;
  if (b->side == white) {
    b->side = black;
    b->xside = white;
  } else {
    b->side = white;
    b->xside = black;
  }
  b->hash = b->drawhash[b->ply - 1];
  return 0;
}

static int make_move(board* b, move m)
{
  hasht hh;

  {
    hh = b->hash ^ hash_side_white;
  }

  if (m.flag & mf_drop)
  {
    {
      hh = hh ^ hash_piece[b->side][m.p][m.t];
      hh = hh ^ hash_piece_hand[b->side][m.p][b->hand[b->side][m.p]];
      hh = hh ^ hash_piece_hand[b->side][m.p][b->hand[b->side][m.p]-1];
    }

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

    b->piece[m.t] = m.m;
    b->color[m.t] = b->side;
    b->piece[m.f] = empty;
    b->color[m.f] = none;
    if (m.flag&mf_pro)
    {
      b->piece[m.t] = promotes[m.m];
    }

    {
      if (m.flag & mf_cap) {
        hh = hh ^ hash_piece[b->xside][m.p][m.t];
        if (m.p != king) {
          hh = hh ^ hash_piece_hand[b->side][unpromotes[m.p]][b->hand[b->side][unpromotes[m.p]]];
          hh = hh ^ hash_piece_hand[b->side][unpromotes[m.p]][b->hand[b->side][unpromotes[m.p]]-1];
        }
      }
      hh = hh ^ hash_piece[b->side][m.m][m.f];
      if (m.flag & mf_pro)
        hh = hh ^ hash_piece[b->side][promotes[m.m]][m.t];
      else
        hh = hh ^ hash_piece[b->side][m.m][m.t];
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
  {
    b->hash = hh;
    if (check_incremental_hash)
    {
      hash_board(b);
      if (hh != b->hash) {
        printf("***** ***** Error incremental hashing: move = ");
        show_move(stdout, m);
        printf(" ***** *****\n");
      }
    }
  }

  /* draw by repetition check
   * lame slow approach
   * (could replace with hash for speed)
   * */
  b->drawhash[b->ply-1] = b->hash;
  if (the_settings.check_draws)
  {
    int i;
    for (i=0; i<b->ply-1; ++i)
    {
      if (b->drawhash[i] == b->hash)
      {
        b->play = draw;
        break;
      }
    }
  }

  return 0;
}

static int unmake_move(board* b, move m)
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
  b->hash = b->drawhash[b->ply - 1];
  if (check_incremental_hash)
  {
    hasht hh = b->hash;
    hash_board(b);
    if (hh != b->hash) {
      printf("***** ***** Error incremental hashing: move = ");
      show_move(stdout, m);
      printf(" ***** *****\n");
    }
  }
  return 0;
}

static int in_check(board* b, move* ml)
{
  int i, n;
  make_null_move(b);
  n = generate_moves(b, ml, 1);
  unmake_null_move(b);
  for (i=0; i<n; ++i)
    if (ml[i].p == king)
      return 1;
  return 0;
}

/* ************************************************************ */
/* Search */
/* ************************************************************ */


#define UPDATE_PV(p,m) do { \
  memcpy(&pv[(p)][1], &pv[(p)+1][0], sizeof(move)*pv_length[(p)+1]); \
  pv[(p)][0] = (m); \
  pv_length[(p)] = pv_length[(p)+1]+1; \
  } while (0)

/* no move chosen if depth<=0 */
int top_level = 0;
static int search_alphabeta_tt(board* b, int ply, int depth, int alpha, int beta, int allow_null_move, int is_pv)
{
  move  ml[256];
  int   n, v, i;
  int   have_tt_move = 0;
  move  tt_move;
  tt_move.i = 0;
  if (ply>the_stats.ply) the_stats.ply=ply;

  pv_length[ply] = 0;

  if (debug_dump||(ply<top_level)) {
    char buff[256];
    printf("%*ssearch_alphabeta_tt(%s, ply=%i, depth=%i, alpha=%i, beta=%i)\n", ply*4, "",
        board_to_fen(b,buff), ply, depth, alpha, beta);
  }
  
  /* Terminal node (W/L/D)
   * */
  if (b->play!=playing) {
    ++the_stats.preq_nodes;
    int val = evaluate_rel(b, ply);
    if (ply<top_level)
    {
      printf("%*s- terminal = %i\n", ply*4+2, "", val);
    }
    return val;
  }

  /* No more depth: evaluate or do quiescence search
   * */
  if (depth<=0) {
    ++the_stats.preq_nodes;
    int val;
    if (the_settings.use_quiescence)
    {
      val = search_alphabeta_quiesce(b, ply, alpha, beta);
    }
    else
    {
      val = evaluate_rel(b, ply);
    }
    if (ply<top_level)
    {
      printf("%*s- depth zero = %i\n", ply*4+2, "", val);
    }
    return val;
  }

  /* Look for entry in transposition table
   * */
  ttable_entry* tt = get_ttable(b->hash);
  if (the_settings.use_tt && (tt->flags & tt_valid))
  {
    ++the_stats.tt_hits;
    if (tt->hash != b->hash)
    {
      ++the_stats.tt_false;
    }
    else if (tt->depth < depth)
    {
      ++the_stats.tt_shallow;
      have_tt_move = tt->flags & tt_have;
      tt_move = tt->best_move;
    }
    else if ((tt->depth > depth) && !the_settings.use_tt_deep)
    {
      ++the_stats.tt_deep;
      have_tt_move = tt->flags & tt_have;
      tt_move = tt->best_move;
    }
    else if ((tt->flags & tt_exact) && !is_pv)
    {
      if (ply<top_level)
      {
        char buff[32];
        printf("%*s- tt hit= %i (%s)\n", ply*4+2, "", tt->score, move_to_string(tt->best_move, buff));
      }
      ++the_stats.tt_used_exact;
      pv[ply][0] = tt->best_move;
      pv_length[ply] = 1;
      return tt->score;
    }
    else if ((tt->flags & tt_lower) && !is_pv)
    {
      if (the_settings.use_tt_bounds)
      {
        if (ply<top_level) printf("%*s- tt lower_bound = %i\n", ply*4+2, "", tt->score);
        ++the_stats.tt_used_lower;
        if (the_settings.use_tt_bounds_crafty)
        {
          if (tt->score >= beta)
          {
            if (ply<top_level) printf("%*s- - cutoff\n", ply*4+2, "");
            return beta;
          }
        }
        else
        {
          if (tt->score > alpha)
          {
            alpha = tt->score;
            if (alpha >= beta)
            {
              if (ply<top_level) printf("%*s- - cutoff\n", ply*4+2, "");
              return alpha;
            }
          }
        }
      }
    }
    else if ((tt->flags & tt_upper) && !is_pv)
    {
      if (the_settings.use_tt_bounds)
      {
        if (ply<top_level) printf("%*s- tt upper_bound = %i\n", ply*4+2, "", tt->score);
        ++the_stats.tt_used_upper;
        if (the_settings.use_tt_bounds_crafty)
        {
          if (tt->score <= alpha)
          {
            if (ply<top_level) printf("%*s- - cutoff\n", ply*4+2, "");
            return alpha;
          }
        }
        else
        {
          if (tt->score < beta)
          {
            beta = tt->score;
            if (alpha >= beta)
            {
              if (ply<top_level) printf("%*s- - cutoff\n", ply*4+2, "");
              return beta;
            }
          }
        }
      }
    }
  }

  /* Nullmove pruning
   * */
  int null_move_reduction =
    the_settings.adaptive_null_move_level
      ? (100 + the_settings.null_move_reduction * (depth/the_settings.adaptive_null_move_level))
      : the_settings.null_move_reduction;
  if (the_settings.use_null_move && allow_null_move && (depth > null_move_reduction+100))
  {
    double bt, et;
    uint64 pren, postn;
    if (ply<top_level) {
      bt = read_clock();
      pren = the_stats.nodes_evaluated;
    }
    if (!in_check(b, ml))
    {
      make_null_move(b);
      int v = -search_alphabeta_tt(b, ply+1, depth-100-null_move_reduction, -beta, -beta+1, 0, 0);
      unmake_null_move(b);
      if (ply<top_level) printf("%*s- null-move result = %i\n", ply*4+2, "", v);
      if (v >= beta) {
        if (ply<top_level) printf("%*s- - cutoff %i >= %i\n", ply*4+2, "", v, beta);
        return beta;
      }
    }
    else
    {
      if (ply<top_level) printf("%*s- in-check/no-null-move\n", ply*4+2, "");
    }
    if (ply<top_level) {
      et = read_clock();
      postn = the_stats.nodes_evaluated;
      printf("%*s- - Null-move check: %.4fs/%'llun\n", ply*4+2, "", (et-bt), (postn-pren));
    }
  }

  n = generate_moves(b, ml, 0);

  /* Move hash-move to front of list
   * */
  if (the_settings.use_tt && the_settings.use_tt_move && have_tt_move)
  {
    for (i=0; i<n; ++i) {
      if (ml[i].i == tt_move.i) {
        move t = ml[0];
        ml[0] = ml[i];
        ml[i] = t;
      }
    }
  }

  int store_tt = 0;
  int best_v = loss_value - 1;
  for (i=0; (i<n) && !the_stats.stop_flag; ++i)
  {
    /* if we've searched the first move, then we try to finish the iteration
     * even if we exceed our normal time allotment
     * */
    if (ply==0) the_stats.panic_stop_only = (i!=0);
    double bt, et;
    uint64 pren, postn;
    if (ply<top_level)
    {
      { char buff[64]; printf("%*s%s:", ply*4+2, "", move_to_string(ml[i], buff)); }
      bt = read_clock();
      pren = the_stats.nodes_evaluated;
    }
    make_move(b,ml[i]);
    if (the_settings.use_pvs)
    {
      if (!store_tt)
      {
        if (ply<top_level) printf(" [%i,%i] ", -beta, -alpha);
        v = -search_alphabeta_tt(b, ply+1, depth-100, -beta, -alpha, 1, (i==0));
      }
      else
      {
        if (ply<top_level) printf(" [%i,%i] ", -alpha-1, -alpha);
        v = -search_alphabeta_tt(b, ply+1, depth-100, -alpha-1, -alpha, 1, (i==0));
        if (v > alpha) {
          if (ply<top_level) printf(" RE-SEARCH [%i,%i] ", -beta, -alpha);
          v = -search_alphabeta_tt(b, ply+1, depth-100, -beta, -alpha, 1, (i==0));
        }
      }
    }
    else
    {
      v = -search_alphabeta_tt(b, ply+1, depth-100, -beta, -alpha, 1, (i==0));
    }
    unmake_move(b,ml[i]);
    if (ply<top_level)
    {
      printf("%*s- Result = %i  ", ply*4+2, "", v);
      et = read_clock();
      postn = the_stats.nodes_evaluated;
      printf(" ( %.4fs / %'llun )\n", (et-bt), (postn-pren));
    }

    if (v > best_v)
      best_v = v;
    if (best_v>alpha)
    {
      if (ply<top_level) printf(" - - new best move\n");
      store_tt = 1;
      alpha = best_v;
      UPDATE_PV(ply, ml[i]);
      if (alpha >= beta)
      {
        if (the_settings.use_tt)
          put_ttable(b->hash, alpha, ml[i], tt_lower|tt_have, depth);
        if (ply<top_level) printf("%*s-=>beta cut-off a=%i >= b=%i\n", ply*4, "", alpha, beta);
        return alpha;
      }
    }
  }
  
  if (store_tt)
  {
    char buff[64];
    if (ply<top_level) printf("%*s-=> %i (%s)\n", ply*4, "", alpha, move_to_string(pv[ply][0], buff));
    if (the_settings.use_tt)
      put_ttable(b->hash, alpha, pv[ply][0], tt_exact|tt_have, depth);
  }
  else
  {
    if (ply<top_level) printf("%*s-=> alpha cut-off <= %i\n", ply*4, "", alpha);
    if (the_settings.use_tt)
      put_ttable(b->hash, alpha, pv[ply][0], tt_upper, depth);
  }
  return alpha;
}


static int search_alphabeta_quiesce(board* b, int ply, int alpha, int beta)
{
  move  ml[256], tt_move;
  tt_move.i = 0;
  int   n, v, i, static_eval;
  int   have_tt_move = 0;
  if (ply>the_stats.qply) the_stats.qply=ply;

  pv_length[ply] = 0;

  //static_eval = evaluate_rel(b, 100+ply);
  static_eval = evaluate_rel(b, ply);

  if (debug_dump) printf("%*ssearch_alphabeta_quiesce(..., ply=%i, alpha=%i, beta=%i)\n", ply*4, "", ply, alpha, beta);
  if (debug_dump) printf("%*sstatic_eval=%i\n", ply*4+2, "", static_eval);
  
  if (b->play!=playing) {
    if (debug_dump) printf("%*sGameover return = %i\n", ply*4+2, "", static_eval);
    return static_eval;
  }

  int is_in_check = 0;
  
  if (the_settings.in_check_no_pat)
    is_in_check = in_check(b, ml);

  /* "stand-pat" */
  if (!is_in_check && (static_eval>alpha))
  {
    alpha = static_eval;
    if (alpha >= beta)
    {
      if (debug_dump) printf("%*sStandpat return = %i\n", ply*4+2, "", alpha);
      return alpha;
    }
  }

  /* Look for entry in transposition table
   * */
  ttable_entry* tt = get_ttable(b->hash);
  if (the_settings.use_qtt && (tt->flags & tt_valid))
  {
    ++the_stats.tt_hits;
    if (tt->hash != b->hash)
    {
      ++the_stats.tt_false;
    }
    else if ((tt->depth > 0) && !the_settings.use_qtt_deep)
    {
      ++the_stats.tt_deep;
      have_tt_move = tt->flags & tt_have;
      tt_move = tt->best_move;
    }
    else if (tt->flags & tt_exact)
    {
      if (debug_dump)
      {
        char buff[32];
        printf("%*s- tt hit= %i (%s)\n", ply*4+2, "", tt->score, move_to_string(tt->best_move, buff));
      }
      ++the_stats.tt_used_exact;
      pv[ply][0] = tt->best_move;
      pv_length[ply] = 1;
      return tt->score;
    }
    else if (tt->flags & tt_lower)
    {
      if (the_settings.use_tt_bounds)
      {
        if (debug_dump) printf("%*s- tt lower_bound = %i\n", ply*4+2, "", tt->score);
        ++the_stats.tt_used_lower;
        if (the_settings.use_tt_bounds_crafty)
        {
          if (tt->score >= beta)
          {
            if (debug_dump) printf("%*s- - cutoff\n", ply*4+2, "");
            return beta;
          }
        }
        else
        {
          if (tt->score > alpha)
          {
            alpha = tt->score;
            if (alpha >= beta)
            {
              if (debug_dump) printf("%*s- - cutoff\n", ply*4+2, "");
              return alpha;
            }
          }
        }
      }
    }
    else if (tt->flags & tt_upper)
    {
      if (the_settings.use_tt_bounds)
      {
        if (debug_dump) printf("%*s- tt upper_bound = %i\n", ply*4+2, "", tt->score);
        ++the_stats.tt_used_upper;
        if (the_settings.use_tt_bounds_crafty)
        {
          if (tt->score <= alpha)
          {
            if (debug_dump) printf("%*s- - cutoff\n", ply*4+2, "");
            return alpha;
          }
        }
        else
        {
          if (tt->score < beta)
          {
            beta = tt->score;
            if (alpha >= beta)
            {
              if (debug_dump) printf("%*s- - cutoff\n", ply*4+2, "");
              return beta;
            }
          }
        }
      }
    }
  }

  /* if in-check, then generate -all- moves */
  if (!the_settings.in_check_no_pat)
    is_in_check = in_check(b, ml);
  if (is_in_check)
    n = generate_moves(b, ml, 0);
  else
    n = generate_moves(b, ml, 1);

  if (n==0)
    return static_eval;

  if (the_settings.sort_quiesce)
  {
    int k1, k2;
    //for (k1=0; k1<n; ++k1) { show_move(stdout, ml[k1]); printf(" "); } printf("\n");
    for (k1=0; k1<n; ++k1)
    {
      int best_k = k1;
      int best_v = (b->side == black)
        ? (100*PIECE_VALUE_WHITE(ml[k1].t,ml[k1].p) - PIECE_VALUE_BLACK(ml[k1].f,ml[k1].m)
            + ((have_tt_move && (ml[k1].i==tt_move.i)) ? 1000000 : 0))
        : (100*PIECE_VALUE_BLACK(ml[k1].t,ml[k1].p) - PIECE_VALUE_WHITE(ml[k1].f,ml[k1].m)
            + ((have_tt_move && (ml[k1].i==tt_move.i)) ? 1000000 : 0));
      for (k2=k1+1; k2<n; ++k2)
      {
        int val = (b->side == black)
          ? (100*PIECE_VALUE_WHITE(ml[k2].t,ml[k2].p) - PIECE_VALUE_BLACK(ml[k2].f,ml[k2].m)
              + ((have_tt_move && (ml[k2].i==tt_move.i)) ? 1000000 : 0))
          : (100*PIECE_VALUE_BLACK(ml[k2].t,ml[k2].p) - PIECE_VALUE_WHITE(ml[k2].f,ml[k2].m)
              + ((have_tt_move && (ml[k2].i==tt_move.i)) ? 1000000 : 0));
        if (val > best_v)
        {
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
  for (i=0; (i<n) && !the_stats.stop_flag; ++i)
  {
    if (the_settings.qfutprune && (static_eval+piece_value[ml[i].p] < alpha)) continue;
    if (debug_dump) { char buff[64]; printf("%*s%s:", ply*4+2, "", move_to_string(ml[i], buff)); }
    make_move(b,ml[i]);
    v = -search_alphabeta_quiesce(b, ply+1, -beta, -alpha);
    unmake_move(b,ml[i]);
    if (debug_dump) printf("%*s- Result = %i\n", ply*4+2, "", v);
    if (v>alpha) {
      store_tt = 1;
      alpha = v;
      UPDATE_PV(ply, ml[i]);
      if (alpha >= beta) {
        if (debug_dump) printf("%*sCutoff return = %i\n", ply*4+2, "", alpha);
        if (the_settings.use_qtt)
          put_ttable(b->hash, alpha, ml[i], tt_lower|tt_have, 0);
        return alpha;
      }
    }
  }

  if (store_tt)
  {
    char buff[64];
    if (debug_dump) printf("%*s-=> %i (%s)\n", ply*4, "", alpha, move_to_string(pv[ply][0], buff));
    if (the_settings.use_tt)
      put_ttable(b->hash, alpha, pv[ply][0], tt_exact|tt_have, 0);
  }
  else
  {
    if (debug_dump) printf("%*s-=> alpha cut-off <= %i\n", ply*4, "", alpha);
    if (the_settings.use_tt)
      put_ttable(b->hash, alpha, pv[ply][0], tt_upper, 0);
  }
  return alpha;
}

/* ************************************************************ */
/* Board */
/* ************************************************************ */

static int show_board(FILE* f, const board* b, int indent)
{
  int  r, c, rc, i, j;

  fprintf(f, "%*s   5  4  3  2  1  \n", indent, "");
  fprintf(f, "%*s +---------------+    [%016llX]\n", indent, "", b->hash);
  for (r=4; r>=0; --r) {
    fprintf(f, "%*s |", indent, "");
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
    fprintf(f, "| (%c)", ('a' + 4-r));

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
    } else if (r==3) {
      char buff[256];
      board_to_fen(b, buff);
      fprintf(f, " (%s)", buff);
    } else if (r==4) {
      fprintf(f, " {%i}", evaluate_rel(b,0));
    } else if (r==0) {
      fprintf(f, " %i.%s", (b->ply+1)/2, (b->side==black ? "" : ".."));
    }
    fprintf(f, "\n");
  }
  fprintf(f, "%*s +---------------+\n", indent, "");

  if (b->play==playing)
  {
    if (b->side==black)
      fprintf(f, "%*s Black to move\n", indent, "");
    else
      fprintf(f, "%*s White to move\n", indent, "");
  }
  else
  {
    if (b->play==black_win)
      fprintf(f, "%*s Black wins\n", indent, "");
    else if (b->play==white_win)
      fprintf(f, "%*s White wins\n", indent, "");
    else 
      fprintf(f, "%*s Draw\n", indent, "");
  }

  return 0;
}

/* assumes _perfect_ fen input! */
static int fen_to_board(board* b, const char* fen)
{
  int w_king = 0, b_king = 0;
  b->ply = 1;
  {
    int r=4, c=0;
    for (; *fen && (*fen!=' '); ++fen)
    {
      if (*fen=='/')
      {
        --r;
        c = 0;
      }
      else if (('1' <= *fen) && (*fen <= '5'))
      {
        int i;
        for (i=0; i<(*fen-'0'); ++i)
        {
          int rc = r*5 + c;
          b->color[rc] = none;
          b->piece[rc] = empty;
          ++c;
        }
      }
      else
      {
        int rc = r*5 + c, i;
        b->color[rc] = isupper((uint)*fen) ? black : white;
        for (i=0; piece_name[i]!='#'; ++i)
          if (toupper((uint)*fen) == piece_name[i])
            b->piece[rc] = i;
        if (*fen == 'k') w_king = 1;
        else if (*fen == 'K') b_king = 1;
        ++c;
      }
    }
    ++fen;
    if (*fen++ == 'b')
    {
      b->side = black;
      b->xside = white;
    }
    else
    {
      b->side = white;
      b->xside = black;
    }
    ++fen;
    b->nhand[0] = b->nhand[1] = 0;
    memset(b->hand, 0, sizeof(b->hand));
    if (*fen == '-')
    {
      ++fen;
    }
    else
    {
      for (; *fen && (*fen!=' '); ++fen)
      {
        int i;
        colort col = isupper((uint)*fen) ? black : white;
        for (i=0; piece_name[i]!='#'; ++i)
        {
          if (toupper((uint)*fen) == piece_name[i]) {
            ++b->nhand[col];
            ++b->hand[col][i];
          }
        }
      }
    }
  }

  hash_board(b);
  b->drawhash[0] = b->hash;

  if (b_king && w_king)
    b->play = playing;
  else if (b_king)
    b->play = black_win;
  else if (w_king)
    b->play = white_win;
  else
    b->play = draw;
  return 0;
}

static char* board_to_fen(const board* b, char* fen)
{
  char* save_fen = fen;
  int r, c, blanks;
  for (r=4; r>=0; --r)
  {
    blanks = 0;
    for (c=0; c<5; ++c)
    {
      int rc = r*5+c;
      if (b->piece[rc] == empty)
      {
        ++blanks;
      }
      else
      {
        if (blanks != 0) *fen++ = '0'+blanks;
        blanks = 0;
        if (b->color[rc] == black)
          *fen++ = piece_name[b->piece[rc]];
        else
          *fen++ = piece_name[b->piece[rc]]+'a'-'A';
      }
    }
    if (blanks != 0) *fen++ = '0'+blanks;
    if (r!=0) *fen++ = '/';
  }
  *fen++ = ' ';
  *fen++ = (b->side==black) ? 'b' : 'w';
  *fen++ = ' ';
  if ((b->nhand[black] + b->nhand[white]) == 0)
  {
    *fen++ = '-';
  }
  else
  {
    int i, j;
    for (i=0; i<5; ++i)
      for (j=0; j<b->hand[black][i]; ++j)
        *fen++ = piece_name[i];
    for (i=0; i<5; ++i)
      for (j=0; j<b->hand[white][i]; ++j)
        *fen++ = piece_name[i] + 'a' - 'A';
  }
  //*fen++ = ' ';
  *fen = '\x00';
  return save_fen;
}

static int show_moves(FILE* f, board* b)
{
  char  buff[32];
  move  ml[256];
  int j, n;
  n = generate_moves(b, ml, 0);
  for (j=0; j<n; ++j)
  {
    fprintf(f, " %2i:%-8s", j, move_to_string(ml[j], buff));
    if (!((j+1)%10) && (j!=n-1)) printf("\n");
  }
  fprintf(f, "\n");
  return 0;
}

static int show_move(FILE* f, move m) {
  char buff[64];
  return fprintf(f, "%s", move_to_string(m, buff));
}

static char* move_to_string(move m, char* buff)
{
  char* save_buff = buff;
  if (m.flag & mf_drop)
  {
    *buff++ = piece_name[m.p]+((m.flag & mf_whitemove) ? ('a'-'A') : 0);
    *buff++ = '*';
  }
  else
  {
    *buff++ = piece_name[m.m]+((m.flag & mf_whitemove) ? ('a'-'A'): 0);
    *buff++ = '1' + (4 - (m.f%5));
    *buff++ = 'a' + (4 - (m.f/5));
    *buff++ = (m.flag & mf_cap) ? 'x' : '-';
    if (m.flag & mf_cap)
      *buff++ = piece_name[m.p]+((m.flag & mf_whitemove) ? 0 : ('a'-'A'));
  }
  *buff++ = '1' + (4 - (m.t%5));
  *buff++ = 'a' + (4 - (m.t/5));
  if (m.flag & mf_pro)
    *buff++ = '+';
  else if (m.flag & mf_nonpro)
    *buff++ = '=';
  if (m.p == king)
    *buff++ = '#';
  *buff++ = '\x00';
  return save_buff;
}

/* ************************************************************ */
/* Opening book */
/* ************************************************************ */

static double uct(const book_entry* book)
{
  if (!book || !book->parent || !book->parent->times_played || !book->times_played)
    return (1.0/0.0);
  const double cp = 1/sqrt(2.0);
  int num_parent = book->parent->times_played;
  int num_child = book->times_played;
  int won = book->times_won;
  int lost = book->times_lost;
  int proved = book->proved_value;

  if (proved)
  {
    if (proved == proved_win)
      return (-1.0/0.0);
    else if (proved == proved_loss)
      return (+1.0/0.0);
    /* draws... */
  }

  return -eval_transform(book->eval, 0.50/100)/num_child - (double)(won-lost)/num_child + cp * sqrt(2*log(num_parent) / num_child);
}

static int show_book__rec__compare(const void* a, const void* b)
{
  double da = ((const double*)a)[1];
  double db = ((const double*)b)[1];
  if (da>db) return -1;
  else if (da<db) return +1;
  else return 0;
}
static int show_book__rec(FILE* f, const book_entry* book, int indent)
{
  char  buff[32];
  int   i;
  fprintf(f, "[%-32s | %016llX] <%f> (%.2f%% = +%i=%i-%i/%i) %i {%i ~ %.2f}\n", book->fen, book->hash, uct(book),
      100.0 * (book->times_won - book->times_lost) / book->times_played,
      book->times_won, book->times_drawn, book->times_lost, book->times_played,
      book->proved_value, book->eval, eval_transform(book->eval, 0.50/100)/book->times_played);
  if (book->children_expanded)
  {
    double ucts[book->num_moves][2];
    for (i=0; i<book->num_moves; ++i)
    {
      ucts[i][0] = i;
      ucts[i][1] = uct(book->children[i]);
    }
    qsort(ucts, book->num_moves, 2*sizeof(double), &show_book__rec__compare);

    for (i=0; i<book->num_moves; ++i)
    {
      int idx = (int)ucts[i][0];
      fprintf(f, "%*s%-8s ", indent+2, "", move_to_string(book->moves[idx], buff));
      if (book->children[idx])
        show_book__rec(f, book->children[idx], indent+4);
      else
        fprintf(f, "\n");
    }
  }
  return 0;
}
static int show_book(FILE* f, const book_entry* book)
{
  return show_book__rec(f, book, 0);
}


static book_entry* load_book__rec(FILE* f, board* b)
{
  book_entry* book = make_entry_from_position(b);
  char fen[64];
  int won, drawn, lost, played, expanded, proved;
  fscanf(f, "[%[^]]] %i %i %i %i %i %i", fen, &won, &drawn, &lost, &played, &expanded, &proved);
  book->times_won = won;
  book->times_drawn = drawn;
  book->times_lost = lost;
  book->times_played = played;
  book->children_expanded = expanded;
  book->proved_value = proved;
  if (expanded)
  {
    int i;
    for (i=0; i<book->num_moves; ++i)
    {
      char  buff[64];
      fscanf(f, "%s ", buff);
      make_move(b, book->moves[i]);
      book_entry* child = load_book__rec(f, b);
      unmake_move(b, book->moves[i]);
      book->children[i] = child;
      child->parent = book;
    }
  }
  return book;
}

static book_entry* load_book(FILE* f)
{
  board b;
  init_hash(0);
  init_board(&b);
  return load_book__rec(f, &b);
}

static int save_book__rec(FILE* f, const book_entry* book, int indent)
{
  char  buff[32];
  int   i;
  fprintf(f, "[%s] %i %i %i %i %i %i\n",
      book->fen, book->times_won, book->times_drawn, book->times_lost, book->times_played, book->children_expanded, book->proved_value);
  if (book->children_expanded)
  {
    for (i=0; i<book->num_moves; ++i)
    {
      fprintf(f, "%*s%s ", indent, "", move_to_string(book->moves[i], buff));
      if (book->children[i])
        save_book__rec(f, book->children[i], indent+2);
      else
        fprintf(f, "\n");
    }
  }
  return 0;
}
static int save_book(FILE* f, const book_entry* book)
{
  return save_book__rec(f, book, 0);
}

static book_entry* make_entry_from_position(board* b)
{
  char  buff[256];
  book_entry* book = (book_entry*)calloc(1, sizeof(book_entry));
  int n = generate_moves(b, book->moves, 0);
  book->hash = b->hash;
  book->eval = evaluate_rel(b, 0);
  strcpy(book->fen, board_to_fen(b, buff));
  book->num_moves = n;
  if (b->play != playing)
  {
    if (b->play==draw)
      book->proved_value = proved_draw;
    else if (((b->play==black_win) && (b->side==black)) || ((b->play==white_win) && (b->side==white)))
      book->proved_value = proved_win;
    else
      book->proved_value = proved_loss;
  }
  return book;
}

static book_entry* make_full_book__rec(board* b, int depth)
{
  book_entry* book = make_entry_from_position(b);

  if ((depth == 0) || (b->play != playing))
    return book;

  int i, n = book->num_moves;
  book->children_expanded = 1;
  for (i=0; i<n; ++i)
  {
    move m = book->moves[i];
    make_move(b, m);
    book->children[i] = make_full_book__rec(b, depth-1);
    book->children[i]->parent = book;
    unmake_move(b, m);
  }
  return book;
}

static book_entry* make_full_book(int depth)
{
  board b;
  init_hash(0);
  init_board(&b);
  //const char* mate_in_2 = "1bRk1/1S1Bp/1KS2/4g/d1G2 w p";
  //fen_to_board(&b, mate_in_2);
  book_entry* book = make_full_book__rec(&b, depth);
  return book;
}

static void update_book_entry(book_entry* book, int result)
{
  while (book)
  {
    if (result == +1)
    {
      ++(book->times_won);
      result = -1;
    }
    else if (result == 0)
    {
      ++(book->times_drawn);
    }
    else if (result == -1)
    {
      ++(book->times_lost);
      result = +1;
    }
    else if (result == +2)
    {
      ++(book->times_won);
      book->proved_value = proved_win;
      result = -3;
    }
    else if (result == -2)
    {
      ++(book->times_lost);
      book->proved_value = proved_loss;
      result = +2;
    }
    else if (result == -3)
    {
      /* one child is proved win for opponent
       *   - if all children are such, then we have a proved loss
       *   - otherwise, just record a regular loss
       * */
      int i;
      int all_children_proved = 1;
      for (i=0; i<book->num_moves; ++i)
      {
        if (book->children[i]->proved_value != proved_win)
        {
          all_children_proved = 0;
          break;
        }
      }
      if (all_children_proved)
      {
        ++(book->times_lost);
        book->proved_value = proved_loss;
        result = +2;
      }
      else
      {
        ++(book->times_lost);
        result = +1;
      }
    }
    ++(book->times_played);
    book = book->parent;
  }
}

static book_entry* find_uct_optimal(book_entry* current_book, board* b, move moves[], int* num_moves)
{
  if (!current_book->children_expanded || current_book->proved_value)
    return current_book;

  int i;
  int optimal_i = 0;
  int num_equals = 0;
  double  optimal_uct = -1.0/0.0/*-Inf*/;
  for (i=0; i<current_book->num_moves; ++i)
  {
    double this_uct = uct(current_book->children[i]);
    if (this_uct > optimal_uct)
    {
      optimal_i = i;
      optimal_uct = this_uct;
      num_equals = 1;
    }
    else if (this_uct == optimal_uct)
    {
      /* choose uniformly randomly among equal possibilities */
      ++num_equals;
      if (!(lrand48() % num_equals))
      {
        optimal_i = i;
        optimal_uct = this_uct;
      }
    }
  }
  { char  buff[32]; printf("Current: %s, choosing %s\n", current_book->fen, move_to_string(current_book->moves[optimal_i], buff)); }
  make_move(b, current_book->moves[optimal_i]);
  moves[(*num_moves)++] = current_book->moves[optimal_i];
  return find_uct_optimal(current_book->children[optimal_i], b, moves, num_moves);
}

static book_entry* make_uct_book(book_entry* current_book, int num_expansions, int num_paths, int base_seed, const search_settings* player)
{
  int expansion, path;
  if (!current_book)
    current_book = make_full_book(0);

  printf("***** ***** ***** Initial book: ***** ***** *****\n");
  show_book(stdout, current_book);

  board b;
  move  forced_moves[256];
  for (expansion=0; expansion<num_expansions; ++expansion)
  {
    printf("\n***** ***** Expansion #%i ***** *****\n", expansion);
    fen_to_board(&b, current_book->fen);
    int num_moves = 0;
    book_entry* book = find_uct_optimal(current_book, &b, forced_moves, &num_moves);

    if (book->proved_value)
    {
      printf("Node [%s] has proved value: %+i\n", book->fen, book->proved_value);
      if (book->proved_value == proved_win)
        update_book_entry(book, +2);
      else if (book->proved_value == proved_loss)
        update_book_entry(book, -2);
      else
        update_book_entry(book, 0);
      continue;
    }

    /* expand all children of node */
    int i, n = book->num_moves;
    book->children_expanded = 1;
    for (i=0; i<n; ++i)
    {
      move m = book->moves[i];
      make_move(&b, m);
      book->children[i] = make_entry_from_position(&b);
      book->children[i]->parent = book;

      { char buff[32]; printf("%s : ", move_to_string(m, buff)); }
      if (b.play != playing)
      {
        if (b.play == black_win)
          update_book_entry(book->children[i], b.side==black ? +2 : -2);
        else if (b.play == white_win)
          update_book_entry(book->children[i], b.side==white ? +2 : -2);
        else
          update_book_entry(book->children[i], 0);
        printf("%s\n", (b.play==black_win) ? "Black won" : (b.play==white_win) ? "White won" : "Drawn");
      }
      else
      {
        forced_moves[num_moves] = m;
        for (path=0; path<num_paths; ++path)
        {
          search_settings black_settings=*player, white_settings=*player;
          game g;
          state result = play_game(&black_settings, &white_settings, base_seed + 133*expansion+17*path, &g, 0, num_moves+1, forced_moves, current_book->fen, 0);
          if (result == black_win)
            update_book_entry(book->children[i], b.side==black ? +1 : -1);
          else if (result == white_win)
            update_book_entry(book->children[i], b.side==white ? +1 : -1);
          else
            update_book_entry(book->children[i], 0);
          printf("%s\n", (result==black_win) ? "Black wins" : (result==white_win) ? "White wins" : "Draw");
          //show_game(stdout, &g, 0);
        }
      }
      unmake_move(&b, m);
    }
    printf("----- Updated book: -----\n");
    show_book(stdout, current_book);

    {
      FILE* f = fopen("book_temp.txt", "w");
      if (f)
      {
        save_book(f, current_book);
        fclose(f);
      }
    }
  }
  return current_book;
}

static int book_move(const book_entry* book, int num_moves, const move moves[], move* chosen_move)
{
  {
    /* follow line in book */
    int current_move;
    for (current_move=0; current_move<num_moves; ++current_move)
    {
      int i;
      for (i=0; i<book->num_moves; ++i)
        if (book->moves[i].i == moves[current_move].i)
          break;
      /* move is somehow not in book */
      if (i >= book->num_moves)
        return 0;

      book = book->children[i];
      if (!book)
        break;
    }

    /* out of book */
    if ((current_move < num_moves)
        || !book->children_expanded)
      return 0;
  }

  {
    /* end of given line, choose move from book */
    int i;
    int optimal_i = 0;
    int num_equals = 0;
    double  optimal_uct = -1.0/0.0/*-Inf*/;
    for (i=0; i<book->num_moves; ++i)
    {
      double this_uct = uct(book->children[i]);
      //{ char buff[32]; printf("%s=%.2f ", move_to_string(book->moves[i], buff), this_uct); }
      if (this_uct > optimal_uct)
      {
        optimal_i = i;
        optimal_uct = this_uct;
        num_equals = 1;
      }
      else if (this_uct == optimal_uct)
      {
        /* choose uniformly randomly among equal possibilities */
        ++num_equals;
        if (!(lrand48() % num_equals))
        {
          optimal_i = i;
          optimal_uct = this_uct;
        }
      }
    }
    //{ printf("\n"); }
    *chosen_move = book->moves[optimal_i];
    return 1;
  }
}

/* ************************************************************ */
/* Time control */
/* ************************************************************ */

static int show_time_control(FILE* f, const time_control* control) {
  fprintf(f, "%.2f(%.0f+%g)@%i", control->remaining, control->base, control->increment, control->ply);
  return 0;
}

static int show_time_allotment(FILE* f, const time_allotment* allotment) {
  fprintf(f, "%.2f/%.2f/%.2f", allotment->standard, allotment->panic, allotment->hard_limit);
  return 0;
}

int compute_time_allotment(time_allotment* allotment, const time_control* control)
{
  double  min_time = (control->base/20) < 1.0 ? (control->base/20) : 1.0;

  int     expected_moves = (control->ply < 15) ? (20 - control->ply) : 8;

  double  standard_time = (control->remaining / expected_moves) + 0.5*control->increment;
  if (standard_time < min_time) standard_time = min_time;
  if (standard_time >= control->remaining*0.2) standard_time = control->remaining*0.2;

  //double  panic_time = 6.0 * standard_time;
  double  panic_time = 4.0 * standard_time;
  if (panic_time >= control->remaining*0.9) panic_time = control->remaining*0.9;

  double  limit_time = control->remaining*0.95;

  allotment->standard = standard_time;
  allotment->panic = panic_time;
  allotment->hard_limit = limit_time;

  return 0;
}

/* ************************************************************ */
/* TD(lambda) learning */
/* ************************************************************ */

static double eval_transform(double eval, double beta)
{
  return tanh(beta * eval);
}
static void compute_weight_partials(const board* b, double beta, int num_weights, const double weights[], double partials[])
{
  int i;
  double temp_weights[num_weights];
  memcpy(temp_weights, weights, num_weights*sizeof(double));
  for (i=0; i<num_weights; ++i)
  {
    double  h = 0.25;
    double  pv, nv;

    temp_weights[i] = weights[i] + h;
    set_evaluation_weights(temp_weights);
    pv = eval_transform((b->side==black?1:-1)*evaluate_rel(b, 1), beta);
    temp_weights[i] = weights[i] - h;
    set_evaluation_weights(temp_weights);
    nv = eval_transform((b->side==black?1:-1)*evaluate_rel(b, 1), beta);
    temp_weights[i] = weights[i];

    partials[i] = (pv - nv) / (2*h);
  }

  set_evaluation_weights(weights);
}

static int td_lambda_process_game(const game* g, double lambda, double alpha, double beta, int num_weights, const double weights[], double new_weights[])
{
  int i, j;
  int num_states = g->num_moves + 2;

  double  evals[num_states];
  double  evals_xform[num_states];
  double  diffs[num_states];
  double  weight_partials[num_states][num_weights];

  switch (g->current_board.play)
  {
    case black_win: evals_xform[num_states-1] = +1.0; break;
    case white_win: evals_xform[num_states-1] = -1.0; break;
    default:        evals_xform[num_states-1] =  0.0; break;
  }
  switch (g->current_board.play)
  {
    case black_win: evals_xform[num_states-2] = +1.0; break;
    case white_win: evals_xform[num_states-2] = -1.0; break;
    default:        evals_xform[num_states-2] =  0.0; break;
  }

  board b;
  memcpy(&b, &g->initial_board, sizeof(board));
  for (i=0; i<num_states-2; ++i) {
    int j;
    /* get board at end of pv */
    for (j=0; j<g->moves[i].pv_length; ++j) make_move(&b, g->moves[i].pv[j]);

    /* evaluate board at end-of-pv */
    evals[i] = (b.side==black?1:-1)*evaluate_rel(&b, 1);
    evals_xform[i] = eval_transform(evals[i], beta);

    /* get partial-derivative wrt weights */
    compute_weight_partials(&b, beta, num_weights, weights, weight_partials[i]);

    /* undo all moves except the first */
    for (j=g->moves[i].pv_length-1; j>0; --j) unmake_move(&b, g->moves[i].pv[j]);
  }

  for (i=0; i<num_states-2; ++i)
  {
    diffs[i] = evals_xform[i+2] - evals_xform[i];
  }

  for (j=0; j<num_weights; ++j) {
    double  sum = 0.0;
    for (i=0; i<num_states-2; ++i)
    {
      int m;
      double lam_sum = 0.0;
      for (m=i; m<num_states-2; m+=2)
        lam_sum += pow(lambda, m-i)*diffs[m];
      sum += weight_partials[i][j] * lam_sum;
    }
    new_weights[j] = weights[j] + alpha * sum;
  }

  return 0;
}

/* ************************************************************ */
/* Top level */
/* ************************************************************ */

void searcher(board* b, int display_progress, int* ev)
{
  init_ttable();
  if (display_progress) {
    printf("**\n  Searching: AB/ID/Asp(adapt)["); show_settings(stdout, &the_settings); printf("]\n");
  }
  compute_time_allotment(&the_settings.allotment, &the_settings.control);
  if (display_progress) {
    printf("  Time control: "); show_time_control(stdout, &the_settings.control); printf("\n");
    printf("  Time allotment: "); show_time_allotment(stdout, &the_settings.allotment); printf("\n");
  }
  int ply, max_ply = the_settings.max_depth/100;
  int window_width = 5;
  int window_low = loss_value - 1;
  int window_high = win_value + 1;

  int   last_good_pv_length = 0;
  move  last_good_pv[MAX_PV_LENGTH];
  int   last_good_eval = loss_value - 1;

  double  ply_time_used[32];
  int     ply_eval[32];
  move    ply_move[32];
  int     ply_is_good[32];

  memset(ply_time_used, '\x00', sizeof(ply_time_used));
  memset(ply_eval, '\x00', sizeof(ply_eval));

  double st = read_clock();
  double tt = 0.0;
  int    restarts_up = 0, restarts_down = 0;
  for (ply=1; (ply<=max_ply); ++ply)
  {
    double bt = read_clock();

    /* decide if we start another iteration... */
    if (ply>1)
    {
      double  time_used = bt - st;
      if (display_progress)
        printf("    time_used=%.4f estimate=%.4f standard_allotment=%.2f (%.2f)\n",
            time_used, (time_used + 4*ply_time_used[ply-2]), the_settings.allotment.standard, the_settings.allotment.panic);
      if (the_stats.stop_flag) break;
      if (time_used + 4*ply_time_used[ply-2] > the_settings.allotment.standard)
      {
        break;
      }
      if (last_good_pv_length > 0)
      {
        /* found mate within search depth, no point to search further */
        if ((last_good_eval + (ply-1) >= win_value) || (last_good_eval - (ply-1) <= loss_value))
          break;
      }
    }

    int depth = 100 * ply;
    uint64 pre_nodes = the_stats.nodes_evaluated;
    *ev = search_alphabeta_tt(b, 0, depth, window_low, window_high, 1, 1);
    double et = read_clock();
    tt += et - bt;
    uint64 post_nodes = the_stats.nodes_evaluated;
    ply_time_used[ply-1] = et - bt;
    ply_eval[ply-1] = *ev;
    ply_move[ply-1] = pv[0][0];
    if (display_progress)
      printf("  (%4i) [%10i,%10i] {{%10i}} [%'9llu] (%8.4fs) ",
          depth, window_low, window_high, *ev, (post_nodes - pre_nodes), (et-bt));
    if (*ev <= window_low)
    {
      --ply;
      window_width *= 2;
      if (++restarts_down >= 2)
      {
        window_low = loss_value - 1;
        window_high = *ev + window_width;
      }
      else
      {
        window_low = *ev - window_width;
        window_high = *ev + window_width;
      }
      if (display_progress) printf("R");
    }
    else if (*ev >= window_high)
    {
      --ply;
      window_width *= 2;
      if (++restarts_down >= 2)
      {
        window_low = *ev - window_width;
        window_high = win_value + 1;
      }
      else
      {
        window_low = *ev - window_width;
        window_high = *ev + window_width;
      }
      if (display_progress) printf("R");
    }
    else
    {
      restarts_up = restarts_down = 0;
      window_low = *ev - window_width;
      window_high = *ev + window_width;
      if (display_progress) printf(" ");
      if (!the_stats.stop_flag) {
        memcpy(last_good_pv, pv[0], sizeof(move)*pv_length[0]);
        last_good_pv_length = pv_length[0];
      }
      ply_is_good[ply-1] = 1;
      last_good_eval = *ev;
    }
    if (display_progress) {
      int j;
      for (j=0; j<pv_length[0]; ++j) { char buff[32]; printf(" %-8s", move_to_string(pv[0][j], buff)); } printf("\n");
    }
  }
  *ev = last_good_eval;
  memcpy(pv[0], last_good_pv, sizeof(move)*last_good_pv_length);
  pv_length[0] = last_good_pv_length;

  if (display_progress)
  {
    char  buff[16];
    int j;
    for (j=0; j<last_good_pv_length; ++j) { char buff[32]; printf(" %-8s", move_to_string(last_good_pv[j], buff)); } printf("\n");
    printf("  {%7i}  [nodes=%'9llu; time=%8.4fs; speed=%9.1fnps]\n", *ev, the_stats.nodes_evaluated, tt, the_stats.nodes_evaluated/tt);
    printf("  "); show_stats(stdout, &the_stats);
    printf("\n**  %i%s%s\n", (b->ply+1)/2, ((b->ply%2)?".":"..."), move_to_string(pv[0][0], buff));
  }
}

static state play_game(search_settings* settings_black, search_settings* settings_white, int seed, game* g, int display_progress,
    int num_forced_moves, move* forced_moves, const char* initial_board, const book_entry* book)
{
  int i;
  board b;
  search_settings the_search_settings[2];
  the_search_settings[black] = *settings_black;
  the_search_settings[white] = *settings_white;

  the_search_settings[black].seed = the_search_settings[white].seed = seed;

  init_hash(the_search_settings[black].seed);
  if (initial_board)
    fen_to_board(&b, initial_board);
  else
    init_board(&b);
  init_game(g, &b);

  g->controls[black] = settings_black->control;
  g->controls[white] = settings_white->control;

  move  game_moves[MAX_GAME_LENGTH];
  int book_flag = 1;
  for (i=0; (b.play==playing)&&(i<MAX_GAME_LENGTH); ++i)
  {
    double bt = 0.0, et = 0.0;
    int  ev = 0;

    if (i<num_forced_moves)
    {
      if (!display_progress) printf("%c", b.side==black ? ':' : '.');
      init_stats(&the_stats);
      pv_length[0] = 1;
      pv[0][0] = forced_moves[i];
    }
    else
    {
      draw_value = (b.side==black) ? -10 : +10;
      the_settings = the_search_settings[b.side];
      set_evaluation_weights(the_settings.evaluation_weights ? the_settings.evaluation_weights : initial_weights);
      init_hash(the_settings.seed);
      init_stats(&the_stats);
      the_stats.count_down = COUNT_DOWN_NODES;
      g->controls[b.side].ply = b.ply;
      the_settings.control = g->controls[b.side];
      init_ttable();

      if (display_progress) show_game(stdout, g, 0);
      if (display_progress) show_moves(stdout, &b);

      int have_book_move = 0;
      move bookm;
      if (the_settings.opening_book)
      {
        if (book_move(book, i, game_moves, &bookm))
        {
          char  buff[32];
          if (display_progress) printf("++ Book move: %s ++\n", move_to_string(bookm, buff));
          have_book_move = 1;
        }
        else
        {
          if (display_progress && book_flag) { book_flag = 0; printf("++ out of book ++\n"); }
        }
      }

      if (!display_progress) printf("%c", b.side==black ? '+' : '-');

      the_stats.start_time = read_clock();
      bt = the_stats.start_time;
      if (the_search_settings[b.side].is_human)
      {
        move  ml[256];
        int   n;
        int m = -1;
        n = generate_moves(&b, ml, 0);
        while (m<0 || m>=n)
        {
          printf("*** Please enter a move number: ");
          scanf("%i", &m);
        }
        pv_length[0] = 1;
        pv[0][0] = ml[m];
      }
      else
      {
        if (have_book_move)
        {
          pv[0][0] = bookm;
          pv_length[0] = 1;
        }
        else
        {
          searcher(&b, display_progress, &ev);
        }
      }
      et = read_clock();
    }
    game_moves[i] = pv[0][0];
    make_move(&b, pv[0][0]);
    if (display_progress) printf("\n");
    add_game_entry(g, pv[0], pv_length[0], ev, (et-bt), &the_stats);
    if (g->controls[b.xside].remaining < 0.0)
    {
      /* loss on time */
      printf("** Loss on time **\n");
      if (b.side==black) b.play = black_win;
      else if (b.side==white) b.play = white_win;
      else b.play = draw;
    }
  }
  return b.play;
}

void save_weights(FILE* f, int num_weights, double weights[])
{
  int i;
  fprintf(f, "%i\n", num_weights);
  for (i=0; i<num_weights; ++i)
    fprintf(f, "%.18f ", weights[i]);
}

void load_weights(FILE* f, int* num_weights, double weights[])
{
  int i;
  fscanf(f, "%i", num_weights);
  for (i=0; i<*num_weights; ++i)
    fscanf(f, "%lf", &weights[i]);
}

} // namespace minishogi

int main(int argc, char* argv[])
{
  game  g;

  setvbuf(stdout, NULL, _IONBF, 0);
  setlocale(LC_ALL,"");

  setup_initial_weights();

  double  play_weights[NUM_WEIGHTS];
  double  play_weights2[NUM_WEIGHTS];
  {
    int num_weights = 0;
    FILE* fw = fopen("weights.txt", "r");
    if (fw)
    {
      load_weights(fw, &num_weights, play_weights);
      fclose(fw);
    }
    if (!fw || (num_weights != NUM_WEIGHTS))
      memcpy(play_weights, initial_weights, NUM_WEIGHTS*sizeof(double));
    set_evaluation_weights(play_weights);
  }
  {
    int num_weights = 0;
    FILE* fw = fopen("weights_b.txt", "r");
    if (fw)
    {
      load_weights(fw, &num_weights, play_weights2);
      fclose(fw);
    }
    if (!fw || (num_weights != NUM_WEIGHTS))
      memcpy(play_weights2, initial_weights, NUM_WEIGHTS*sizeof(double));
  }

  book_entry* the_book = 0;
  {
    FILE* f = fopen("book.txt", "r");
    if (f)
    {
      the_book = load_book(f);
      fclose(f);
    }
  }

  if (0)
  {
    //book_entry* book = make_full_book(1);
    int num_expansions = 1000;
    int num_paths = 13;
    int base_seed = -1333;
    double  base_time = 1.00, increment = 0.05;
    search_settings player;

    init_settings(&player, base_seed, 300);
    player.null_move_reduction = 100;
    player.adaptive_null_move_level = 200;
    player.evaluation_weights = play_weights;
    player.control.ply = 0;
    player.control.base = base_time;
    player.control.remaining = base_time;
    player.control.increment = increment;

    book_entry* book = 0;
    {
      FILE* f = fopen("book.txt", "r");
      if (f)
      {
        book = load_book(f);
        fclose(f);
      }
    }
    printf("\n***** ***** ***** Initial book: ***** ***** *****\n");
    if (book)
      show_book(stdout, book);
    else
      printf("---empty---\n");
    printf("***** ***** ***** : ***** ***** *****\n");

    book = make_uct_book(book, num_expansions, num_paths, base_seed, &player);

    printf("\n***** ***** ***** Final book: ***** ***** *****\n");
    show_book(stdout, book);
    printf("***** ***** ***** : ***** ***** *****\n");
    {
      FILE* f = fopen("book.txt", "w");
      if (f)
      {
        save_book(f, book);
        fclose(f);
      }
    }
    return 0;
  }


  int human_player = -1;
  //double increment = 15.00, base = 45*60.0;
  double increment = 0.10, base = 2.0;
  //double increment = 12.0, base = 2*60.0;
  int max_round = 50;
  int display_progress = 0;
  int update_weights = 0;
  top_level = 0;

#if 0
  int num_players = 12;

  int results[num_players][num_players][4];
  int net_results[num_players][3][4];
  uint64  total_nodes[num_players];
  double  total_time[num_players];
  int     total_games[num_players];

  memset(results, '\x00', sizeof(results));
  memset(net_results, '\x00', sizeof(net_results));
  memset(total_nodes, '\x00', sizeof(total_nodes));
  memset(total_time, '\x00', sizeof(total_time));
  memset(total_games, '\x00', sizeof(total_games));

  search_settings the_search_settings[num_players];
  int   i, j;
  for (i=0; i<4; ++i)
  {
    for (j=0; j<3; ++j)
    {
      int p = i*3+j;
      init_settings(&the_search_settings[p], 17, 400+i*100);
      switch (j)
      {
        case 0:
          the_search_settings[p].null_move_reduction = i>1 ? 400 : 300;
          the_search_settings[p].adaptive_null_move_level = 0;
          break;
        case 1:
          the_search_settings[p].null_move_reduction = i>1 ? 300 : 200;
          the_search_settings[p].adaptive_null_move_level = 0;
          break;
        case 2:
          the_search_settings[p].null_move_reduction = i>1 ? 200 : 100;
          the_search_settings[p].adaptive_null_move_level = 0;
          break;
      }
      if (i==0 && j==0) the_search_settings[p].null_move_reduction = 500;

      the_search_settings[p].control.ply = 0;
      the_search_settings[p].control.base = base;
      the_search_settings[p].control.remaining = base;
      the_search_settings[p].control.increment = increment;
    }
  }
#elif 0
  int num_players = 5;

  int results[num_players][num_players][4];
  int net_results[num_players][3][4];
  uint64  total_nodes[num_players];
  double  total_time[num_players];
  int     total_games[num_players];

  memset(results, '\x00', sizeof(results));
  memset(net_results, '\x00', sizeof(net_results));
  memset(total_nodes, '\x00', sizeof(total_nodes));
  memset(total_time, '\x00', sizeof(total_time));
  memset(total_games, '\x00', sizeof(total_games));

  search_settings the_search_settings[num_players];
  int   i;
  for (i=0; i<num_players; ++i)
  {
    init_settings(&the_search_settings[i], 17, 2000);
    if (i==3) {
      the_search_settings[i].null_move_reduction = 100;
      the_search_settings[i].adaptive_null_move_level = 300;
    } else if (i==4) {
      the_search_settings[i].null_move_reduction = 100;
      the_search_settings[i].adaptive_null_move_level = 200;
    } else {
      the_search_settings[i].null_move_reduction = 100 + i*100;
    }
    the_search_settings[i].control.ply = 0;
    the_search_settings[i].control.base = base;
    the_search_settings[i].control.remaining = base;
    the_search_settings[i].control.increment = increment;
    the_search_settings[i].evaluation_weights = play_weights;
  }
#elif 1
  int num_players = 2;

  int results[num_players][num_players][4];
  int net_results[num_players][3][4];
  uint64  total_nodes[num_players];
  double  total_time[num_players];
  int     total_games[num_players];

  memset(results, '\x00', sizeof(results));
  memset(net_results, '\x00', sizeof(net_results));
  memset(total_nodes, '\x00', sizeof(total_nodes));
  memset(total_time, '\x00', sizeof(total_time));
  memset(total_games, '\x00', sizeof(total_games));

  search_settings the_search_settings[num_players];
  int   i;
  for (i=0; i<num_players; ++i)
  {
    init_settings(&the_search_settings[i], 17, 2000);
    the_search_settings[i].null_move_reduction = 100;
    the_search_settings[i].adaptive_null_move_level = 200;
    the_search_settings[i].control.ply = 0;
    the_search_settings[i].control.base = base;
    the_search_settings[i].control.remaining = base;
    the_search_settings[i].control.increment = increment;
    the_search_settings[i].evaluation_weights = play_weights;
  }
  the_search_settings[0].opening_book = the_book;
#elif 0
  int num_players = 3;

  int results[num_players][num_players][4];
  int net_results[num_players][3][4];
  uint64  total_nodes[num_players];
  double  total_time[num_players];
  int     total_games[num_players];
  //const double* weights[] = { play_weights, 0 };
  const double* weights[] = { play_weights, play_weights2, 0 };
  printf("play_weights=0x%08X, play_weights2=0x%08X\n", (uint32)play_weights, (uint32)play_weights2);

  memset(results, '\x00', sizeof(results));
  memset(net_results, '\x00', sizeof(net_results));
  memset(total_nodes, '\x00', sizeof(total_nodes));
  memset(total_time, '\x00', sizeof(total_time));
  memset(total_games, '\x00', sizeof(total_games));

  search_settings the_search_settings[num_players];
  int   i;
  for (i=0; i<num_players; ++i)
  {
    init_settings(&the_search_settings[i], 177, 2000);
    the_search_settings[i].control.ply = 0;
    the_search_settings[i].control.base = base;
    the_search_settings[i].control.remaining = base;
    the_search_settings[i].control.increment = increment;
    the_search_settings[i].evaluation_weights = weights[i];
  }
#endif

  if (human_player >= 0) {
    the_search_settings[human_player].is_human = 1;
    display_progress = 1;
  }

  int round;
  for (round=0; round<max_round; ++round)
  {
    int black_player, white_player;
    for (black_player=0; black_player<num_players; ++black_player)
    {
      for (white_player=0; white_player<num_players; ++white_player)
      {
        if (white_player == black_player) continue;
        state result = play_game(&the_search_settings[black_player], &the_search_settings[white_player], 133+17*round, &g, display_progress,
            0, 0, 0, the_book);
        show_game(stdout, &g, 0);
        total_nodes[black_player] += g.total_nodes[black];
        total_nodes[white_player] += g.total_nodes[white];
        total_time[black_player] += g.total_time[black];
        total_time[white_player] += g.total_time[white];
        ++total_games[black_player];
        ++total_games[white_player];
        switch (result)
        {
          case black_win:
            ++results[black_player][white_player][0];
            //++results[white_player][black_player][1];
            ++net_results[black_player][black][0];
            ++net_results[white_player][white][1];
            ++net_results[black_player][none][0];
            ++net_results[white_player][none][1];
            break;
          case white_win:
            ++results[black_player][white_player][1];
            //++results[white_player][black_player][0];
            ++net_results[black_player][black][1];
            ++net_results[white_player][white][0];
            ++net_results[black_player][none][1];
            ++net_results[white_player][none][0];
            break;
          case draw:
            ++results[black_player][white_player][2];
            //++results[white_player][black_player][2];
            ++net_results[black_player][black][2];
            ++net_results[white_player][white][2];
            ++net_results[black_player][none][2];
            ++net_results[white_player][none][2];
            break;
          case playing:
            ++results[black_player][white_player][3];
            //++results[white_player][black_player][3];
            ++net_results[black_player][black][3];
            ++net_results[white_player][white][3];
            ++net_results[black_player][none][3];
            ++net_results[white_player][none][3];
            break;
        }

        printf("Black: ");
        show_settings(stdout, &the_search_settings[black_player]);
        printf("\tTime: %.1fs  Nodes:%'llu\n", total_time[black_player], total_nodes[black_player]);
        printf("White: ");
        show_settings(stdout, &the_search_settings[white_player]);
        printf("\tTime: %.1fs  Nodes:%'llu\n", total_time[white_player], total_nodes[white_player]);

        printf("\n------------------------------------------------------------\n");
        int xi, xj;
        for (xi=0; xi<num_players; ++xi)
        {
          printf("(%2i) : ", xi);
          show_settings(stdout, &the_search_settings[xi]);
          printf("\tTime:%6.1fs (Ave:%6.2fs/g)   Nodes:%'12llu (Ave:%'10llu/g)  %.0fnps\n",
              total_time[xi], (total_games[xi] ? total_time[xi]/total_games[xi] : 0.0),
              total_nodes[xi], (total_games[xi] ? total_nodes[xi]/total_games[xi] : 0llu),
              (double)total_nodes[xi]/total_time[xi] );
        }
        printf("       "); for (xi=0; xi<num_players; ++xi) printf("    (%2i)   ", xi); printf("\n");
        for (xi=0; xi<num_players; ++xi)
        {
          printf("(%2i) : ", xi);
          for (xj=0; xj<num_players; ++xj)
          {
            if (xi==xj) printf("           ");
            else printf("  +%-2i-%-2i=%-2i", results[xi][xj][0], results[xi][xj][1], results[xi][xj][2]);
          }
          printf("  B:+%2i-%2i=%2i(%.1f/%i)  W:+%2i-%2i=%2i(%.1f/%i)   T:%2i-%2i=%2i(%.1f/%i)\n",
              net_results[xi][black][0], net_results[xi][black][1], net_results[xi][black][2],
                (net_results[xi][black][0]+0.5*net_results[xi][black][2]),
                (net_results[xi][black][0]+net_results[xi][black][1]+net_results[xi][black][2]),
              net_results[xi][white][0], net_results[xi][white][1], net_results[xi][white][2],
                (net_results[xi][white][0]+0.5*net_results[xi][white][2]),
                (net_results[xi][white][0]+net_results[xi][white][1]+net_results[xi][white][2]),
              net_results[xi][none][0], net_results[xi][none][1], net_results[xi][none][2],
                (net_results[xi][none][0]+0.5*net_results[xi][none][2]),
                (net_results[xi][none][0]+net_results[xi][none][1]+net_results[xi][none][2]));
        }
        printf("------------------------------------------------------------\n");

        if (update_weights)
        {
          double  new_weights[NUM_WEIGHTS];
          double  alpha = 50.0;
          double  beta = 0.50/100;
          double  lambda = 0.90;
          td_lambda_process_game(&g, lambda, alpha, beta, NUM_WEIGHTS, play_weights, new_weights);
          int pc, i;
          for (pc=0; pc<10; ++pc) {
            double ave = 0.0;
            for (i=0; i<25; ++i) ave += new_weights[i*10 + pc];
            ave /= 25;
            printf("Average(%c): %.2f [%.2f]\n", piece_name[pc], ave, new_weights[25*10 + unpromotes[pc]]);
          }
          printf("Pawn map:\n");
          {
            int r, c, rc;
            for (r=4; r>=0; --r) { for (c=0; c<5; ++c) {
                rc = r*5+c;
                printf(" %4.1f", new_weights[rc*10 + pawn]);
              }
              printf("\n");
            }
          }
          printf("Rook map:\n");
          {
            int r, c, rc;
            for (r=4; r>=0; --r) { for (c=0; c<5; ++c) {
                rc = r*5+c;
                printf(" %4.1f", new_weights[rc*10 + rook]);
              }
              printf("\n");
            }
          }
          printf("Gold map:\n");
          {
            int r, c, rc;
            for (r=4; r>=0; --r) { for (c=0; c<5; ++c) {
                rc = r*5+c;
                printf(" %4.1f", new_weights[rc*10 + gold]);
              }
              printf("\n");
            }
          }
          memcpy(play_weights, new_weights, NUM_WEIGHTS*sizeof(double));
        }
      }
    }
    if (update_weights && (!((round+1)%10))) {
      FILE* fw = fopen("temp_weights.txt", "w");
      if (fw)
      {
        save_weights(fw, NUM_WEIGHTS, play_weights);
        fclose(fw);
      }
    }
  }

  if (update_weights) {
    FILE* fw = fopen("weights.txt", "w");
    if (fw)
    {
      save_weights(fw, NUM_WEIGHTS, play_weights);
      fclose(fw);
    }
  }

  return 0;
}

/* $Id: simple-shogi.c,v 1.3 2009/11/28 05:56:49 apollo Exp apollo $ */

