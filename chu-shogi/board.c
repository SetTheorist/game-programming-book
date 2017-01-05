/* $Id: board.c,v 1.1 2010/12/21 16:30:32 ahogan Exp ahogan $ */
#include <ctype.h>
#include <math.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "evaluate.h"
#include "search.h"
#include "util.h"
#include "util-bits.h"

//#define VALIDATE_BOARD
#define USE_COLORS

/* **************************************** */

static hasht piece_square_hash[144][num_color][num_piece][2];
static hasht square_no_capture_hash[144];
static hasht btm_hash;
static hasht no_lion_recapture_hash;

const char* piece_names[num_piece] = {
  "?", "P", "GB", "RC", "Ln",
  "K", "FK", "L", "DK", "DH", "R", "B", "Ky",
  "Ph", "DE", "BT", "FL",
  "G", "S", "C", "VM", "SM",
};
const char* piece_kanji[2][num_piece][2] = {
  {
    {"??","??"}, {"歩","兵"}, {"仲","人"}, {"反","車"}, {"香","車"},
    {"玉","将"}, {"奔","王"}, {"獅","子"}, {"龍","王"}, {"龍","馬"}, {"飛","車"}, {"角","行"}, {"麒","麟"},
    {"鳳","凰"}, {"酔","象"}, {"盲","虎"}, {"猛","豹"},
    {"金","将"}, {"銀","将"}, {"銅","将"}, {"竪","行"}, {"横","行"},
  },
  {
    {"+?","+?"}, {"と","金"}, {"酔","象"}, {"鯨","鯢"}, {"白","駒"},
    {"xx","xx"}, {"xx","xx"}, {"xx","xx"}, {"飛","鷲"}, {"角","鷹"}, {"龍","王"}, {"龍","馬"}, {"獅","子"},
    {"奔","王"}, {"太","子"}, {"飛","鹿"}, {"角","行"},
    {"飛","車"}, {"竪","行"}, {"横","行"}, {"飛","牛"}, {"奔","猪"},
  }
};
const char* other_king_kanji[2] = {"王","将"};
const char* piece_kanji1[2][num_piece] = {
  {
    "??", "歩", "仲", "反", "香",
    "玉", "奔", "獅", "龍", "馬", "飛", "角", "麒",
    "鳳", "象", "虎", "豹",
    "金", "銀", "銅", "竪", "横",
  },
  {
    "!!", "と", "象", "鯨", "駒",
    "xx", "xx", "xx", "鷲", "鷹", "竜", "馬", "獅",
    "奔", "太", "鹿", "角",
    "飛", "竪", "横", "牛", "猪",
  }
};

/* **************************************** */

/* returns true if square loc is attacked by any piece of color c
 * - ignores subtleties of lions
 */
static inline int is_square_attacked(const board* b, int loc, color c);

/* **************************************** */

/* returns 1 if target square on-board and empty */
static inline int add_cap_no_boardcheck(register const board* b, movet* ml, int* n, color tm, piece p, int pj, int f, int t)
{
  if (b->b[t].c == tm) return 0;
  if (b->b[t].c == none) return 1;

  {
    movet m;
    m.i = 0;
    m.from = f;
    m.to = t;
    m.side_is_white = (tm == white);
    m.piece = p;
    m.promoted_piece = b->b[f].promoted;
    m.idx = pj;
    m.capture = 1;
    m.valid = 1;
    ml[*n] = m;
    ++*n;
  }

  if (!b->b[f].promoted)
  {
    if ((tm==white) ? (max(f,t)>=8*12) : (min(f,t)<4*12))
    {
      if (p==pawn)
      {
        /* pawn either promotes on first or last move in(to) promotion zone only */
        if ((tm==white) ? ((t>=11*12)||(t<9*12)) : ((t<1*12)||(t>=3*12)))
        {
          if ((t >= 11*12) || (t < 1*12))
          {
            /* pawn must promote on last rank */
            ml[*n-1].can_promote = 1;
            ml[*n-1].promote = 1;
          }
          else
          {
            ml[*n-1].can_promote = 1;
            ml[*n] = ml[*n-1];
            ml[*n-1].promote = 1;
            ++*n;
          }
        }
      }
      else if ((1<<p) & PROMOTION_MASK)
      {
        if (p==lance && ((t >= 11*12) || (t < 1*12)))
        {
          /* lance must promote on last rank */
          ml[*n-1].can_promote = 1;
          ml[*n-1].promote = 1;
        }
        else
        {
          ml[*n-1].can_promote = 1;
          ml[*n] = ml[*n-1];
          ml[*n-1].promote = 1;
          ++*n;
        }
      }
    }
  }
  return 0;
}

/* returns 1 if target square on-board and empty */
static inline int add_cap(register const board* b, movet* ml, int* n, color tm, piece p, int pj, int f, int dx, int dy)
{
  const int tx = (f % 12) + dx;
  const int ty = (f / 12) + dy;
  if (tx<0 || tx>=12 || ty<0 || ty>=12) return 0;
  const int t = tx + 12*ty;
  return add_cap_no_boardcheck(b, ml, n, tm, p, pj, f, t);
}

/* **************************************** */

/* returns 1 if target square on-board and empty */
static inline int add_move_or_cap_no_boardcheck(register const board* b, movet* ml, int* n, color tm, piece p, int pj, int f, int t)
{
  if (b->b[t].c == tm) return 0;
  const int is_capture = (b->b[t].c != none);

  {
    movet m;
    m.i = 0;
    m.from = f;
    m.to = t;
    m.side_is_white = (tm == white);
    m.piece = p;
    m.promoted_piece = b->b[f].promoted;
    m.idx = pj;
    m.capture = is_capture;
    m.valid = 1;
    ml[*n] = m;
    ++*n;
  }
  if (((1<<p)&PROMOTION_MASK)
      && !b->b[f].promoted
      && (is_capture || !b->pl[tm][p][pj].cannot_promote_unless_capture))
  {
    if ((tm==white) ? ((t>=8*12)||(f>=8*12)) : ((t<4*12)||(f<4*12)))
    {
      if (p==pawn)
      {
        /* pawn either promotes on first or last move in(to) promotion zone only */
        if ((tm==white) ? ((t>=11*12)||(t<9*12)) : ((t<1*12)||(t>=3*12)))
        {
          if ((t >= 11*12) || (t < 1*12))
          {
            /* pawn must promote on last rank */
            ml[*n-1].can_promote = 1;
            ml[*n-1].promote = 1;
          }
          else
          {
            ml[*n-1].can_promote = 1;
            ml[*n] = ml[*n-1];
            ml[*n-1].promote = 1;
            ++*n;
          }
        }
      }
      else if ((1<<p) & PROMOTION_MASK)
      {
        if (p==lance && ((t >= 11*12) || (t < 1*12)))
        {
          /* lance must promote on last rank */
          ml[*n-1].can_promote = 1;
          ml[*n-1].promote = 1;
        }
        else
        {
          ml[*n-1].can_promote = 1;
          ml[*n] = ml[*n-1];
          ml[*n-1].promote = 1;
          ++*n;
        }
      }
    }
  }
  return !is_capture;
}

/* returns 1 if target square on-board and empty */
static inline int add_move_or_cap(register const board* b, movet* ml, int* n, color tm, piece p, int pj, int f, int dx, int dy)
{
  const int tx = (f % 12) + dx;
  const int ty = (f / 12) + dy;
  if (tx<0 || tx>=12 || ty<0 || ty>=12) return 0;
  const int t = tx + 12*ty;
  return add_move_or_cap_no_boardcheck(b, ml, n, tm, p, pj, f, t);
}

/* **************************************** */

static inline int add_lion_move_or_cap(register const board* b, movet* ml, int* n, color tm, piece p, int pj, int f, int dir1, int dir2,
    int caps_only, int first_step_caps_only)
{
  if (dir1 && !((square_neighbor_map[f]<<1) & (1<<dir1))) return 0;
  const int t1 = f + dir_map[dir1];
  const int cap1 = (b->b[t1].c == (tm^3));
  if ((t1!=f) && (b->b[t1].c == tm)) return 0;
  if (first_step_caps_only && !cap1) return 1;

  if (dir2 && !((square_neighbor_map[t1]<<1) & (1<<dir2))) return 0;
  const int t2 = t1 + dir_map[dir2];
  const int cap2 = (b->b[t2].c == (tm^3));
  if ((t2!=f) && (b->b[t2].c == tm)) return 0;
  if (caps_only && !cap1 && !cap2) return 1;

  {
    movet m;
    m.i = 0;
    m.special.from = f;
    m.special.to1 = dir1;
    m.special.to2 = dir2;
    m.special.side_is_white = (tm == white);
    m.special.piece = p;
    m.special.promoted_piece = b->b[f].promoted;
    m.special.idx = pj;
    m.special.capture1 = cap1;
    m.special.capture2 = cap2;
    m.special.movetype = 1;
    m.special.is_special = 1;
    m.special.valid = 1;
    ml[*n] = m;
    ++*n;
  }

  return 1;
}

/* **************************************** */

/* special case to _only_ generate lion / promoted-kirin captures */
static int move_generator_caps_lion(register const board* b, movet* ml)
{
  const color tm = b->to_move;
  int j, n=0;
  for (j=0; b->pl[tm][lion][j].valid; ++j)
  {
    if (!b->pl[tm][lion][j].active) continue;
    const int loc = b->pl[tm][lion][j].loc;

    /* two-steps with capture on first step (includes igui) */
    int dir1, dir2;
    for (dir1=1; dir1<=8; ++dir1)
      if ((square_neighbor_map[loc]<<1) & (1<<dir1))
        for (dir2=1; dir2<=8; ++dir2)
          add_lion_move_or_cap(b, ml, &n, tm, lion, j, loc, dir1, dir2, 1, 1);

    /* Direct jump / capture */
    int dx, dy;
    for (dx=-2; dx<=2; ++dx)
      for (dy=-2; dy<=2; ++dy)
        if (dx || dy)
          add_cap(b, ml, &n, tm, lion, j, loc, dx, dy);
  }

  for (j=0; b->pl[tm][kirin][j].valid; ++j)
  {
    if (!b->pl[tm][kirin][j].active || !b->pl[tm][kirin][j].promoted) continue;
    const int loc = b->pl[tm][kirin][j].loc;

    /* two-steps with capture on first step (includes igui) */
    int dir1, dir2;
    for (dir1=1; dir1<=8; ++dir1)
      if ((square_neighbor_map[loc]<<1) & (1<<dir1))
        for (dir2=1; dir2<=8; ++dir2)
          add_lion_move_or_cap(b, ml, &n, tm, kirin, j, loc, dir1, dir2, 1, 1);

    /* Direct jump / capture */
    int dx, dy;
    for (dx=-2; dx<=2; ++dx)
      for (dy=-2; dy<=2; ++dy)
        if (dx || dy)
          add_cap(b, ml, &n, tm, kirin, j, loc, dx, dy);
  }

  return n;
}

/* **************************************** */

static int move_generator_caps(const board* b, movet* ml)
{
  int ip, j, n=0;
  const color tm=b->to_move;
  for (ip=1; ip<num_piece; ++ip)
  {
    for (j=0; b->pl[tm][ip][j].valid; ++j)
    {
      register const piece_info current_piece = b->pl[tm][ip][j];
      if (!current_piece.active) continue;
      const int pro = current_piece.promoted;
      const int loc = current_piece.loc;

      if (ip==lion || (ip==kirin && pro))
      {
        /* two-steps with capture on first step (includes igui) */
        const int dirs = square_neighbor_map[loc] << 1;
        int dir1, dir2;
        for (dir1=1; dir1<=8; ++dir1)
          if (dirs & (1<<dir1))
            for (dir2=1; dir2<=8; ++dir2)
              add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, dir1, dir2, 0, 1);
        /* Direct jump / capture */
        int dx, dy;
        for (dx=-2; dx<=2; ++dx)
          for (dy=-2; dy<=2; ++dy)
            if (dx || dy)
              add_cap(b, ml, &n, tm, ip, j, loc, dx, dy);
      }
      else
      {
        if (ip==dragon_king && pro)
        {
          /* = soaring_eagle */
          add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, tm==white?1:5, tm==white?5:1, 1, 1);
          add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, tm==white?1:5, tm==white?1:5, 1, 1);
          add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, tm==white?3:7, tm==white?7:3, 1, 1);
          add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, tm==white?3:7, tm==white?3:7, 1, 1);
        }
        else if (ip==dragon_horse && pro)
        {
          /* = horned_falcon */
          add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, tm==white?2:6, tm==white?6:2, 1, 1);
          add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, tm==white?2:6, tm==white?2:6, 1, 1);
        }

        const int move_type = piece_move_type[tm][ip][pro] & square_neighbor_map[loc];
        const int move_type_1 = (move_type & 0x0000FF);
        const int move_type_2 = (move_type & 0x00FF00)>>8;
        const int move_type_n = (move_type & 0xFF0000)>>16;

        /* one-step moves */
        if (move_type_1)
        {
          int d;
          for (d=0; d<8; ++d)
          {
            if (move_type_1 & (1<<d))
            {
              const int to = loc + dir_map[d+1];
              add_cap_no_boardcheck(b, ml, &n, tm, ip, j, loc, to);
            }
          }
        }

        /* two-step moves */
        if (move_type_2)
        {
          int d;
          for (d=0; d<8; ++d)
          {
            if (move_type_2 & (1<<d))
            {
              const int to = loc + 2*dir_map[d+1];
              add_cap_no_boardcheck(b, ml, &n, tm, ip, j, loc, to);
            }
          }
        }

        /* slides */
        if (move_type_n)
        {
          int d;
          for (d=0; d<8; ++d)
          {
            if (move_type_n & (1<<d))
            {
              const int dx = dir_map_dx[d+1];
              const int dy = dir_map_dy[d+1];
              int i;
              for (i=1; add_cap(b, ml, &n, tm, ip, j, loc, i*dx,  i*dy); ++i);
            }
          }
        }
      }
    }
  }
  return n;
}

/* **************************************** */

static int move_generator(const board* b, movet* ml)
{
  int ip, j, n=0;
  const color tm=b->to_move;
  for (ip=1; ip<num_piece; ++ip)
  {
    for (j=0; b->pl[tm][ip][j].valid; ++j)
    {
      register const piece_info current_piece = b->pl[tm][ip][j];
      if (!current_piece.active) continue;
      const int pro = current_piece.promoted;
      const int loc = current_piece.loc;

      if (ip==lion || (ip==kirin && pro))
      {
        /* two-steps with capture on first step (includes igui) */
        const int dirs = square_neighbor_map[loc] << 1;
        int dir1, dir2;
        for (dir1=1; dir1<=8; ++dir1)
          if (dirs & (1<<dir1))
            for (dir2=1; dir2<=8; ++dir2)
              add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, dir1, dir2, 0, 1);
        /* Direct jump / capture */
        int dx, dy;
        for (dx=-2; dx<=2; ++dx)
          for (dy=-2; dy<=2; ++dy)
            if (dx || dy)
              add_move_or_cap(b, ml, &n, tm, ip, j, loc, dx, dy);
        /* pass-move */
        add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, 0, 0, 0, 0);
      }
      else
      {
        if (ip==dragon_king && pro)
        {
          /* = soaring_eagle */
          add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, tm==white?1:5, tm==white?5:1, 0, 1);
          add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, tm==white?1:5, tm==white?1:5, 0, 1);
          add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, tm==white?3:7, tm==white?7:3, 0, 1);
          add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, tm==white?3:7, tm==white?3:7, 0, 1);
        }
        else if (ip==dragon_horse && pro)
        {
          /* = horned_falcon */
          add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, tm==white?2:6, tm==white?6:2, 0, 1);
          add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, tm==white?2:6, tm==white?2:6, 0, 1);
        }

        const int move_type = piece_move_type[tm][ip][pro] & square_neighbor_map[loc];
        const int move_type_1 = (move_type & 0x0000FF);
        const int move_type_2 = (move_type & 0x00FF00)>>8;
        const int move_type_n = (move_type & 0xFF0000)>>16;

        /* one-step moves */
        if (move_type_1)
        {
          int d;
          for (d=0; d<8; ++d)
          {
            if (move_type_1 & (1<<d))
            {
              const int to = loc + dir_map[d+1];
              add_move_or_cap_no_boardcheck(b, ml, &n, tm, ip, j, loc, to);
            }
          }
        }

        /* two-step moves */
        if (move_type_2)
        {
          int d;
          for (d=0; d<8; ++d)
          {
            if (move_type_2 & (1<<d))
            {
              const int to = loc + 2*dir_map[d+1];
              add_move_or_cap_no_boardcheck(b, ml, &n, tm, ip, j, loc, to);
            }
          }
        }

        /* slides */
        if (move_type_n)
        {
          int d;
          for (d=0; d<8; ++d)
          {
            if (move_type_n & (1<<d))
            {
              const int dx = dir_map_dx[d+1];
              const int dy = dir_map_dy[d+1];
              int i;
              for (i=1; add_move_or_cap(b, ml, &n, tm, ip, j, loc, i*dx,  i*dy); ++i);
            }
          }
        }

        if (pro && (ip==dragon_king || ip==dragon_horse))
          add_lion_move_or_cap(b, ml, &n, tm, ip, j, loc, 0, 0, 0, 0);
      }
    }
  }
  return n;
}

/* **************************************** */

int gen_moves(const board* b, movet* ml)
{
  return move_generator(b, ml);
}

int gen_moves_cap(const board* b, movet* ml)
{
  return move_generator_caps(b, ml);
}

movet null_move() {
  movet m;
  m.i = 0;
  m.valid = 1;
  m.is_special = 1;
  m.special.movetype = 0;
  return m;
}

/* **************************************** */

int init_hash()
{
  int i, j, k, l;
  btm_hash = gen_hash();
  no_lion_recapture_hash = gen_hash();
  for (i=0; i<144; ++i)
    for (j=0; j<num_color; ++j)
      for (k=0; k<num_piece; ++k)
        for (l=0; l<2; ++l)
          piece_square_hash[i][j][k][l] = gen_hash();
  for (i=0; i<144; ++i)
    square_no_capture_hash[i] = gen_hash();
  return 0;
}

static hasht rehash_board(const board* b)
{
  hasht h = b->to_move==black ? btm_hash : 0ULL;

  if (b->flags.no_lion_recapture)
    h ^= no_lion_recapture_hash;

  int i;
  for (i=0; i<12*12; ++i)
  {
    if (b->b[i].c!=none)
    {
      const int c = b->b[i].c;
      const int p = b->b[i].piece;
      const int pro = b->b[i].promoted;
      const int x = b->b[i].idx;
      const int cap = b->pl[c][p][x].cannot_promote_unless_capture;
      h ^= piece_square_hash[i][c][p][pro];
      if (cap) h ^= square_no_capture_hash[i];
    }
  }
  return h;
}

/* **************************************** */

#ifdef  INCREMENTAL_MATERIAL
static evalt recompute_material(const board* b)
{
  evalt material_w = 0, material_b = 0;
  int ip, j;

  const int opening_phase = max(0, 100 - b->ply);
  const int midgame_phase = min(50, b->ply);
  const int endgame_phase = max(0, b->ply - 150);
  const int total_phase = opening_phase + midgame_phase + endgame_phase;

  for (ip=1; ip<num_piece; ++ip)
  {
    for (j
    =0; b->pl[white][ip][j].valid; ++j)
    {
      register const piece_info current_piece = b->pl[white][ip][j];
      if (!current_piece.active) continue;

      const int pro = current_piece.promoted;
      const int loc = current_piece.loc;

      /* material */
      material_w += e->piece_vals[ip][pro]
         + ( opening_phase * e->piece_square_bonus[opening][ip][pro][loc]
           + midgame_phase * e->piece_square_bonus[midgame][ip][pro][loc]
           + endgame_phase * e->piece_square_bonus[endgame][ip][pro][loc] ) / total_phase;
    }
  }
  for (ip=1; ip<num_piece; ++ip)
  {
    for (j=0; b->pl[black][ip][j].valid; ++j)
    {
      register const piece_info current_piece = b->pl[black][ip][j];
      if (!current_piece.active) continue;

      const int pro = current_piece.promoted;
      const int loc = current_piece.loc;

      /* material */
      material_b += e->piece_vals[ip][pro]
         + ( opening_phase * e->piece_square_bonus[opening][ip][pro][143 - loc]
           + midgame_phase * e->piece_square_bonus[midgame][ip][pro][143 - loc]
           + endgame_phase * e->piece_square_bonus[endgame][ip][pro][143 - loc] ) / total_phase;
    }
  }

  return (material_w - material_b);
}
#endif

/* **************************************** */

#define P   pawn
#define GB  go_between
#define RC  reverse_chariot
#define Ln  lance
#define K   king
#define FK  free_king
#define Li  lion
#define DK  dragon_king
#define DH  dragon_horse
#define R   rook
#define B   bishop
#define Kr  kirin
#define Ph  phoenix
#define DE  drunk_elephant
#define BT  blind_tiger
#define FL  ferocious_leopard
#define GG  gold_general
#define SG  silver_general
#define CG  copper_general
#define VM  vertical_mover
#define SM  side_mover
#define __  empty
static const int initial_board[5*12] = {
Ln, FL, CG, SG, GG,  K, DE, GG, SG, CG, FL, Ln,
RC, __,  B, __, BT, Kr, Ph, BT, __,  B, __, RC,
SM, VM,  R, DH, DK, Li, FK, DK, DH,  R, VM, SM,
 P,  P,  P,  P,  P,  P,  P,  P,  P,  P,  P,  P,
__, __, __, GB, __, __, __, __, GB, __, __, __
};
int init_board(board* b)
{
  int i;
  memset(b, '\x00', sizeof(board));
  for (i=0; i<5*12; ++i)
  {
    if (initial_board[i])
    {
      b->b[i].c = white;
      b->b[i].piece = initial_board[i];
      for (b->b[i].idx=0; b->pl[white][initial_board[i]][b->b[i].idx].valid; ++b->b[i].idx);
      b->pl[white][initial_board[i]][b->b[i].idx].loc = i;
      b->pl[white][initial_board[i]][b->b[i].idx].valid = 1;
      b->pl[white][initial_board[i]][b->b[i].idx].active = 1;
      if (initial_board[i] == king) b->king_mask[white] |= (1 << b->b[i].idx);
      b->b[i].raw_idx = &b->pl[white][initial_board[i]][b->b[i].idx] - (piece_info*)b->pl;

      const int bi = 12*(11-(i/12)) + (11-(i%12));
      b->b[bi].c = black;
      b->b[bi].piece = initial_board[i];
      for (b->b[bi].idx=0; b->pl[black][initial_board[i]][b->b[bi].idx].valid; ++b->b[bi].idx);
      b->pl[black][initial_board[i]][b->b[bi].idx].loc = bi;
      b->pl[black][initial_board[i]][b->b[bi].idx].valid = 1;
      b->pl[black][initial_board[i]][b->b[bi].idx].active = 1;
      if (initial_board[i] == king) b->king_mask[black] |= (1 << b->b[i].idx);
      b->b[bi].raw_idx = &b->pl[black][initial_board[i]][b->b[bi].idx] - (piece_info*)b->pl;
    }
  }
  b->to_move = white;
  b->h = rehash_board(b);
  b->hash_stack[0] = b->h;
  b->flags_stack[0] = b->flags;
  b->ply = 0;
  return 0;
}
#undef P
#undef GB
#undef RC
#undef Ln
#undef K
#undef FK
#undef Li
#undef DK
#undef DH
#undef R
#undef B
#undef Kr
#undef Ph
#undef DE
#undef BT
#undef FL
#undef GG
#undef SG
#undef CG
#undef VM
#undef SM
#undef __

/* **************************************** */

int draw_by_repetition(const board* b)
{
  int i;

#if 0
  /* HACK TEST */
  // NOTE: even with 64k entries there are lots of collisions
  {
    int table[64*1024];
    memset(table, '\x00', sizeof(table));
    for (i=0; i<=b->ply; ++i)
      ++table[b->hash_stack[i] % (64*1024)];
    for (i=0; i<64*1024; ++i)
      if (table[i] > 1)
        printf("DRAW HASH COLLISION %i\n", table[i]);
  }
#endif

  const hasht h = b->hash_stack[b->ply];
  for (i=b->ply-1; i>=0; --i)
    if (h == b->hash_stack[i])
      return 1;
  return 0;
}

int terminal(const board* b)
{
  /* TODO: check terminality in make_move! */
  if (draw_by_repetition(b)
      || (!b->king_mask[black] && !b->prince_mask[black])
      || (!b->king_mask[white] && !b->prince_mask[white]))
    return 1;
  return 0;
}

/* **************************************** */

/* returns true if square loc is attacked by any piece of color c
 * - ignores subtleties of lions
 */
static inline int is_square_attacked(const board* b, int loc, color c)
{
  const color oc = c^3;

  const int x = loc % 12;
  const int y = loc / 12;

  const int dirs = square_neighbor_map[loc];
  const int dirs1 = (dirs & 0x0000FF);
  const int dirs2 = (dirs & 0x00FF00)>>8;
  const int dirsn = (dirs & 0xFF0000)>>16;

  /* one-step */
  if (dirs1)
  {
    int d;
    for (d=0; d<8; ++d)
    {
      if (dirs1 & (1<<d))
      {
        const int step = dir_map[d+1];
        const int loc1 = loc + step;
        if (b->b[loc1].c == c)
        {
          const piece op = b->b[loc1].piece;
          const int opro = b->b[loc1].promoted;
          if (piece_move_type[oc][op][opro] & (1<<(d+0)))
            return 1;
        }
      }
    }
  }

  /* two-step */
  if (dirs2)
  {
    int d;
    for (d=0; d<8; ++d)
    {
      if (dirs2 & (1<<d))
      {
        const int step = dir_map[d+1];
        const int loc1 = loc + 2*step;
        if (b->b[loc1].c == c)
        {
          const piece op = b->b[loc1].piece;
          const int opro = b->b[loc1].promoted;
          if (piece_move_type[oc][op][opro] & (1<<(d+8)))
            return 1;
        }
      }
    }
  }

  /* slides */
  if (dirsn)
  {
    int d;
    for (d=0; d<8; ++d)
    {
      if (dirsn & (1<<d))
      {
        const int step = dir_map[d+1];
        const int step_dx = dir_map_dx[d+1];
        const int step_dy = dir_map_dy[d+1];
        int i;
        for (i=1; i<12; ++i)
        {
          const int x1 = x + i*step_dx;
          const int y1 = y + i*step_dy;
          if (x1<0 || x1>=12 || y1<0 || y1>=12) break;
          const int loc1 = loc + i*step;
          if (b->b[loc1].c == c)
          {
            const piece op = b->b[loc1].piece;
            const int opro = b->b[loc1].promoted;
            if (piece_move_type[oc][op][opro] & (1<<(d+16)))
              return 1;
            break;
          }
          else if (b->b[loc1].c == oc)
          {
            break;
          }
        }
      }
    }
  }

  /* lions */
  {
    /* TODO: improve this */
    const int jump_x[8] = {-2, -1,  1,  2,  2,  1, -1, -2};
    const int jump_y[8] = { 1,  2,  2,  1, -1, -2, -2, -1};
    int i;
    for (i=0; i<8; ++i)
    {
      const int x1 = x + jump_x[i];
      const int y1 = y + jump_y[i];
      if (x1<0 || x1>=12 || y1<0 || y1>=12) continue;
      const int loc1 = loc + jump_x[i] + 12*jump_y[i];
      if (b->b[loc1].c == c)
      {
        const piece op = b->b[loc1].piece;
        const piece opro = b->b[loc1].promoted;
        if ((op == lion) || ((op == kirin) && opro))
          return 1;
      }
    }
  }

  return 0;
}

/* returns true if any piece _not_ a lion or promoted-kirin of color c attacks square loc */
static inline int is_square_attacked_no_lions(const board* b, int loc, color c)
{
  const color oc = c^3;

  const int x = loc % 12;
  const int y = loc / 12;

  const int dirs = square_neighbor_map[loc];
  const int dirs1 = (dirs & 0x0000FF);
  const int dirs2 = (dirs & 0x00FF00)>>8;
  const int dirsn = (dirs & 0xFF0000)>>16;

  /* one-step */
  if (dirs1)
  {
    int d;
    for (d=0; d<8; ++d)
    {
      if (dirs1 & (1<<d))
      {
        const int step = dir_map[d+1];
        const int loc1 = loc + step;
        if (b->b[loc1].c == c)
        {
          const piece op = b->b[loc1].piece;
          const int opro = b->b[loc1].promoted;
          if (!(op==lion || (op==kirin && opro)))
            if (piece_move_type[oc][op][opro] & (1<<(d+0)))
              return 1;
        }
      }
    }
  }

  /* two-step */
  if (dirs2)
  {
    int d;
    for (d=0; d<8; ++d)
    {
      if (dirs2 & (1<<d))
      {
        const int step = dir_map[d+1];
        const int loc1 = loc + 2*step;
        if (b->b[loc1].c == c)
        {
          const piece op = b->b[loc1].piece;
          const int opro = b->b[loc1].promoted;
          if (!(op==lion || (op==kirin && opro)))
            if (piece_move_type[oc][op][opro] & (1<<(d+8)))
              return 1;
        }
      }
    }
  }

  /* slides */
  if (dirsn)
  {
    int d;
    for (d=0; d<8; ++d)
    {
      if (dirsn & (1<<d))
      {
        const int step = dir_map[d+1];
        const int step_dx = dir_map_dx[d+1];
        const int step_dy = dir_map_dy[d+1];
        int i;
        for (i=1; i<12; ++i)
        {
          const int x1 = x + i*step_dx;
          const int y1 = y + i*step_dy;
          if (x1<0 || x1>=12 || y1<0 || y1>=12) break;
          const int loc1 = loc + i*step;
          if (b->b[loc1].c == c)
          {
            const piece op = b->b[loc1].piece;
            const int opro = b->b[loc1].promoted;
            if (!(op==lion || (op==kirin && opro)))
              if (piece_move_type[oc][op][opro] & (1<<(d+16)))
                return 1;
            break;
          }
          else if (b->b[loc1].c == oc)
          {
            break;
          }
        }
      }
    }
  }
  return 0;
}

/* returns true if _any_ king/prince of color c is in check */
/* move outward from king for possible attackers */
check_status in_check(const board* b, color c)
{
  int j;
  for (j=0; b->pl[c][king][j].valid; ++j)
  {
    if (!b->pl[c][king][j].active) continue;
    const int loc = b->pl[c][king][j].loc;
    if (is_square_attacked(b, loc, c^3))
      return king_check;
  }
  for (j=0; b->pl[c][drunk_elephant][j].valid; ++j)
  {
    if (!b->pl[c][drunk_elephant][j].active || !b->pl[c][drunk_elephant][j].promoted) continue;
    const int loc = b->pl[c][drunk_elephant][j].loc;
    if (is_square_attacked(b, loc, c^3))
      return king_check;
  }
  return no_check;
}

/* **************************************** */

/* here we return true simply if any piece of color c^3 attacks any lion/promoted-kirin of color c */
check_status in_check_lion(const board* b, color c)
{
  int j;
  /* check if non-lion gives check */
  for (j=0; b->pl[c][lion][j].valid; ++j)
  {
    if (!b->pl[c][lion][j].active) continue;
    const int loc = b->pl[c][lion][j].loc;
    if (is_square_attacked_no_lions(b, loc, c^3))
      return lion_check;
  }
  for (j=0; b->pl[c][kirin][j].valid; ++j)
  {
    if (!b->pl[c][kirin][j].active || !b->pl[c][kirin][j].promoted) continue;
    const int loc = b->pl[c][kirin][j].loc;
    if (is_square_attacked_no_lions(b, loc, c^3))
      return lion_check;
  }

  /* painful part: try all potential captures by lion */
  check_status flag = no_check;
  const int swap = (b->to_move == c);
  if (swap) make_move((board*)b, null_move());
  {
    movet ml[MAXNCAPMOVES];
    int i, n = move_generator_caps_lion(b, ml);
    for (i=0; i<n; ++i)
    {
      if (ml[i].is_special)
      {
        const int to = ml[i].special.from + dir_map[ml[i].special.to1];
        if (ml[i].special.capture1
            && ((b->b[to].piece == lion) || ((b->b[to].piece == kirin) && b->b[to].promoted)))
        {
          /* adjacent captures always legal */
          flag = lion_check;
          goto exit;
        }
        const int to2 = to + dir_map[ml[i].special.to2];
        if (ml[i].special.capture2
            && ((b->b[to2].piece == lion) || ((b->b[to2].piece == kirin) && b->b[to2].promoted)))
        {
          if (make_move((board*)b, ml[i]) != invalid_move)
          {
            unmake_move((board*)b);
            flag = lion_check;
            goto exit;
          }
        }
      }
      else
      {
        if ((b->b[ml[i].to].piece == lion) || ((b->b[ml[i].to].piece == kirin) && b->b[ml[i].to].promoted))
        {
          if (make_move((board*)b, ml[i]) != invalid_move)
          {
            unmake_move((board*)b);
            flag = lion_check;
            goto exit;
          }
        }
      }
    }
  }
exit:
  if (swap) unmake_move((board*)b);
  return flag;
}

/* **************************************** */

#define MAKE_MOVE(b,tm,f,t,p,pp,ix) \
  do { \
    (b)->h ^= piece_square_hash[(f)][(tm)][(p)][(pp)] ^ piece_square_hash[(t)][(tm)][(p)][(pp)]; \
    (b)->pl[(tm)][(p)][(ix)].loc = (t); \
    ++(b)->pl[(tm)][(p)][(ix)].tempi; \
    (b)->b[(t)] = (b)->b[(f)]; \
    (b)->b[(f)].i = 0; \
  } while (0)

#define MAKE_CAP(b,tm,t) \
  do { \
    const int otm = (tm)^3; \
    const int op = (b)->b[(t)].piece; \
    const int ox = (b)->b[(t)].idx; \
    const int opp = (b)->b[(t)].promoted; \
    const int ocap = (b)->pl[otm][op][ox].cannot_promote_unless_capture; \
    (b)->h ^= piece_square_hash[(t)][(otm)][(op)][(opp)]; \
    if (ocap) (b)->h ^= square_no_capture_hash[(t)]; \
    (b)->cap_stack[(b)->num_caps++] = (b)->b[(t)]; \
    (b)->pl[otm][op][ox].captured = 1; \
    (b)->pl[otm][op][ox].active = 0; \
    (b)->b[(t)].i = 0; \
    if (op==king) (b)->king_mask[(otm)] &= ~(1<<ox); \
    if (op==drunk_elephant && opp==1) (b)->prince_mask[(otm)] &= ~(1<<ox); \
  } while (0)
    
int make_move(board* b, movet m)
{
  /* TODO: check terminality in make_move! */
  if (!m.valid) { fstatus(stderr, "!%m!", m); return invalid_move; }

  const color tm = b->to_move;
  int no_lion_recapture = 0;
  int last_mover_had_no_promote_unless_capture = 0;
  if (MOVE_IS_NULL(m))
  {
    /* nop */
  }
  else if (MOVE_IS_LION(m))
  {
    const int to = m.special.from + dir_map[m.special.to1];
    const int to2 = to + dir_map[m.special.to2];

    /* cannot capture lion with our lion if
     *  - it is non-adjacent
     *  - it is non-defended (TODO: handle "hidden" defenders!)
     *  - we don't capture a non-pawn/non-go-between before
     */
    if (m.special.capture2)
    {
      const int otm = tm^3;
      const int our_lion = (m.piece==lion || (m.piece==kirin && m.promoted_piece));

      const int op2 = b->b[to2].piece;
      const int opp2 = b->b[to2].promoted;
      const int their_lion = (op2==lion || (op2==kirin && opp2));

      /* cannot recapture lion with a non-lion piece */
      if (b->flags.no_lion_recapture && their_lion && !our_lion) return invalid_move;

      if (our_lion && their_lion)
      {
        const int adjacent = (abs((m.special.from/12)-(to2/12)) <= 1) && (abs((m.special.from%12)-(to2%12)) <= 1);
        if (!adjacent)
        {
          square save_sq;
          /* remove lion to get "hidden defenders" */
          {
            save_sq = b->b[m.special.from];
            b->b[m.special.from].i = 0;
            b->rawpl[save_sq.raw_idx].active = 0;
            b->h ^= piece_square_hash[m.special.from][tm][m.special.piece][m.special.promoted_piece];
            b->hash_stack[b->ply] = b->h;
          }
          const int defended = is_square_attacked(b, to2, otm);
          /* restore square */
          {
            b->b[m.special.from] = save_sq;
            b->rawpl[save_sq.raw_idx].active = 1;
            b->h ^= piece_square_hash[m.special.from][tm][m.special.piece][m.special.promoted_piece];
            b->hash_stack[b->ply] = b->h;
          }

          if (defended)
          {
            if (!m.special.capture1) return invalid_move;
            const int op = b->b[to].piece;
            const int opp = b->b[to].promoted;
            const int petty = (op==pawn || op==go_between) && !opp;
            if (petty) return invalid_move;
          }
        }
      }

      /* can't recapture next turn with non-lion */
      if (their_lion && !our_lion) no_lion_recapture = 1;
    }

    if (m.special.capture1)
      MAKE_CAP(b, tm, to);
    if (m.special.capture2)
      MAKE_CAP(b, tm, to2);
    if (m.from != to2)
      MAKE_MOVE(b, tm, m.special.from, to2, m.special.piece, m.special.promoted_piece, m.special.idx);
  }
  else
  {
    if (m.capture)
    {
      const int otm = tm^3;
      const int op = b->b[m.to].piece;
      const int opp = b->b[m.to].promoted;
      const int our_lion = (m.piece==lion || (m.piece==kirin && m.promoted_piece));
      const int their_lion = (op==lion || (op==kirin && opp));

      /* cannot recapture lion with a non-lion piece */
      if (b->flags.no_lion_recapture && their_lion && !our_lion) return invalid_move;

      /* cannot capture non-adjacent defended lion with lion */
      if (our_lion && their_lion)
      {
        const int adjacent = (abs((m.from/12)-(m.to/12)) <= 1) && (abs((m.from%12)-(m.to%12)) <= 1);
        if (!adjacent)
        {
          /* remove lion to get "hidden defenders" */
          square save_sq;
          {
            save_sq = b->b[m.from];
            b->b[m.from].i = 0;
            b->rawpl[save_sq.raw_idx].active = 0;
            b->h ^= piece_square_hash[m.from][tm][m.piece][m.promoted_piece];
            b->hash_stack[b->ply] = b->h;
          }
          const int defended = is_square_attacked(b, m.to, otm);
          /* restore square */
          {
            b->b[m.from] = save_sq;
            b->rawpl[save_sq.raw_idx].active = 1;
            b->h ^= piece_square_hash[m.from][tm][m.piece][m.promoted_piece];
            b->hash_stack[b->ply] = b->h;
          }
          if (defended)
            return invalid_move;
        }
      }

      MAKE_CAP(b, tm, m.to);

      /* can't recapture next turn with non-lion */
      if (their_lion && !our_lion) no_lion_recapture = 1;

    }

    if (b->pl[tm][m.piece][m.idx].cannot_promote_unless_capture)
    {
      last_mover_had_no_promote_unless_capture = 1;
      b->pl[tm][m.piece][m.idx].cannot_promote_unless_capture = 0;
      b->h ^= square_no_capture_hash[m.from];
    }

    MAKE_MOVE(b, tm, m.from, m.to, m.piece, m.promoted_piece, m.idx);

    if (m.promote)
    {
      b->h ^= piece_square_hash[m.to][tm][m.piece][0] ^ piece_square_hash[m.to][tm][m.piece][1];
      b->pl[tm][m.piece][m.idx].promoted = 1;
      b->b[m.to].promoted = 1;
      if (m.piece == drunk_elephant)
        b->prince_mask[tm] |= (1 << m.idx);
    }
    else if (m.can_promote)
    {
      /* if we moved into promotion zone from outside and didn't promote,
       * then next move we cannot promote unless we capture
       */
      const int into_zone = (tm==white) ? (m.from < 8*12) : (m.from >= 4*12);
      if (into_zone)
      {
        b->pl[tm][m.piece][m.idx].cannot_promote_unless_capture = 1;
        b->h ^= square_no_capture_hash[m.to];
      }
    }
  }

  b->to_move ^= 3;
  b->h ^= btm_hash;
  b->move_stack[b->ply] = m;
  if (b->flags.no_lion_recapture != no_lion_recapture) {
    b->h ^= no_lion_recapture_hash;
    b->flags.no_lion_recapture = no_lion_recapture;
  }
  b->flags.last_mover_had_no_promote_unless_capture = last_mover_had_no_promote_unless_capture;
  ++b->ply;
  b->hash_stack[b->ply] = b->h;
  b->flags_stack[b->ply] = b->flags;
#ifdef  VALIDATE_BOARD
  if (validate_board(b)) { fstatus(stderr, "<===make_move(%m)\n", m); exit(-1); }
#endif
  return 0;
}

/* **************************************** */

#define UNMAKE_MOVE(b,tm,f,t,p,ix) \
  do { \
    (b)->pl[(tm)][(p)][(ix)].loc = (f); \
    --(b)->pl[(tm)][(p)][(ix)].tempi; \
    (b)->b[(f)] = b->b[(t)]; \
    (b)->b[(t)].i = 0; \
  } while (0)

#define UNMAKE_CAP(b,tm,t) \
  do { \
    (b)->b[(t)] = (b)->cap_stack[--(b)->num_caps]; \
    const int otm = (tm)^3; \
    const int op = b->b[(t)].piece; \
    const int ox = b->b[(t)].idx; \
    const int opp = (b)->b[(t)].promoted; \
    (b)->pl[otm][op][ox].captured = 0; \
    (b)->pl[otm][op][ox].active = 1; \
    if (op==king) (b)->king_mask[(otm)] |= (1<<ox); \
    if (op==drunk_elephant && opp==1) (b)->prince_mask[(otm)] |= (1<<ox); \
  } while (0)

int unmake_move(board* b)
{
  if (!b->move_stack[b->ply-1].valid) { fprintf(stderr, "!?"); return invalid_move; }
  --b->ply;
  movet m = b->move_stack[b->ply];
  color tm = b->to_move = b->to_move ^ 3;
  if (MOVE_IS_NULL(m))
  {
    /* nop */
  }
  else if (MOVE_IS_LION(m))
  {
    int to = m.special.from + dir_map[m.special.to1];
    int to2 = to + dir_map[m.special.to2];
    if (m.from != to2)
      UNMAKE_MOVE(b, tm, m.special.from, to2, m.special.piece, m.special.idx);
    if (m.special.capture2)
      UNMAKE_CAP(b, tm, to2);
    if (m.special.capture1)
      UNMAKE_CAP(b, tm, to);
  }
  else
  {
    UNMAKE_MOVE(b, tm, m.from, m.to, m.piece, m.idx);
    if (m.capture)
      UNMAKE_CAP(b, tm, m.to);
    if (m.promote)
    {
      b->pl[tm][m.piece][m.idx].promoted = 0;
      b->b[m.from].promoted = 0;
      if (m.piece == drunk_elephant)
        b->prince_mask[tm] &= ~(1 << m.idx);
    }
    b->pl[tm][m.piece][m.idx].cannot_promote_unless_capture = b->flags_stack[b->ply+1].last_mover_had_no_promote_unless_capture;
  }
  b->h = b->hash_stack[b->ply];
  b->flags = b->flags_stack[b->ply];
#ifdef  VALIDATE_BOARD
  if (validate_board(b)) { fstatus(stderr, "<===unmake_move(%m)\n", m); exit(-2); }
#endif
  return 0;
}

/* **************************************** */

int validate_board(const board* b)
{
  int i, j, k, ok=0;

  if (b->to_move != white && b->to_move != black) { fprintf(stderr, "{TM=%i}", b->to_move); ++ok; }

  for (i=1; i<num_color; ++i)
  {
    for (j=1; j<num_piece; ++j)
    {
      for (k=0; b->pl[i][j][k].valid; ++k)
      {
        piece_info pi = b->pl[i][j][k];
        square sq = b->b[pi.loc];
        if ((pi.active && !pi.captured) && (sq.c!=i || sq.piece!=j || sq.idx!=k || pi.promoted!=sq.promoted))
        {
          fprintf(stderr, "{%c%s%i,",
                  i==white?'W':i==black?'B':'?', piece_names[j], k);
          fprintf(stderr, "pl=<%i%c%c%c>", pi.loc, pi.valid?'V':'v',
                  pi.active?'A':'a', pi.captured?'C':'c');
          fprintf(stderr, "sq=<%c%s%i>", sq.c==white?'W':sq.c==black?'B':'?',
                  piece_names[sq.piece], sq.idx);
          fprintf(stderr, "(Pr%i--%i)", pi.promoted, sq.promoted);
          fprintf(stderr, "}");
          ++ok;
        }
      }
    }
  }

  for (i=0; i<12*12; ++i)
  {
    square sq = b->b[i];
    if (sq.c == none) continue;
    piece_info pi = b->pl[sq.c][sq.piece][sq.idx];
    if (pi.captured || !pi.valid || !pi.active || pi.loc!=i || pi.promoted!=sq.promoted)
    {
      fprintf(stderr, "[%i(%i%c),", i, 1+(11-(i%12)), 'a'+(11-(i/12)));
      fprintf(stderr, "sq=<%c%s%i>", sq.c==white?'W':sq.c==black?'B':'?',
              piece_names[sq.piece], sq.idx);
      fprintf(stderr, "pl=<%i(%i%c)%c%c%c>", pi.loc, 1+(11-(pi.loc%12)), 'a'+(11-(pi.loc/12)),
              pi.valid?'V':'v', pi.active?'A':'a', pi.captured?'C':'c');
      fprintf(stderr, "(Pr%i--%i)", pi.promoted, sq.promoted);
      fprintf(stderr, "]");
      ++ok;
    }
  }

  if (b->h != rehash_board(b))
  {
    fprintf(stderr, "{b->h:%016llX != %016llX:rehash}", b->h, rehash_board(b));
    ++ok;
  }
  if (ok) {
    fprintf(stderr, "\n");
    fprintf(stderr, "Error in validate_board()\n");
    show_board(stderr, b);
  }
  return ok;
}

/* **************************************** */

int board_to_fen(const board* b, char* chp)
{
  int r, c;
  *chp++ = '[';
  for (r=11; r>=0; --r)
  {
    int blanks = 0, comma = 0;
    for (c=0; c<12; ++c)
    {
      int i = r*12 + c;
      if (b->b[i].c == white)
      {
        if (blanks) { if (comma) *chp++ = ','; chp += sprintf(chp, "%i", blanks); comma = 1; }
        if (comma) *chp++ = ',';
        blanks = 0;
        const char* nmp = piece_names[b->b[i].piece];
        if (b->b[i].promoted) *chp++ = '+';
        for (; *nmp; ++nmp) *chp++ = tolower((unsigned char)*nmp);
        if (b->rawpl[b->b[i].raw_idx].cannot_promote_unless_capture) *chp++ = '=';
        comma = 1;
      }
      else if (b->b[i].c == black)
      {
        if (blanks) { if (comma) *chp++ = ','; chp += sprintf(chp, "%i", blanks); }
        if (comma) *chp++ = ',';
        blanks = 0;
        const char* nmp = piece_names[b->b[i].piece];
        if (b->b[i].promoted) *chp++ = '+';
        for (; *nmp; ++nmp) *chp++ = toupper((unsigned char)*nmp);
        if (b->rawpl[b->b[i].raw_idx].cannot_promote_unless_capture) *chp++ = '=';
        comma = 1;
      }
      else
      {
        ++blanks;
      }
    }
    if (blanks) { if (comma) *chp++ = ','; else comma=1; chp += sprintf(chp, "%i", blanks); comma = 1; }
    if (r) *chp++ = '/';
  }
  *chp++ = ' ';
  *chp++ = (b->to_move == white) ? 'w' : 'b';
  /* TODO: promotability status, lion capture status */
  *chp++ = ']';
  *chp = '\x00';
  return 0;
}

/* example:
 * lfcsgekgscfl/a1b1txot1b1a/mvrhdqndhrvm/pppppppppppp/3i4i3/12/12/3I4I3/PPPPPPPPPPPP/MVRHDNQDHRVM/A1B1TOXT1B1A/LFCSGKEGSCFL w - 0 1
 * 
 * uses "+" preceding piece for promoted pieces
 *
 * L = lance
 * F = ferocious leopard
 * C = copper general
 * S = silver general
 * G = gold general
 * E = drunk elephant
 * K = king
 * N = lion
 * A = reverse chariot
 * B = bishop
 * T = blind tiger
 * X = phoenix
 * O = kirin
 * M = side mover
 * V = vertical mover
 * R = rook
 * H = dragon horse
 * D = dragon king
 * Q = free king
 * P = pawn
 * I = go-between
 */
int wbfen_to_board(board* b, const char* chp) {
  static const char white_mapping[] = "LFCSGEKNABTXOMVRHDQPI";
  static const char black_mapping[] = "lfcsgeknabtxomvrhdqpi";
  static const piece mapping_piece[] = {
    lance, ferocious_leopard, copper_general, silver_general, gold_general,
    drunk_elephant, king, lion, reverse_chariot, bishop,
    blind_tiger, phoenix, kirin, side_mover, vertical_mover,
    rook, dragon_horse, dragon_king, free_king, pawn,
    go_between
  };

  int r, c, promoted;
  memset(b, '\x00', sizeof(board));

  /* layout pieces on board */
  r = 11;
  c = 0;
  promoted = 0;
  for (; *chp && *chp!=' '; ++chp)
  {
    const char* srch;
    if (*chp == '/')
    {
      --r;
      c = 0;
      promoted = 0;
      //sendlog("new row (r=%i c=%i)\n", r, c);
    }
    else if (*chp == '+')
    {
      promoted = 1;
      //sendlog("+");
    }
    else if (strchr("0123456789", *chp))
    {
      char buff[8];
      int n;
      for (n=0; *chp && strchr("0123456789", *chp); ++n)
        buff[n] = *chp++;
      --chp;
      buff[n] = '\x00';
      int num = atoi(buff);
      c += num;
      promoted = 0;
      //sendlog("skip %i\t", num, r, c);
    }
    else if ((srch=strchr(white_mapping, *chp)))
    {
      const int rc = r*12 + c;
      piece p = mapping_piece[srch - white_mapping];
      b->b[rc].c = white;
      b->b[rc].piece = p;
      b->b[rc].promoted = promoted;
      for (b->b[rc].idx=0; b->pl[white][p][b->b[rc].idx].valid; ++b->b[rc].idx);
      b->pl[white][p][b->b[rc].idx].loc = rc;
      b->pl[white][p][b->b[rc].idx].valid = 1;
      b->pl[white][p][b->b[rc].idx].active = 1;
      b->pl[white][p][b->b[rc].idx].promoted = promoted;
      if (p==king) b->king_mask[white] |= (1 << b->b[rc].idx);
      if (p==drunk_elephant && promoted) b->prince_mask[white] |= (1 << b->b[rc].idx);
      b->b[rc].raw_idx = &b->pl[white][p][b->b[rc].idx] - (piece_info*)b->pl;
      ++c;
      promoted = 0;
      //sendlog("w%s\t", piece_names[p]);
    }
    else if ((srch=strchr(black_mapping, *chp)))
    {
      const int rc = r*12 + c;
      piece p = mapping_piece[srch - black_mapping];
      b->b[rc].c = black;
      b->b[rc].piece = p;
      b->b[rc].promoted = promoted;
      for (b->b[rc].idx=0; b->pl[black][p][b->b[rc].idx].valid; ++b->b[rc].idx);
      b->pl[black][p][b->b[rc].idx].loc = rc;
      b->pl[black][p][b->b[rc].idx].valid = 1;
      b->pl[black][p][b->b[rc].idx].active = 1;
      b->pl[black][p][b->b[rc].idx].promoted = promoted;
      if (p==king) b->king_mask[black] |= (1 << b->b[rc].idx);
      if (p==drunk_elephant && promoted) b->prince_mask[black] |= (1 << b->b[rc].idx);
      b->b[rc].raw_idx = &b->pl[black][p][b->b[rc].idx] - (piece_info*)b->pl;
      ++c;
      promoted = 0;
      //sendlog("b%s\t", piece_names[p]);
    }
    else
    {
      promoted = 0;
      //sendlog("'%c'\t", *chp);
    }
  }

  /* skip space */
  ++chp;

  /* to-move */
  if (*chp=='w')
    b->to_move = white;
  else
    b->to_move = black;

  /* (and skip the rest for now) */

  b->h = rehash_board(b);
  b->hash_stack[0] = b->h;
  b->flags_stack[0] = b->flags;
  b->ply = 0;
  return 0;
}

int fen_to_board(board* b, const char* chp) {
  /* TODO: implement fen_to_board */
  return (void*)b==(void*)chp;
}

int board_to_sgf(const board* b, char* chp) {
  /* TODO: implement board_to_sgf */
  return (void*)b==(void*)chp;
}

int sgf_to_board(board* b, const char* chp) {
  /* TODO: implement sgf_to_board */
  return (void*)b==(void*)chp;
}

/* winboard move rep looks like:
 * a0a1
 * e4e5,e5e6
 * @@@@
 */
int move_to_winboard(movet m, char* buff1, char* buff2)
{
  if (MOVE_IS_NULL(m))
  {
    sprintf(buff1, "@@@@");
    buff2[0] = '\x00';
  }
  else if (MOVE_IS_LION(m))
  {
    int to = m.special.from + dir_map[m.special.to1];
    int to2 = to + dir_map[m.special.to2];
    if (m.special.from==to && to==to2)
    {
      /* HaChu has problems with pass moves... */
      sprintf(buff1, "@@@@");

      /* winboard rejects this: */
      //sprintf(buff1, "%c%i%c%i", (m.from%12)+'a', m.from/12, (to%12)+'a', to/12);

      /* could use this if have free square nearby: */
      // sprintf(buff1, "%c%i%c%i,", (m.from%12)+'a', m.from/12, (to%12)+'a', to/12);
      // sprintf(buff2, "%c%i%c%i", (to%12)+'a', to/12, (to2%12)+'a', to2/12);
    }
    else
    {
      sprintf(buff1, "%c%i%c%i,", (m.from%12)+'a', m.from/12, (to%12)+'a', to/12);
      sprintf(buff2, "%c%i%c%i", (to%12)+'a', to/12, (to2%12)+'a', to2/12);
     }
  }
  else
  {
    sprintf(buff1, "%c%i%c%i", (m.from%12)+'a', m.from/12, (m.to%12)+'a', m.to/12);
    if (m.promote)
      strcat(buff1, "+");
    else if (m.can_promote)
      strcat(buff1, "=");
    buff2[0] = '\x00';
  }
  return 0;
}
movet winboard_to_move(const board* b, const char* movestring)
{
  static int initialize_regexes = 1;
  static regex_t null_pattern, normal_pattern, lion_pattern;
  if (initialize_regexes)
  {
    initialize_regexes = 0;
    regcomp(&null_pattern, "^@@@@|null$", REG_EXTENDED);
    regcomp(&normal_pattern, "^([a-z])([0-9]+)([a-z])([0-9]+)([+=]?)$", REG_EXTENDED);
    regcomp(&lion_pattern, "^([a-z])([0-9]+)([a-z])([0-9]+),([a-z])([0-9]+)([a-z])([0-9]+)[+=]?$", REG_EXTENDED);
  }

  movet  m;
  m.i = 0;

  regmatch_t  pmatches[9] = {{-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1}};

  if (regexec(&null_pattern, movestring, 0, NULL, 0)==0)
  {
    m = null_move();
  }
  else if (regexec(&normal_pattern, movestring, 6, pmatches, 0)==0)
  {
    char buff[8];

    int fc = movestring[pmatches[1].rm_so] - 'a';
    memset(buff, '\x00', sizeof(buff)); memcpy(buff, movestring+pmatches[2].rm_so, pmatches[2].rm_eo-pmatches[2].rm_so);
    int fr = atoi(buff);

    int tc = movestring[pmatches[3].rm_so] - 'a';
    memset(buff, '\x00', sizeof(buff)); memcpy(buff, movestring+pmatches[4].rm_so, pmatches[4].rm_eo-pmatches[4].rm_so);
    int tr = atoi(buff);

    char promote = movestring[pmatches[5].rm_so];
    int from = fr*12 + fc;
    int to = tr*12 + tc;
    movet ml[MAXNMOVES];
    int i, n = gen_moves(b, ml);
    for (i=0; i<n; ++i)
    {
      if (MOVE_IS_NULL(ml[i]))
      {
        continue;
      }
      else if (MOVE_IS_LION(ml[i]))
      {
        continue;
      }
      else if (ml[i].from==from && ml[i].to==to)
      {
        if (!ml[i].can_promote || (ml[i].promote == (promote=='+')))
        {
          m = ml[i];
          break;
        }
      }
    }
  }
  else if (regexec(&lion_pattern, movestring, 9, pmatches, 0)==0)
  {
    char buff[8];

    int fc = movestring[pmatches[1].rm_so] - 'a';
    memset(buff, '\x00', sizeof(buff)); memcpy(buff, movestring+pmatches[2].rm_so, pmatches[2].rm_eo-pmatches[2].rm_so);
    int fr = atoi(buff);

    int tc = movestring[pmatches[3].rm_so] - 'a';
    memset(buff, '\x00', sizeof(buff)); memcpy(buff, movestring+pmatches[4].rm_so, pmatches[4].rm_eo-pmatches[4].rm_so);
    int tr = atoi(buff);

    //int fc2 = movestring[pmatches[5].rm_so] - 'a';
    //memset(buff, '\x00', sizeof(buff)); memcpy(buff, movestring+pmatches[6].rm_so, pmatches[6].rm_eo-pmatches[6].rm_so);
    //int fr2 = atoi(buff);

    int tc2 = movestring[pmatches[7].rm_so] - 'a';
    memset(buff, '\x00', sizeof(buff)); memcpy(buff, movestring+pmatches[8].rm_so, pmatches[8].rm_eo-pmatches[8].rm_so);
    int tr2 = atoi(buff);

    int from = fr*12 + fc;
    int to = tr*12 + tc;
    //int from2 = fr2*12 + fc2;
    int to2 = tr2*12 + tc2;
    movet ml[MAXNMOVES];
    int i, n = gen_moves(b, ml);
    for (i=0; i<n; ++i)
    {
      if (MOVE_IS_NULL(ml[i]))
      {
        continue;
      }
      else if (MOVE_IS_LION(ml[i]))
      {
        int mto = ml[i].special.from + dir_map[ml[i].special.to1];
        int mto2 = to + dir_map[ml[i].special.to2];
        if (ml[i].special.from==from && mto==to && mto2==to2)
        {
          m = ml[i];
          break;
        }
      }
      else
      {
        continue;
      }
    }
  }

  return m;
}

/* **************************************** */

int show_move_eng(FILE* f, movet m)
{
  /* TODO: tidy up this code */
  if (!m.valid) return fprintf(f, "<invalid:%08X>", m.i);
  if (m.is_special)
  {
    if (m.special.movetype)
    {
      /* lion-type move */
      int to = m.from + dir_map[m.special.to1];
      int to2 = to + dir_map[m.special.to2];

      fprintf(f, "%s%s%i%c",
          (m.special.promoted_piece ? "+" : ""), piece_names[m.special.piece],
          1+(11-(m.from%12)), 'a'+(11-(m.from/12))
          );

      if (to2 == m.from)
      {
        fprintf(f, "!");
        if (m.special.capture1)
          fprintf(f, "x%i%c", 1+(11-(to%12)), 'a'+(11-(to/12)));
      }
      else
      {
        fprintf(f, "%s%i%c", (m.special.capture1 ? "x" : "-"), 1+(11-(to%12)), 'a'+(11-(to/12)));
        fprintf(f, "%s%i%c", (m.special.capture2 ? "x" : "-"), 1+(11-(to2%12)), 'a'+(11-(to2/12)));
      }
    }
    else
    {
      /* null-move */
      fprintf(f, "<null>");
    }
  }
  else
  {
    fprintf(f, "%s%s%i%c%c%i%c%s",
        (m.promoted_piece ? "+" : ""), piece_names[m.piece],
        1+(11-(m.from%12)), 'a'+(11-(m.from/12)),
        m.capture ? 'x' : '-',
        1+(11-(m.to%12)), 'a'+(11-(m.to/12)),
        (m.can_promote ? (m.promote ? "+" : "=") : "")
        );
  }
  return 0;
}

int show_move_kanji(FILE* f, movet m)
{
  /* TODO: tidy up this code */
  if (!m.valid) return fprintf(f, "<invalid:%08X>", m.i);
  if (m.is_special)
  {
    if (m.special.movetype)
    {
      /* lion-type move */
      int to = m.from + dir_map[m.special.to1];
      int to2 = to + dir_map[m.special.to2];

      fprintf(f, "%s%s(%s%s/%s)%i%c",
          (m.special.promoted_piece ? "+" : ""), piece_names[m.special.piece],
          piece_kanji[m.special.promoted_piece][m.special.piece][0],
            piece_kanji[m.special.promoted_piece][m.special.piece][1],
          piece_kanji1[m.special.promoted_piece][m.special.piece],
          1+(11-(m.from%12)), 'a'+(11-(m.from/12))
          );

      if (to2 == m.from)
      {
        fprintf(f, "!");
        if (m.special.capture1)
          fprintf(f, "x%i%c", 1+(11-(to%12)), 'a'+(11-(to/12)));
      }
      else
      {
        fprintf(f, "%s%i%c", (m.special.capture1 ? "x" : "-"), 1+(11-(to%12)), 'a'+(11-(to/12)));
        fprintf(f, "%s%i%c", (m.special.capture2 ? "x" : "-"), 1+(11-(to2%12)), 'a'+(11-(to2/12)));
      }
    }
    else
    {
      /* null-move */
      fprintf(f, "<null>");
    }
  }
  else
  {
    fprintf(f, "%s%s(%s%s/%s)%i%c%c%i%c%s",
        (m.promoted_piece ? "+" : ""), piece_names[m.piece],
        piece_kanji[m.promoted_piece][m.piece][0],
          piece_kanji[m.promoted_piece][m.piece][1],
        piece_kanji1[m.promoted_piece][m.piece],
        1+(11-(m.from%12)), 'a'+(11-(m.from/12)),
        m.capture ? 'x' : '-',
        1+(11-(m.to%12)), 'a'+(11-(m.to/12)),
        (m.can_promote ? (m.promote ? "+" : "=") : "")
        );
  }
  return 0;
}

int show_move(FILE* f, movet m)
{
  /* TODO: tidy up this code */
  if (!m.valid) return fprintf(f, "<invalid:%08X>", m.i);
  if (m.is_special)
  {
    if (m.special.movetype)
    {
      /* lion-type move */
      int to = m.from + dir_map[m.special.to1];
      int to2 = to + dir_map[m.special.to2];

      fprintf(f, "%s%s(%s%s/%s)%i%c",
          (m.special.promoted_piece ? "+" : ""), piece_names[m.special.piece],
          piece_kanji[m.special.promoted_piece][m.special.piece][0],
            piece_kanji[m.special.promoted_piece][m.special.piece][1],
          piece_kanji1[m.special.promoted_piece][m.special.piece],
          1+(11-(m.from%12)), 'a'+(11-(m.from/12))
          );

      if (to2 == m.from)
      {
        fprintf(f, "!");
        if (m.special.capture1)
          fprintf(f, "x%i%c", 1+(11-(to%12)), 'a'+(11-(to/12)));
      }
      else
      {
        fprintf(f, "%s%i%c", (m.special.capture1 ? "x" : "-"), 1+(11-(to%12)), 'a'+(11-(to/12)));
        fprintf(f, "%s%i%c", (m.special.capture2 ? "x" : "-"), 1+(11-(to2%12)), 'a'+(11-(to2/12)));
      }
    }
    else
    {
      /* null-move */
      fprintf(f, "<null>");
    }
  }
  else
  {
    fprintf(f, "%s%s(%s%s/%s)%i%c%c%i%c%s",
        (m.promoted_piece ? "+" : ""), piece_names[m.piece],
        piece_kanji[m.promoted_piece][m.piece][0],
          piece_kanji[m.promoted_piece][m.piece][1],
        piece_kanji1[m.promoted_piece][m.piece],
        1+(11-(m.from%12)), 'a'+(11-(m.from/12)),
        m.capture ? 'x' : '-',
        1+(11-(m.to%12)), 'a'+(11-(m.to/12)),
        (m.can_promote ? (m.promote ? "+" : "=") : "")
        );
  }
  return 0;
}

/* **************************************** */

#if 1
int show_board(FILE* f, const board* b)
{
  int i, j;
#ifdef USE_COLORS
  ansi_bg_black(f);
#endif
  fprintf(f, "  12 11 10  9  8  7  6  5  4  3  2  1\n");
  fprintf(f, " +--+--+--+--+--+--+--+--+--+--+--+--+\n");
  for (j=11; j>=0; --j)
  {
    fprintf(f, "%c|", 'a'+(11-j));
    for (i=0; i<12; ++i)
    {
      if (b->b[j*12+i].c==none)
      {
#ifdef USE_COLORS
        ansi_faint(f);
#endif
        fprintf(f, ((j+i)%2) ? "  " : "::");
#ifdef USE_COLORS
        ansi_normal(f);
#endif
      }
      else
      {
        const color c = b->b[j*12+i].c;
        const piece pc = b->b[j*12+i].piece;
        const int pro = b->b[j*12+i].promoted;
        const int raw_idx = b->b[j*12+i].raw_idx;
#ifdef USE_COLORS
        if (pro)
        {
          if (c==white) ansi_fg_red(f); else ansi_fg_green(f);
        }
        else
        {
          if (c==white) {} else ansi_fg_yellow(f);
        }
        if (!b->rawpl[raw_idx].cannot_promote_unless_capture)
          ansi_bright(f);
#endif
        if (pc==king && c==black)
          fprintf(f, "%2s", other_king_kanji[0]);
        else
          fprintf(f, "%2s", piece_kanji1[pro][pc]);
#ifdef USE_COLORS
        ansi_fg_white(f);
        ansi_normal(f);
#endif
      }
      fprintf(f, ((i==11) ? "|" : ((j==8 || j==4) && (i==3 || i==7)) ? "." : " "));
    }
    fprintf(f, "%c", 'a'+(11-j));
    if (j==11) {
      fprintf(f, "  [%016llX]", b->h);
      fprintf(f, "[%016llX]", rehash_board(b));
    } else if (j==10) {
      fprintf(f, "  %s", (in_check(b, white) ? "*W" : "  "));
      fprintf(f, " %s",  (in_check(b, black) ? "*B" : "  "));
      fprintf(f, "  %s", (in_check_lion(b, white) ? "*lW" : "   "));
      fprintf(f, " %s",  (in_check_lion(b, black) ? "*lB" : "   "));
    } else if (j==9) {
      fprintf(f, "  k%04X|%04X", b->king_mask[white], b->king_mask[black]);
      fprintf(f, " p%04X|%04X", b->prince_mask[white], b->prince_mask[black]);
      if (b->flags.no_lion_recapture) fprintf(f, "  %s", "!Lion");
      if (b->flags.last_mover_had_no_promote_unless_capture) fprintf(f, "  %s", "~lmhnpuc");
    } else if (j==8) {
      fprintf(f, "  (%i) %s", b->ply, (b->to_move==white ? "White to move(先手)" : b->to_move==black ? "Black to move(後手)" : "(??\?)"));
      if (terminal(b)) fprintf(f, "#");
    } else if (j<7) {
      if (b->ply>j)
      {
        fprintf(f, "  %c%c",
            b->flags_stack[b->ply-1-j].no_lion_recapture ? 'L' : ' ',
            b->flags_stack[b->ply-1-j].last_mover_had_no_promote_unless_capture ? '=' : ' ');
        fstatus(f, ((b->to_move==white)^(j%2)) ? "  %i...%m" : "  %i.%m",
            (b->ply-j-1)/2+1, b->move_stack[b->ply-1-j]);
      }
    }
    fprintf(f, "\n");
  }
  fprintf(f, " +--+--+--+--+--+--+--+--+--+--+--+--+\n");
  fprintf(f, "  12 11 10  9  8  7  6  5  4  3  2  1\n");
  {
    char buff[1024];
    board_to_fen(b, buff);
    fprintf(f, "CSFEN: %s\n\n", buff);
  }
  {
    movet ml[MAXNMOVES];
    int n = gen_moves(b, ml);
    fstatus(f, "Moves: %M\n\n", ml, n);
  }

  if (0 && logfile()) {
    int r, c;
    int counts_w[12*12], counts_b[12*12];
    compute_board_control(b, white, counts_w);
    compute_board_control(b, black, counts_b);
    for (r=11; r>=0; --r)
    {
      for (c=0; c<12; ++c)
      {
        if (counts_w[r*12 + c])
          fprintf(logfile(), "  %2i", counts_w[r*12 + c]);
        else
          fprintf(logfile(), "   .");
        if (counts_b[r*12 + c])
          fprintf(logfile(), "%-3i", -counts_b[r*12 + c]);
        else
          fprintf(logfile(), "-. ");
      }
      fprintf(logfile(), "\n");
    }
  }
  return 0;
}
#elif 0
int show_board(FILE* f, const board* b)
{
  int i, j, k;
  fprintf(f, "  12 11 10  9  8  7  6  5  4  3  2  1\n");
  fprintf(f, " +--+--+--+--+--+--+--+--+--+--+--+--+\n");
  for (j=11; j>=0; --j)
  {
    for (k=0; k<2; ++k)
    {
      fprintf(f, "%c|", (k ? ' ' : 'a'+(11-j)));
      for (i=0; i<12; ++i)
      {
        if (b->b[j*12+i].c==none)
        {
#ifdef USE_COLORS
          ansi_faint(f);
#endif
          fprintf(f, ((j+i)%2) ? "  " : "::");
#ifdef USE_COLORS
          ansi_normal(f);
#endif
        }
        else
        {
#ifdef USE_COLORS
          if (b->b[j*12+i].c==white) ansi_bright(f); else {ans_bright(f); ansi_fg_yellow(f);}
#endif
          if ((b->b[j*12+i].piece == king) && (b->b[j*12+i].c==black))
          {
            fprintf(f, "%2s", other_king_kanji[k]);
          }
          else
          {
            const int c = b->b[j*12+i].c;
            const int pc = b->b[j*12+i].piece;
            const int idx = b->b[j*12+i].idx;
            fprintf(f, "%2s", piece_kanji[b->pl[c][pc][idx].promoted][pc][k]);
          }
#ifdef USE_COLORS
          ansi_fg_white(f);
          ansi_normal(f);
#endif
        }
        fprintf(f, ((i==11) ? "|" : " "));
      }
      fprintf(f, "%c", 'a'+(11-j));
      if (j==11) {
        fprintf(f, "  [%016llX]", b->h);
      } else if (j==10) {
        fprintf(f, "  [%016llX]", rehash_board(b));
      } else if (j==5) {
        fprintf(f, "  (%i) %s", b->ply,
                b->to_move==white ? "White to move(先手)" : "Black to move(後手)");
      } else if (j<5) {
        if (b->ply>j)
          fstatus(f, ((b->to_move==white)^(j%2)) ? "  %i...%m" : "  %i.%m",
              (b->ply-j-1)/2+1, b->move_stack[b->ply-1-j]);
      }
      fprintf(f, "\n");
    }
  }
  fprintf(f, " +--+--+--+--+--+--+--+--+--+--+--+--+\n");
  fprintf(f, "  12 11 10  9  8  7  6  5  4  3  2  1\n");
  return 0;
}
#else
int show_board(FILE* f, const board* b)
{
  int i, j;
  fprintf(f, "    12     11     10      9      8      7      6      5      4      3      2      1    \n");
  fprintf(f, " #======+======+======+======+======+======+======+======+======+======+======+======# \n");
  for (j=11; j>=0; --j)
  {
    fprintf(f, "%c|", 'a'+(11-j));
    for (i=0; i<12; ++i)
    {
      if (b->b[j*12+i].c==none)
      {
#ifdef USE_COLORS
        ansi_faint(f);
#endif
        fprintf(f, ((j+i)%2) ? "      " : "  ::  ");
#ifdef USE_COLORS
        ansi_normal(f);
#endif
        fprintf(f, "|");
      } else {
#ifdef USE_COLORS
        if (b->b[j*12+i].c==white) ansi_bright(f); else {ansi_bright(f); ansi_fg_yellow(f);}
#endif
        if ((b->b[j*12+i].piece == king) && (b->b[j*12+i].c==black))
        {
          fprintf(f, "%c %2s%2s",
                 b->b[j*12+i].c==white ? 'w' : 'b',
                 other_king_kanji[0], other_king_kanji[1]);
        }
        else
        {
          const int c = b->b[j*12+i].c;
          const int pc = b->b[j*12+i].piece;
          const int idx = b->b[j*12+i].idx;
          fprintf(f, "%c %2s%2s",
                 b->b[j*12+i].c==white ? 'w' : 'b',
                 piece_kanji[b->pl[c][pc][idx].promoted][b->b[j*12+i].piece][0],
                 piece_kanji[b->pl[c][pc][idx].promoted][b->b[j*12+i].piece][1]);
        }
#ifdef USE_COLORS
        ansi_fg_white(f);
        ansi_normal(f);
#endif
        fprintf(f, "|");
      }
    }
    fprintf(f, "%c", 'a'+(11-j));
    if (j==11) {
      fprintf(f, "  [%016llX]", b->h);
    } else if (j==10) {
      fprintf(f, "  [%016llX]", rehash_board(b));
    } else if (j==5) {
      fprintf(f, "  (%i) %s", b->ply,
              b->to_move==white ? "White to move(先手)" : "Black to move(後手)");
    } else if (j<5) {
      if (b->ply>j)
        fstatus(f, ((b->to_move==white)^(j%2)) ? "  %i...%m" : "  %i.%m",
            (b->ply-j-1)/2+1, b->move_stack[b->ply-1-j]);
    }
    fprintf(f, "\n");
    if (j==8 || j==4)
      fprintf(f, " +------|------|------|------*------|------|------|------*------|------|------|------+ \n");
    else if (j)
      fprintf(f, " +------|------|------|------|------|------|------|------|------|------|------|------+ \n");
  }
  fprintf(f, " #======+======+======+======+======+======+======+======+======+======+======+======# \n");
  fprintf(f, "    12     11     10      9      8      7      6      5      4      3      2      1    \n");
  return 0;
}
#endif
