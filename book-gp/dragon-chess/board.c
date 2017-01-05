/* $Id: board.c,v 1.4 2010/12/23 23:59:43 apollo Exp apollo $ */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "board.h"
#include "search.h"
#include "util.h"

//#define VALIDATE_MOVE

static hasht hash_piece[2][max_piece-1][3][8][12];
static hasht hash_to_move[2];

/* ************************************************************ */

#define col_normal "\033[0m"
#define col_bold "\033[1m"
#define col_fg_black "\033[30m"
#define col_fg_red "\033[31m"
#define col_fg_green "\033[32m"
#define col_fg_yellow "\033[33m"
#define col_fg_blue "\033[34m"
#define col_fg_magenta "\033[35m"
#define col_fg_cyan "\033[36m"
#define col_fg_white "\033[37m"
#define col_bg_black "\033[40m"
#define col_bg_red "\033[41m"
#define col_bg_green "\033[42m"
#define col_bg_yellow "\033[43m"
#define col_bg_blue "\033[44m"
#define col_bg_magenta "\033[45m"
#define col_bg_cyan "\033[46;22m"
#define col_bg_white "\033[47m"

static const char piece_figs[3][max_piece] = {
 {'&', '&', '&', '&', '&', '&', '&', '&', '&', '&', '&', '&', '&', '&', '&', '&'},
 {'!', 'S', 'G', 'R', 'O', 'U', 'H', 'T', 'C', 'M', 'K', 'P', 'W', 'B', 'E', 'D'},
 {'?', 's', 'g', 'r', 'o', 'u', 'h', 't', 'c', 'm', 'k', 'p', 'w', 'b', 'e', 'd'}
};
static const char* border_col[3] = {col_fg_red, col_fg_green, col_fg_blue};
static const char* dark_square_col[3] = {col_bg_red, col_bg_green, col_bg_cyan};

int show_board(FILE* f, const board* b) {
  int x, y, z;
  fprintf(f, "  %sabcdefghijkl%s    %sabcdefghijkl%s    %sabcdefghijkl%s",
    border_col[0], col_normal, border_col[1], col_normal, border_col[2], col_normal);
  fprintf(f, "  [%016llX||%016llX]\n", hash_board(b), board_hash(b));
  fprintf(f, " %sB------------B%s  %sM------------M%s  %sT------------T%s\n",
    border_col[0], col_normal, border_col[1], col_normal, border_col[2], col_normal);
  for (y=7; y>=0; --y) {
    for (z=0; z<3; ++z) {
      fprintf(f, "%s%i|%s", border_col[z], y+1, col_normal);
      for (x=0; x<12; ++x) {
        //if (!((x+y)%2)) fprintf(f, "%s", (x>=4 && x<8 && y>=2 && y<6) ? col_bg_white : dark_square_col[z]);
        if (!((x+y)%2)) fprintf(f, "%s", col_bg_white);
        if (b->b[z][y][x].s) {
          if (!b->pieces[b->b[z][y][x].s-1][b->b[z][y][x].p][b->b[z][y][x].idx].frozen) {
            fprintf(f, "%s%c%s", col_bold, piece_figs[b->b[z][y][x].s][b->b[z][y][x].p], col_normal);
          } else {
            fprintf(f, "%c", piece_figs[b->b[z][y][x].s][b->b[z][y][x].p]);
          }
        } else {
          fprintf(f, " ");
        }
        if (!((x+y)%2)) fprintf(f, "%s", col_normal);
      }
      fprintf(f, "%s|%s ", border_col[z], col_normal);
    }
    fprintf(f, "\n");
  }
  fprintf(f, " %sB------------B%s  %sM------------M%s  %sT------------T%s\n",
    border_col[0], col_normal, border_col[1], col_normal, border_col[2], col_normal);
  fprintf(f, "  %sabcdefghijkl%s    %sabcdefghijkl%s    %sabcdefghijkl%s\n",
    border_col[0], col_normal, border_col[1], col_normal, border_col[2], col_normal);
  fprintf(f, "*%s to move*\n", b->to_move==white ? "White" : "Black");
  return 0;
}

int dump_board(FILE* f, const board* b) {
  int i, j;
  for (i=1; i<max_piece; ++i) {
    for (j=0; b->pieces[white-1][i][j].valid||b->pieces[black-1][i][j].valid; ++j) {
      fprintf(f, "b->pieces[white][%i=%c][%i]={x=%i;y=%i;z=%i;valid=%i;frozen=%i;captured=%i}\n",
              i, piece_figs[white][i], j,
              b->pieces[white-1][i][j].x, b->pieces[white-1][i][j].y, b->pieces[white-1][i][j].z,
              b->pieces[white-1][i][j].valid, b->pieces[white-1][i][j].frozen, b->pieces[white-1][i][j].captured);
      fprintf(f, "b->pieces[black][%i=%c][%i]={x=%i;y=%i;z=%i;valid=%i;frozen=%i;captured=%i}\n",
              i, piece_figs[black][i], j,
              b->pieces[black-1][i][j].x, b->pieces[black-1][i][j].y, b->pieces[black-1][i][j].z,
              b->pieces[black-1][i][j].valid, b->pieces[black-1][i][j].frozen, b->pieces[black-1][i][j].captured);
    }
  }
  return 0;
}

int show_move(FILE* f, movet m) {
  fprintf(f, "%c%c%c%c%c%c%c",
    (m.f_z==0 ? 'B' : m.f_z==1 ? 'M' : m.f_z==2 ? 'T' : '?'),
    'a'+m.f_x, '1'+m.f_y,
    m.flags&f_cap ? (m.flags&f_farcap ? '*' : 'x') : '-',
    (m.t_z==0 ? 'B' : m.t_z==1 ? 'M' : m.t_z==2 ? 'T' : '?'),
    'a'+m.t_x, '1'+m.t_y
  );
  return 0;
}
int show_move_b(FILE* f, const board* b, movet m) {
  fprintf(f, "%c:%s%c%c%c%s%c%s%c%c%c%s",
    piece_figs[b->to_move][b->b[m.f_z][m.f_y][m.f_x].p],
    border_col[m.f_z], (m.f_z==0 ? 'B' : m.f_z==1 ? 'M' : m.f_z==2 ? 'T' : '?'),
    'a'+m.f_x, '1'+m.f_y, col_normal,
    m.flags&f_cap ? (m.flags&f_farcap ? '*' : 'x') : '-',
    border_col[m.t_z], (m.t_z==0 ? 'B' : m.t_z==1 ? 'M' : m.t_z==2 ? 'T' : '?'),
    'a'+m.t_x, '1'+m.t_y, col_normal
  );
  return 0;
}

int show_evaluator(FILE* f, const evaluator* e) {
  fprintf(f, "[[");
  if (e->f_material) fprintf(f, "Mat"); else fprintf(f, "---");
  if (e->f_mobility) fprintf(f, "Mob"); else fprintf(f, "---");
  if (e->f_control) fprintf(f, "Con"); else fprintf(f, "---");
  if (e->f_king_tropism) fprintf(f, "KTr"); else fprintf(f, "---");
  fprintf(f, "]]");
  return 0;
}

int validate_board(const board* b) {
  int x, y, z;
  int ok = 1;
  for (z=0; z<3; ++z) {
    for (y=0; y<8; ++y) {
      for (x=0; x<12; ++x) {
        square q = b->b[z][y][x];
        if (q.s) {
          if (b->pieces[q.s-1][q.p][q.idx].x != x) {
            printf("[%i]validate_board(): %c%c%c q.s=%i q.p=%i=%c q.idx=%i b->pieces[q.s-1][q.p][q.idx].x=%i != x=%i\n",
                   __LINE__,
                   (z==0 ? 'B' : z==1 ? 'M' : z==2 ? 'T' : '?'), 'a'+x, '1'+y,
                   q.s, q.p, piece_figs[q.s][q.p], q.idx, b->pieces[q.s-1][q.p][q.idx].x, x);
            ok = 0;
          }
          if (b->pieces[q.s-1][q.p][q.idx].y != y) {
            printf("[%i]validate_board(): <%i,%i,%i> q.s=%i q.p=%i=%c q.idx=%i b->pieces[q.s-1][q.p][q.idx].y=%i != y=%i\n",
                   __LINE__,
                   (z==0 ? 'B' : z==1 ? 'M' : z==2 ? 'T' : '?'), 'a'+x, '1'+y,
                   q.s, q.p, piece_figs[q.s][q.p], q.idx, b->pieces[q.s-1][q.p][q.idx].y, y);
            ok = 0;
          }
          if (b->pieces[q.s-1][q.p][q.idx].z != z) {
            printf("[%i]validate_board(): <%i,%i,%i> q.s=%i q.p=%i=%c q.idx=%i b->pieces[q.s-1][q.p][q.idx].z=%i != z=%i\n",
                   __LINE__,
                   (z==0 ? 'B' : z==1 ? 'M' : z==2 ? 'T' : '?'), 'a'+x, '1'+y,
                   q.s, q.p, piece_figs[q.s][q.p], q.idx, b->pieces[q.s-1][q.p][q.idx].z, z);
            ok = 0;
          }
        }
        if (q.p==basilisk && z==0 && b->b[1][y][x].s==(q.s^sidemask)) {
          square q2 = b->b[1][y][x];
          if (q2.s && b->pieces[q2.s-1][q2.p][q2.idx].frozen==0) {
            printf("@@@(%i) %c%c%c (over basilisk) not frozen but should be\n", __LINE__,
                   (z==0 ? 'B' : z==1 ? 'M' : z==2 ? 'T' : '?'), 'a'+x, '1'+y);
            ok = 0;
          }
        }
        if (q.s && z==1 && b->b[0][y][x].s==(q.s^sidemask)
            && b->b[0][y][x].p==basilisk) {
          if (b->pieces[q.s-1][q.p][q.idx].frozen==0) {
            printf("@@@(%i) %c%c%c (basilisk under) not frozen but should be\n", __LINE__,
                   (z==0 ? 'B' : z==1 ? 'M' : z==2 ? 'T' : '?'), 'a'+x, '1'+y);
            ok = 0;
          }
        } else {
          if (b->pieces[q.s-1][q.p][q.idx].frozen==1) {
            printf("@@@(%i) %c%c%c f\n", __LINE__,
                  (z==0 ? 'B' : z==1 ? 'M' : z==2 ? 'T' : '?'), 'a'+x, '1'+y);
            ok = 0;
          }
        }
      }
    }
  }
  return ok;
}

/* ************************************************************ */

static pos put_piece(board* b, int x, int y, int z, side s, piece p, int idx) {
  b->b[z][y][x].s = s;
  b->b[z][y][x].p = p;
  b->b[z][y][x].idx = idx;
  pos l;
  l.x = x;
  l.y = y;
  l.z = z;
  l.valid = 1;
  l.frozen = 0;
  l.captured = 0;
  return l;
}

int setup_board(board* b) {
  int i;
  memset(b, '\x00', sizeof(board));
  b->to_move = white;
  for (i=0; i<6; ++i) b->pieces[0][dwarf][i] = put_piece(b, 2*i+1, 1, 0, white, dwarf, i);
  b->pieces[0][elemental][0] = put_piece(b, 6, 0, 0, white, elemental, 0);
  b->pieces[0][basilisk][0] = put_piece(b, 2, 0, 0, white, basilisk, 0);
  b->pieces[0][basilisk][1] = put_piece(b, 10, 0, 0, white, basilisk, 1);

  for (i=0; i<12; ++i) b->pieces[0][warrior][i] = put_piece(b, i, 1, 1, white, warrior, i);
  b->pieces[0][oliphant][0] = put_piece(b, 0, 0, 1, white, oliphant, 0);
  b->pieces[0][unicorn][0] = put_piece(b, 1, 0, 1, white, unicorn, 0);
  b->pieces[0][hero][0] = put_piece(b, 2, 0, 1, white, hero, 0);
  b->pieces[0][thief][0] = put_piece(b, 3, 0, 1, white, thief, 0);
  b->pieces[0][cleric][0] = put_piece(b, 4, 0, 1, white, cleric, 0);
  b->pieces[0][mage][0] = put_piece(b, 5, 0, 1, white, mage, 0);
  b->pieces[0][king][0] = put_piece(b, 6, 0, 1, white, king, 0);
  b->pieces[0][paladin][0] = put_piece(b, 7, 0, 1, white, paladin, 0);
  b->pieces[0][thief][1] = put_piece(b, 8, 0, 1, white, thief, 1);
  b->pieces[0][hero][1] = put_piece(b, 9, 0, 1, white, hero, 1);
  b->pieces[0][unicorn][1] = put_piece(b, 10, 0, 1, white, unicorn, 1);
  b->pieces[0][oliphant][1] = put_piece(b, 11, 0, 1, white, oliphant, 1);

  for (i=0; i<6; ++i) b->pieces[0][sylph][i] = put_piece(b, 2*i, 1, 2, white, sylph, i);
  b->pieces[0][dragon][0] = put_piece(b, 6, 0, 2, white, dragon, 0);
  b->pieces[0][griffin][0] = put_piece(b, 2, 0, 2, white, griffin, 0);
  b->pieces[0][griffin][1] = put_piece(b, 10, 0, 2, white, griffin, 1);


  for (i=0; i<6; ++i) b->pieces[1][dwarf][i] = put_piece(b, 2*i+1, 6, 0, black, dwarf, i);
  b->pieces[1][elemental][0] = put_piece(b, 6, 7, 0, black, elemental, 0);
  b->pieces[1][basilisk][0] = put_piece(b, 2, 7, 0, black, basilisk, 0);
  b->pieces[1][basilisk][1] = put_piece(b, 10, 7, 0, black, basilisk, 1);

  for (i=0; i<12; ++i) b->pieces[1][warrior][i] = put_piece(b, i, 6, 1, black, warrior, i);
  b->pieces[1][oliphant][0] = put_piece(b, 0, 7, 1, black, oliphant, 0);
  b->pieces[1][unicorn][0] = put_piece(b, 1, 7, 1, black, unicorn, 0);
  b->pieces[1][hero][0] = put_piece(b, 2, 7, 1, black, hero, 0);
  b->pieces[1][thief][0] = put_piece(b, 3, 7, 1, black, thief, 0);
  b->pieces[1][cleric][0] = put_piece(b, 4, 7, 1, black, cleric, 0);
  b->pieces[1][mage][0] = put_piece(b, 5, 7, 1, black, mage, 0);
  b->pieces[1][king][0] = put_piece(b, 6, 7, 1, black, king, 0);
  b->pieces[1][paladin][0] = put_piece(b, 7, 7, 1, black, paladin, 0);
  b->pieces[1][thief][1] = put_piece(b, 8, 7, 1, black, thief, 1);
  b->pieces[1][hero][1] = put_piece(b, 9, 7, 1, black, hero, 1);
  b->pieces[1][unicorn][1] = put_piece(b, 10, 7, 1, black, unicorn, 1);
  b->pieces[1][oliphant][1] = put_piece(b, 11, 7, 1, black, oliphant, 1);

  for (i=0; i<6; ++i) b->pieces[1][sylph][i] = put_piece(b, 2*i, 6, 2, black, sylph, i);
  b->pieces[1][dragon][0] = put_piece(b, 6, 7, 2, black, dragon, 0);
  b->pieces[1][griffin][0] = put_piece(b, 2, 7, 2, black, griffin, 0);
  b->pieces[1][griffin][1] = put_piece(b, 10, 7, 2, black, griffin, 1);

  b->hash_stack[0] = hash_board(b);

  return 0;
}

movet null_move() {
  movet m;
  m.i = 0;
  m.flags = f_null;
  return m;
}

int make_move(board* b, movet m) {
  if (m.flags & f_null) {
    b->to_move ^= sidemask;
    b->hash_stack[b->ply+1] = b->hash_stack[b->ply] ^ hash_to_move[0] ^ hash_to_move[1];
    b->move_stack[b->ply] = m;
    ++(b->ply);
#ifdef VALIDATE_MOVE
    if (!validate_board(b)) printf("  --> make_move(null)\n");
#endif
    return 0;
  }
  side s = b->to_move, o = (b->to_move ^ sidemask);
  piece f_p = b->b[m.f_z][m.f_y][m.f_x].p;
  int f_i = b->b[m.f_z][m.f_y][m.f_x].idx;
#ifdef VALIDATE_MOVE
  if (!f_p) printf("**Invalid move piece**\n");
#endif
  if (f_p == basilisk) {
    /* unfreeze */
    if (b->b[m.f_z+1][m.f_y][m.f_x].s==o)
      b->pieces[o-1][b->b[m.f_z+1][m.f_y][m.f_x].p][b->b[m.f_z+1][m.f_y][m.f_x].idx].frozen = 0;
    /* freeze */
    if (b->b[m.t_z+1][m.t_y][m.t_x].s==o)
      b->pieces[o-1][b->b[m.t_z+1][m.t_y][m.t_x].p][b->b[m.t_z+1][m.t_y][m.t_x].idx].frozen = 1;
  }
  if (m.flags & f_cap) {
    piece x_p = m.cap_piece;
    int x_i = m.cap_idx;
    b->pieces[o-1][x_p][x_i].captured = 1;
    /* unfreeze if basilisk captured */
    if (x_p == basilisk)
      if (b->b[m.t_z+1][m.t_y][m.t_x].s==s)
        b->pieces[s-1][b->b[m.t_z+1][m.t_y][m.t_x].p][b->b[m.t_z+1][m.t_y][m.t_x].idx].frozen = 0;
    if (m.flags & f_farcap) {
      b->b[m.t_z][m.t_y][m.t_x].i = 0;
    } else {
      b->pieces[s-1][f_p][f_i].x = m.t_x;
      b->pieces[s-1][f_p][f_i].y = m.t_y;
      b->pieces[s-1][f_p][f_i].z = m.t_z;
      b->b[m.t_z][m.t_y][m.t_x] = b->b[m.f_z][m.f_y][m.f_x];
      b->b[m.f_z][m.f_y][m.f_x].i = 0;
    }
  } else {
    b->pieces[s-1][f_p][f_i].x = m.t_x;
    b->pieces[s-1][f_p][f_i].y = m.t_y;
    b->pieces[s-1][f_p][f_i].z = m.t_z;
    b->b[m.t_z][m.t_y][m.t_x] = b->b[m.f_z][m.f_y][m.f_x];
    b->b[m.f_z][m.f_y][m.f_x].i = 0;
  }
  /* freeze if over basilisk */
  if (!(m.flags&f_farcap) && (m.t_z==1) && (b->b[0][m.t_y][m.t_x].p==basilisk) && (b->b[0][m.t_y][m.t_x].s==o))
    b->pieces[s-1][f_p][f_i].frozen = 1;
  b->to_move ^= sidemask;

  b->move_stack[b->ply] = m;
  b->hash_stack[++(b->ply)] = hash_board(b);
#ifdef VALIDATE_MOVE
  if (!validate_board(b)) { printf("  --> make_move("); show_move_b(stdout, b, m); printf(")\n"); }
#endif
  return 0;
}

int unmake_move(board* b) {
  movet m = b->move_stack[b->ply-1];
  if (m.flags&f_null) {
    b->to_move ^= sidemask;
    --(b->ply);
#ifdef VALIDATE_MOVE
    if (!validate_board(b)) printf("  --> unmake_move(null)\n");
#endif
    return 0;
  }
  side s = (b->to_move ^ sidemask), o = b->to_move;
  piece t_p = b->b[m.t_z][m.t_y][m.t_x].p;
  int t_i = b->b[m.t_z][m.t_y][m.t_x].idx;
  if (t_p == basilisk) {
    /* unfreeze */
    if (b->b[m.t_z+1][m.t_y][m.t_x].s==o) {
      b->pieces[o-1][b->b[m.t_z+1][m.t_y][m.t_x].p][b->b[m.t_z+1][m.t_y][m.t_x].idx].frozen = 0;
      //printf("uf!%c%c%c", (m.t_z+1==0 ? 'B' : m.t_z+1==1 ? 'M' : m.t_z+1==2 ? 'T' : '?'), 'a'+m.t_x, '1'+m.t_y);
    }
    /* re-freeze */
    if (b->b[m.f_z+1][m.f_y][m.f_x].s==o) {
      b->pieces[o-1][b->b[m.f_z+1][m.f_y][m.f_x].p][b->b[m.f_z+1][m.f_y][m.f_x].idx].frozen = 1;
      //printf("FR!%c%c%c", (m.f_z+1==0 ? 'B' : m.f_z+1==1 ? 'M' : m.f_z+1==2 ? 'T' : '?'), 'a'+m.f_x, '1'+m.f_y);
    }
  }
  if (m.flags & f_cap) {
    piece x_p = m.cap_piece;
    int x_i = m.cap_idx;
    b->pieces[o-1][x_p][x_i].captured = 0;
    if (m.flags & f_farcap) {
    } else {
      b->pieces[s-1][t_p][t_i].x = m.f_x;
      b->pieces[s-1][t_p][t_i].y = m.f_y;
      b->pieces[s-1][t_p][t_i].z = m.f_z;
      b->b[m.f_z][m.f_y][m.f_x] = b->b[m.t_z][m.t_y][m.t_x];
    }
    b->b[m.t_z][m.t_y][m.t_x].p = x_p;
    b->b[m.t_z][m.t_y][m.t_x].s = o;
    b->b[m.t_z][m.t_y][m.t_x].idx = x_i;
    if (x_p==basilisk) {
      if (b->b[m.t_z+1][m.t_y][m.t_x].s==s) {
        b->pieces[s-1][b->b[m.t_z+1][m.t_y][m.t_x].p][b->b[m.t_z+1][m.t_y][m.t_x].idx].frozen = 1;
        //printf("Fx:%c%c%c", (m.t_z+1==0 ? 'B' : m.t_z+1==1 ? 'M' : m.t_z+1==2 ? 'T' : '?'), 'a'+m.t_x, '1'+m.t_y);
      }
    }
  } else {
    b->pieces[s-1][t_p][t_i].x = m.f_x;
    b->pieces[s-1][t_p][t_i].y = m.f_y;
    b->pieces[s-1][t_p][t_i].z = m.f_z;
    b->b[m.f_z][m.f_y][m.f_x] = b->b[m.t_z][m.t_y][m.t_x];
    b->b[m.t_z][m.t_y][m.t_x].i = 0;
  }
  // re-freeze or unfreeze
  if (m.f_z==1 && b->b[0][m.f_y][m.f_x].p==basilisk && b->b[0][m.f_y][m.f_x].s==o)
    b->pieces[s-1][t_p][t_i].frozen = 1;
  else
    b->pieces[s-1][t_p][t_i].frozen = 0;
  b->to_move ^= sidemask;
  --(b->ply);
#ifdef VALIDATE_MOVE
  if (!validate_board(b)) {
    printf("  --> unmake_move("); show_move_b(stdout, b, m); printf(")\n");
    show_board(stdout, b); printf("\n");
  }
#endif
  return 0;
}

static int only_captures = 0;
static void add_move(const board* b, piece p, pos l, int x, int y, int z,
                     movet* ms, int* n, int flags) {
  if (only_captures) return;
  movet m;
  if (x<0 || y<0 || z<0 || x>=12 || y>=8 || z>=3) return;
  if (b->b[z][y][x].s != 0) return;
  m.f_x = l.x;
  m.f_y = l.y;
  m.f_z = l.z;
  m.t_x = x;
  m.t_y = y;
  m.t_z = z;
  m.cap_piece = 0;
  m.cap_idx = 0;
  m.flags = flags;
  ms[(*n)++] = m;
}
static void add_cap(const board* b, piece p, pos l, int x, int y, int z,
                    movet* ms, int* n, int flags) {
  movet m;
  if (x<0 || y<0 || z<0 || x>=12 || y>=8 || z>=3) return;
  if (b->b[z][y][x].s != (b->to_move ^ sidemask)) return;
  m.f_x = l.x;
  m.f_y = l.y;
  m.f_z = l.z;
  m.t_x = x;
  m.t_y = y;
  m.t_z = z;
  m.cap_piece = b->b[z][y][x].p;
  m.cap_idx = b->b[z][y][x].idx;
  m.flags = flags | f_cap;
  ms[(*n)++] = m;
}
static void add_farcap(const board* b, piece p, pos l, int x, int y, int z,
                       movet* ms, int* n, int flags) {
  movet m;
  if (x<0 || y<0 || z<0 || x>=12 || y>=8 || z>=3) return;
  if (b->b[z][y][x].s != (b->to_move ^ sidemask)) return;
  m.f_x = l.x;
  m.f_y = l.y;
  m.f_z = l.z;
  m.t_x = x;
  m.t_y = y;
  m.t_z = z;
  m.cap_piece = b->b[z][y][x].p;
  m.cap_idx = b->b[z][y][x].idx;
  m.flags = flags | f_cap | f_farcap;
  ms[(*n)++] = m;
}
static void add_move_cap(const board* b, piece p, pos l, int x, int y, int z,
                         movet* ms, int* n, int flags) {
  if (only_captures && !b->b[z][y][x].s) return;
  movet m;
  if (x<0 || y<0 || z<0 || x>=12 || y>=8 || z>=3) return;
  if (b->b[z][y][x].s == b->to_move) return;
  m.f_x = l.x;
  m.f_y = l.y;
  m.f_z = l.z;
  m.t_x = x;
  m.t_y = y;
  m.t_z = z;
  m.cap_piece = b->b[z][y][x].s ? b->b[z][y][x].p : 0;
  m.cap_idx = b->b[z][y][x].s ? b->b[z][y][x].idx : 0;
  m.flags = flags | (b->b[z][y][x].s ? f_cap : 0);
  ms[(*n)++] = m;
}

static int sylph_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  side s = b->to_move;
  for (; (l=b->pieces[b->to_move-1][sylph][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][sylph][i].frozen) continue;
    if (b->pieces[b->to_move-1][sylph][i].captured) continue;
    if (l.z==2) {
      if (s==white) {
        add_move(b, sylph, l, l.x+1, l.y+1, 2, ms, &n, 0);
        add_move(b, sylph, l, l.x-1, l.y+1, 2, ms, &n, 0);
        add_cap (b, sylph, l, l.x  , l.y+1, 2, ms, &n, 0);
      } else {
        add_move(b, sylph, l, l.x+1, l.y-1, 2, ms, &n, 0);
        add_move(b, sylph, l, l.x-1, l.y-1, 2, ms, &n, 0);
        add_cap (b, sylph, l, l.x  , l.y-1, 2, ms, &n, 0);
      }
       add_cap (b, sylph, l, l.x  , l.y  , 1, ms, &n, 0);
     } else {
      add_move(b, sylph, l, l.x, l.y, 2, ms, &n, 0);
      if (s==white) {
        add_move(b, sylph, l,  0, 1, 2, ms, &n, 0);
        add_move(b, sylph, l,  2, 1, 2, ms, &n, 0);
        add_move(b, sylph, l,  4, 1, 2, ms, &n, 0);
        add_move(b, sylph, l,  6, 1, 2, ms, &n, 0);
        add_move(b, sylph, l,  8, 1, 2, ms, &n, 0);
        add_move(b, sylph, l, 10, 1, 2, ms, &n, 0);
      } else {
        add_move(b, sylph, l,  0, 6, 2, ms, &n, 0);
        add_move(b, sylph, l,  2, 6, 2, ms, &n, 0);
        add_move(b, sylph, l,  4, 6, 2, ms, &n, 0);
        add_move(b, sylph, l,  6, 6, 2, ms, &n, 0);
        add_move(b, sylph, l,  8, 6, 2, ms, &n, 0);
        add_move(b, sylph, l, 10, 6, 2, ms, &n, 0);
      }
    }
  }
  return n;
}
static int griffin_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  for (; (l=b->pieces[b->to_move-1][griffin][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][griffin][i].frozen) continue;
    if (b->pieces[b->to_move-1][griffin][i].captured) continue;
    if (l.z==2) {
      add_move_cap(b, griffin, l, l.x+3, l.y+2, 2, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x+3, l.y-2, 2, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x-3, l.y+2, 2, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x-3, l.y-2, 2, ms, &n, 0);

      add_move_cap(b, griffin, l, l.x+2, l.y+3, 2, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x+2, l.y-3, 2, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x-2, l.y+3, 2, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x-2, l.y-3, 2, ms, &n, 0);

      add_move_cap(b, griffin, l, l.x+1, l.y+1, 1, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x+1, l.y-1, 1, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x-1, l.y+1, 1, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x-1, l.y-1, 1, ms, &n, 0);
    } else {
      add_move_cap(b, griffin, l, l.x+1, l.y+1, 1, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x+1, l.y-1, 1, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x-1, l.y+1, 1, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x-1, l.y-1, 1, ms, &n, 0);

      add_move_cap(b, griffin, l, l.x+1, l.y+1, 2, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x+1, l.y-1, 2, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x-1, l.y+1, 2, ms, &n, 0);
      add_move_cap(b, griffin, l, l.x-1, l.y-1, 2, ms, &n, 0);
    }
  }
  return n;
}
static int dragon_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  side s = b->to_move, o = s^sidemask;
  for (; (l=b->pieces[b->to_move-1][dragon][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][dragon][i].frozen) continue;
    if (b->pieces[b->to_move-1][dragon][i].captured) continue;
    add_move_cap(b, dragon, l, l.x+1, l.y  , 2, ms, &n, 0);
    add_move_cap(b, dragon, l, l.x  , l.y+1, 2, ms, &n, 0);
    add_move_cap(b, dragon, l, l.x-1, l.y  , 2, ms, &n, 0);
    add_move_cap(b, dragon, l, l.x  , l.y-1, 2, ms, &n, 0);

    add_farcap(b, dragon, l, l.x  , l.y  , 1, ms, &n, 0);
    add_farcap(b, dragon, l, l.x+1, l.y  , 1, ms, &n, 0);
    add_farcap(b, dragon, l, l.x  , l.y+1, 1, ms, &n, 0);
    add_farcap(b, dragon, l, l.x-1, l.y  , 1, ms, &n, 0);
    add_farcap(b, dragon, l, l.x  , l.y-1, 1, ms, &n, 0);

    int jx, jy;
    for (jx=l.x+1, jy=l.y+1; jx<12 && jy<8; ++jx, ++jy)
      if (b->b[l.z][jy][jx].s==0)
        add_move(b, dragon, l, jx, jy, l.z, ms, &n, 0);
      else break;
    if (jx<12 && jy<8 && b->b[l.z][jy][jx].s==o)
      add_cap(b, dragon, l, jx, jy, l.z, ms, &n, 0);

    for (jx=l.x+1, jy=l.y-1; jx<12 && jy>=0; ++jx, --jy)
      if (b->b[l.z][jy][jx].s==0)
        add_move(b, dragon, l, jx, jy, l.z, ms, &n, 0);
      else break;
    if (jx<12 && jy>=0 && b->b[l.z][jy][jx].s==o)
      add_cap(b, dragon, l, jx, jy, l.z, ms, &n, 0);

    for (jx=l.x-1, jy=l.y+1; jx>=0 && jy<8; --jx, ++jy)
      if (b->b[l.z][jy][jx].s==0)
        add_move(b, dragon, l, jx, jy, l.z, ms, &n, 0);
      else break;
    if (jx>=0 && jy<8 && b->b[l.z][jy][jx].s==o)
      add_cap(b, dragon, l, jx, jy, l.z, ms, &n, 0);

    for (jx=l.x-1, jy=l.y-1; jx>=0 && jy>=0; --jx, --jy)
      if (b->b[l.z][jy][jx].s==0)
        add_move(b, dragon, l, jx, jy, l.z, ms, &n, 0);
      else break;
    if (jx>=0 && jy>=0 && b->b[l.z][jy][jx].s==o)
      add_cap(b, dragon, l, jx, jy, l.z, ms, &n, 0);
  }
  return n;
}
static int oliphant_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  side s = b->to_move, o = s^sidemask;
  for (; (l=b->pieces[b->to_move-1][oliphant][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][oliphant][i].frozen) continue;
    if (b->pieces[b->to_move-1][oliphant][i].captured) continue;
    int j;
    for (j=l.x+1; j<12; ++j)
      if (b->b[l.z][l.y][j].s==0)
        add_move(b, oliphant, l, j, l.y, l.z, ms, &n, 0);
      else break;
    if (j<12 && b->b[l.z][l.y][j].s==o)
      add_cap(b, oliphant, l, j, l.y, l.z, ms, &n, 0);
    for (j=l.x-1; j>=0; --j)
      if (b->b[l.z][l.y][j].s==0)
        add_move(b, oliphant, l, j, l.y, l.z, ms, &n, 0);
      else break;
    if (j>=0 && b->b[l.z][l.y][j].s==o)
      add_cap(b, oliphant, l, j, l.y, l.z, ms, &n, 0);
    for (j=l.y+1; j<8; ++j)
      if (b->b[l.z][j][l.x].s==0)
        add_move(b, oliphant, l, l.x, j, l.z, ms, &n, 0);
      else break;
    if (j<8 && b->b[l.z][j][l.x].s==o)
      add_cap(b, oliphant, l, l.x, j, l.z, ms, &n, 0);
    for (j=l.y-1; j>=0; --j)
      if (b->b[l.z][j][l.x].s==0)
        add_move(b, oliphant, l, l.x, j, l.z, ms, &n, 0);
      else break;
    if (j>=0 && b->b[l.z][j][l.x].s==o)
      add_cap(b, oliphant, l, l.x, j, l.z, ms, &n, 0);
  }
  return n;
}
static int unicorn_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  for (; (l=b->pieces[b->to_move-1][unicorn][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][unicorn][i].frozen) continue;
    if (b->pieces[b->to_move-1][unicorn][i].captured) continue;
    add_move_cap(b, unicorn, l, l.x+2, l.y+1, l.z, ms, &n, 0);
    add_move_cap(b, unicorn, l, l.x+2, l.y-1, l.z, ms, &n, 0);
    add_move_cap(b, unicorn, l, l.x-2, l.y+1, l.z, ms, &n, 0);
    add_move_cap(b, unicorn, l, l.x-2, l.y-1, l.z, ms, &n, 0);

    add_move_cap(b, unicorn, l, l.x+1, l.y+2, l.z, ms, &n, 0);
    add_move_cap(b, unicorn, l, l.x-1, l.y+2, l.z, ms, &n, 0);
    add_move_cap(b, unicorn, l, l.x+1, l.y-2, l.z, ms, &n, 0);
    add_move_cap(b, unicorn, l, l.x-1, l.y-2, l.z, ms, &n, 0);
  }
  return n;
}
static int hero_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  for (; (l=b->pieces[b->to_move-1][hero][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][hero][i].frozen) continue;
    if (b->pieces[b->to_move-1][hero][i].captured) continue;
    if (l.z==1) {
      add_move_cap(b, hero, l, l.x+1, l.y  , 0, ms, &n, 0);
      add_move_cap(b, hero, l, l.x  , l.y+1, 0, ms, &n, 0);
      add_move_cap(b, hero, l, l.x-1, l.y  , 0, ms, &n, 0);
      add_move_cap(b, hero, l, l.x  , l.y-1, 0, ms, &n, 0);

      add_move_cap(b, hero, l, l.x+1, l.y  , 1, ms, &n, 0);
      add_move_cap(b, hero, l, l.x  , l.y+1, 1, ms, &n, 0);
      add_move_cap(b, hero, l, l.x-1, l.y  , 1, ms, &n, 0);
      add_move_cap(b, hero, l, l.x  , l.y-1, 1, ms, &n, 0);

      add_move_cap(b, hero, l, l.x+2, l.y  , 1, ms, &n, 0);
      add_move_cap(b, hero, l, l.x  , l.y+2, 1, ms, &n, 0);
      add_move_cap(b, hero, l, l.x-2, l.y  , 1, ms, &n, 0);
      add_move_cap(b, hero, l, l.x  , l.y-2, 1, ms, &n, 0);

      add_move_cap(b, hero, l, l.x+1, l.y  , 2, ms, &n, 0);
      add_move_cap(b, hero, l, l.x  , l.y+1, 2, ms, &n, 0);
      add_move_cap(b, hero, l, l.x-1, l.y  , 2, ms, &n, 0);
      add_move_cap(b, hero, l, l.x  , l.y-1, 2, ms, &n, 0);
    } else {
      add_move_cap(b, hero, l, l.x+1, l.y  , 1, ms, &n, 0);
      add_move_cap(b, hero, l, l.x  , l.y+1, 1, ms, &n, 0);
      add_move_cap(b, hero, l, l.x-1, l.y  , 1, ms, &n, 0);
      add_move_cap(b, hero, l, l.x  , l.y-1, 1, ms, &n, 0);
    }
  }
  return n;
}
static int thief_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  side s = b->to_move, o = s^sidemask;
  for (; (l=b->pieces[b->to_move-1][thief][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][thief][i].frozen) continue;
    if (b->pieces[b->to_move-1][thief][i].captured) continue;
    int jx, jy;
    for (jx=l.x+1, jy=l.y+1; jx<12 && jy<8; ++jx, ++jy)
      if (b->b[l.z][jy][jx].s==0) add_move(b, thief, l, jx, jy, l.z, ms, &n, 0);
      else break;
    if (jx<12 && jy<8 && b->b[l.z][jy][jx].s==o)
      add_cap(b, thief, l, jx, jy, l.z, ms, &n, 0);

    for (jx=l.x+1, jy=l.y-1; jx<12 && jy>=0; ++jx, --jy)
      if (b->b[l.z][jy][jx].s==0) add_move(b, thief, l, jx, jy, l.z, ms, &n, 0);
      else break;
    if (jx<12 && jy>=0 && b->b[l.z][jy][jx].s==o)
        add_cap(b, thief, l, jx, jy, l.z, ms, &n, 0);

    for (jx=l.x-1, jy=l.y+1; jx>=0 && jy<8; --jx, ++jy)
      if (b->b[l.z][jy][jx].s==0) add_move(b, thief, l, jx, jy, l.z, ms, &n, 0);
      else break;
    if (jx>=0 && jy<8 && b->b[l.z][jy][jx].s==o)
    add_cap(b, thief, l, jx, jy, l.z, ms, &n, 0);

    for (jx=l.x-1, jy=l.y-1; jx>=0 && jy>=0; --jx, --jy)
      if (b->b[l.z][jy][jx].s==0) add_move(b, thief, l, jx, jy, l.z, ms, &n, 0);
      else break;
    if (jx>=0 && jy>=0 && b->b[l.z][jy][jx].s==o)
    add_cap(b, thief, l, jx, jy, l.z, ms, &n, 0);
  }
  return n;
}
static int cleric_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  for (; (l=b->pieces[b->to_move-1][cleric][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][cleric][i].frozen) continue;
    if (b->pieces[b->to_move-1][cleric][i].captured) continue;
    add_move_cap(b, cleric, l, l.x+1, l.y+1, l.z  , ms, &n, 0);
    add_move_cap(b, cleric, l, l.x+1, l.y  , l.z  , ms, &n, 0);
    add_move_cap(b, cleric, l, l.x+1, l.y-1, l.z  , ms, &n, 0);
    add_move_cap(b, cleric, l, l.x  , l.y+1, l.z  , ms, &n, 0);
    add_move_cap(b, cleric, l, l.x  , l.y  , l.z+1, ms, &n, 0);
    add_move_cap(b, cleric, l, l.x  , l.y  , l.z-1, ms, &n, 0);
    add_move_cap(b, cleric, l, l.x  , l.y-1, l.z  , ms, &n, 0);
    add_move_cap(b, cleric, l, l.x-1, l.y+1, l.z  , ms, &n, 0);
    add_move_cap(b, cleric, l, l.x-1, l.y  , l.z  , ms, &n, 0);
    add_move_cap(b, cleric, l, l.x-1, l.y-1, l.z  , ms, &n, 0);
  }
  return n;
}
static int mage_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  side s = b->to_move, o = s^sidemask;
  for (; (l=b->pieces[b->to_move-1][mage][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][mage][i].frozen) continue;
    if (b->pieces[b->to_move-1][mage][i].captured) continue;
    if (l.z==1) {
      int j;
      for (j=l.x+1; j<12; ++j)
        if (b->b[l.z][l.y][j].s==0)
          add_move(b, mage, l, j, l.y, l.z, ms, &n, 0);
        else break;
      if (j<12 && b->b[l.z][l.y][j].s==o)
        add_cap(b, mage, l, j, l.y, l.z, ms, &n, 0);

      for (j=l.x-1; j>=0; --j)
        if (b->b[l.z][l.y][j].s==0)
          add_move(b, mage, l, j, l.y, l.z, ms, &n, 0);
        else break;
      if (j>=0 && b->b[l.z][l.y][j].s==o)
        add_cap(b, mage, l, j, l.y, l.z, ms, &n, 0);

      for (j=l.y+1; j<8; ++j)
        if (b->b[l.z][j][l.x].s==0)
          add_move(b, mage, l, l.x, j, l.z, ms, &n, 0);
        else break;
      if (j<8 && b->b[l.z][j][l.x].s==o)
        add_cap(b, mage, l, l.x, j, l.z, ms, &n, 0);

      for (j=l.y-1; j>=0; --j)
        if (b->b[l.z][j][l.x].s==0)
          add_move(b, mage, l, l.x, j, l.z, ms, &n, 0);
        else break;
      if (j>=0 && b->b[l.z][j][l.x].s==o)
        add_cap(b, mage, l, l.x, j, l.z, ms, &n, 0);

      int jx, jy;
      for (jx=l.x+1, jy=l.y+1; jx<12 && jy<8; ++jx, ++jy)
        if (b->b[l.z][jy][jx].s==0)
          add_move(b, mage, l, jx, jy, l.z, ms, &n, 0);
        else break;
      if (jx<12 && jy<8 && b->b[l.z][jy][jx].s==o)
        add_cap(b, mage, l, jx, jy, l.z, ms, &n, 0);

      for (jx=l.x+1, jy=l.y-1; jx<12 && jy>=0; ++jx, --jy)
        if (b->b[l.z][jy][jx].s==0)
          add_move(b, mage, l, jx, jy, l.z, ms, &n, 0);
        else break;
      if (jx<12 && jy>=0 && b->b[l.z][jy][jx].s==o)
        add_cap(b, mage, l, jx, jy, l.z, ms, &n, 0);

      for (jx=l.x-1, jy=l.y+1; jx>=0 && jy<8; --jx, ++jy)
        if (b->b[l.z][jy][jx].s==0)
          add_move(b, mage, l, jx, jy, l.z, ms, &n, 0);
        else break;
      if (jx>=0 && jy<8 && b->b[l.z][jy][jx].s==o)
        add_cap(b, mage, l, jx, jy, l.z, ms, &n, 0);

      for (jx=l.x-1, jy=l.y-1; jx>=0 && jy>=0; --jx, --jy)
        if (b->b[l.z][jy][jx].s==0)
          add_move(b, mage, l, jx, jy, l.z, ms, &n, 0);
        else break;
      if (jx>=0 && jy>=0 && b->b[l.z][jy][jx].s==o)
        add_cap(b, mage, l, jx, jy, l.z, ms, &n, 0);

      add_move_cap(b, mage, l, l.x, l.y, l.z+1, ms, &n, 0);
      add_move_cap(b, mage, l, l.x, l.y, l.z-1, ms, &n, 0);
    } else if (l.z==2) {
      add_move_cap(b, mage, l, l.x+1, l.y  , 2, ms, &n, 0);
      add_move_cap(b, mage, l, l.x-1, l.y  , 2, ms, &n, 0);
      add_move_cap(b, mage, l, l.x  , l.y+1, 2, ms, &n, 0);
      add_move_cap(b, mage, l, l.x  , l.y-1, 2, ms, &n, 0);
      add_move_cap(b, mage, l, l.x  , l.y  , 1, ms, &n, 0);
      if (b->b[1][l.y][l.x].s==none)
        add_move_cap(b, mage, l, l.x  , l.y  , 0, ms, &n, 0);
    } else if (l.z==0) {
      add_move_cap(b, mage, l, l.x+1, l.y  , 0, ms, &n, 0);
      add_move_cap(b, mage, l, l.x-1, l.y  , 0, ms, &n, 0);
      add_move_cap(b, mage, l, l.x  , l.y+1, 0, ms, &n, 0);
      add_move_cap(b, mage, l, l.x  , l.y-1, 0, ms, &n, 0);
      add_move_cap(b, mage, l, l.x  , l.y  , 1, ms, &n, 0);
      if (b->b[1][l.y][l.x].s==none)
        add_move_cap(b, mage, l, l.x  , l.y  , 2, ms, &n, 0);
    }
  }
  return n;
}
static int king_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  for (; (l=b->pieces[b->to_move-1][king][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][king][i].frozen) continue;
    if (b->pieces[b->to_move-1][king][i].captured) continue;
    if (l.z==1) {
      add_move_cap(b, king, l, l.x+1, l.y+1, l.z  , ms, &n, 0);
      add_move_cap(b, king, l, l.x+1, l.y  , l.z  , ms, &n, 0);
      add_move_cap(b, king, l, l.x+1, l.y-1, l.z  , ms, &n, 0);
      add_move_cap(b, king, l, l.x  , l.y+1, l.z  , ms, &n, 0);
      add_move_cap(b, king, l, l.x  , l.y  , l.z+1, ms, &n, 0);
      add_move_cap(b, king, l, l.x  , l.y  , l.z-1, ms, &n, 0);
      add_move_cap(b, king, l, l.x  , l.y-1, l.z  , ms, &n, 0);
      add_move_cap(b, king, l, l.x-1, l.y+1, l.z  , ms, &n, 0);
      add_move_cap(b, king, l, l.x-1, l.y  , l.z  , ms, &n, 0);
      add_move_cap(b, king, l, l.x-1, l.y-1, l.z  , ms, &n, 0);
    } else {
      add_move_cap(b, king, l, l.x  , l.y  , l.z+1, ms, &n, 0);
      add_move_cap(b, king, l, l.x  , l.y  , l.z-1, ms, &n, 0);
    }
  }
  return n;
}
static int paladin_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  for (; (l=b->pieces[b->to_move-1][paladin][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][paladin][i].frozen) continue;
    if (b->pieces[b->to_move-1][paladin][i].captured) continue;
    /* king moves */
    add_move_cap(b, paladin, l, l.x+1, l.y  , l.z  , ms, &n, 0);
    add_move_cap(b, paladin, l, l.x-1, l.y  , l.z  , ms, &n, 0);
    add_move_cap(b, paladin, l, l.x  , l.y+1, l.z  , ms, &n, 0);
    add_move_cap(b, paladin, l, l.x  , l.y-1, l.z  , ms, &n, 0);
    add_move_cap(b, paladin, l, l.x+1, l.y+1, l.z  , ms, &n, 0);
    add_move_cap(b, paladin, l, l.x+1, l.y-1, l.z  , ms, &n, 0);
    add_move_cap(b, paladin, l, l.x-1, l.y+1, l.z  , ms, &n, 0);
    add_move_cap(b, paladin, l, l.x-1, l.y-1, l.z  , ms, &n, 0);
    if (l.z==1) {
      /* same-level knight jumps */
      add_move_cap(b, paladin, l, l.x+2, l.y+1, 1, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x+2, l.y-1, 1, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x-2, l.y+1, 1, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x-2, l.y-1, 1, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x+1, l.y+2, 1, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x+1, l.y-2, 1, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x-1, l.y+2, 1, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x-1, l.y-2, 1, ms, &n, 0);
      /* 1-level knight jumps */
      add_move_cap(b, paladin, l, l.x+2, l.y  , 2, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x  , l.y+2, 2, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x-2, l.y  , 2, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x  , l.y-2, 2, ms, &n, 0);
      /* 1-level knight jumps */
      add_move_cap(b, paladin, l, l.x+2, l.y  , 0, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x  , l.y+2, 0, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x-2, l.y  , 0, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x  , l.y-2, 0, ms, &n, 0);
    } else if (l.z==0) {
      /* 2-level knight jumps */
      add_move_cap(b, paladin, l, l.x+1, l.y  , 2, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x  , l.y+1, 2, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x-1, l.y  , 2, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x  , l.y-1, 2, ms, &n, 0);
      /* 1-level knight jumps */
      add_move_cap(b, paladin, l, l.x+2, l.y  , 1, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x  , l.y+2, 1, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x-2, l.y  , 1, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x  , l.y-2, 1, ms, &n, 0);
    } else if (l.z==2) {
      /* 2-level knight jumps */
      add_move_cap(b, paladin, l, l.x+1, l.y  , 0, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x  , l.y+1, 0, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x-1, l.y  , 0, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x  , l.y-1, 0, ms, &n, 0);
      /* 1-level knight jumps */
      add_move_cap(b, paladin, l, l.x+2, l.y  , 1, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x  , l.y+2, 1, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x-2, l.y  , 1, ms, &n, 0);
      add_move_cap(b, paladin, l, l.x  , l.y-2, 1, ms, &n, 0);
    }
  }
  return n;
}
static int warrior_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  side s = b->to_move;
  for (; (l=b->pieces[b->to_move-1][warrior][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][warrior][i].frozen) continue;
    if (b->pieces[b->to_move-1][warrior][i].captured) continue;
    if (s==white) {
      add_cap (b, warrior, l, l.x+1, l.y+1, l.z, ms, &n, l.y==6 ? f_pro : 0);
      add_cap (b, warrior, l, l.x-1, l.y+1, l.z, ms, &n, l.y==6 ? f_pro : 0);
      add_move(b, warrior, l, l.x  , l.y+1, l.z, ms, &n, l.y==6 ? f_pro : 0);
    } else {
      add_cap (b, warrior, l, l.x+1, l.y-1, l.z, ms, &n, l.y==1 ? f_pro : 0);
      add_cap (b, warrior, l, l.x-1, l.y-1, l.z, ms, &n, l.y==1 ? f_pro : 0);
      add_move(b, warrior, l, l.x  , l.y-1, l.z, ms, &n, l.y==1 ? f_pro : 0);
    }
  }
  return n;
}
static int basilisk_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  side s = b->to_move;
  for (; (l=b->pieces[b->to_move-1][basilisk][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][basilisk][i].frozen) continue;
    if (b->pieces[b->to_move-1][basilisk][i].captured) continue;
    if (s==white) {
      add_move_cap(b, basilisk, l, l.x-1, l.y+1, l.z, ms, &n, 0);
      add_move_cap(b, basilisk, l, l.x  , l.y+1, l.z, ms, &n, 0);
      add_move_cap(b, basilisk, l, l.x+1, l.y+1, l.z, ms, &n, 0);
      add_move(b, basilisk, l, l.x, l.y-1, l.z, ms, &n, 0);
    } else {
      add_move_cap(b, basilisk, l, l.x-1, l.y-1, l.z, ms, &n, 0);
      add_move_cap(b, basilisk, l, l.x  , l.y-1, l.z, ms, &n, 0);
      add_move_cap(b, basilisk, l, l.x+1, l.y-1, l.z, ms, &n, 0);
      add_move(b, basilisk, l, l.x, l.y+1, l.z, ms, &n, 0);
    }
  }
  return n;
}
static int elemental_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  for (; (l=b->pieces[b->to_move-1][elemental][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][elemental][i].frozen) continue;
    if (b->pieces[b->to_move-1][elemental][i].captured) continue;
    if (l.z==0) {
      add_move_cap(b, elemental, l, l.x+1, l.y  , 0, ms, &n, 0);
      add_move_cap(b, elemental, l, l.x-1, l.y  , 0, ms, &n, 0);
      add_move_cap(b, elemental, l, l.x  , l.y+1, 0, ms, &n, 0);
      add_move_cap(b, elemental, l, l.x  , l.y-1, 0, ms, &n, 0);
      if (l.x+2<12 && b->b[0][l.y  ][l.x+1].s==none)
        add_move_cap(b, elemental, l, l.x+2, l.y  , 0, ms, &n, 0);
      if (l.x-2>=0 && b->b[0][l.y  ][l.x-1].s==none)
        add_move_cap(b, elemental, l, l.x-2, l.y  , 0, ms, &n, 0);
      if (l.y+2<8 && b->b[0][l.y+1][l.x  ].s==none)
        add_move_cap(b, elemental, l, l.x  , l.y+2, 0, ms, &n, 0);
      if (l.y-2>=0 && b->b[0][l.y-1][l.x  ].s==none)
        add_move_cap(b, elemental, l, l.x  , l.y-2, 0, ms, &n, 0);
      add_move(b, elemental, l, l.x+1, l.y+1, 0, ms, &n, 0);
      add_move(b, elemental, l, l.x+1, l.y-1, 0, ms, &n, 0);
      add_move(b, elemental, l, l.x-1, l.y+1, 0, ms, &n, 0);
      add_move(b, elemental, l, l.x-1, l.y-1, 0, ms, &n, 0);
      if (b->b[1][l.y][l.x].s==none) {
        add_move_cap(b, elemental, l, l.x+1, l.y  , 1, ms, &n, 0);
        add_move_cap(b, elemental, l, l.x  , l.y+1, 1, ms, &n, 0);
        add_move_cap(b, elemental, l, l.x-1, l.y  , 1, ms, &n, 0);
        add_move_cap(b, elemental, l, l.x  , l.y-1, 1, ms, &n, 0);
      }
    } else {
      add_move_cap(b, elemental, l, l.x+1, l.y  , 0, ms, &n, 0);
      add_move_cap(b, elemental, l, l.x  , l.y+1, 0, ms, &n, 0);
      add_move_cap(b, elemental, l, l.x-1, l.y  , 0, ms, &n, 0);
      add_move_cap(b, elemental, l, l.x  , l.y-1, 0, ms, &n, 0);
    }
  }
  return n;
}
static int dwarf_moves(const board* b, movet* ms) {
  int n = 0, i = 0;
  pos l;
  side s = b->to_move;
  for (; (l=b->pieces[b->to_move-1][dwarf][i]).valid; ++i) {
    if (b->pieces[b->to_move-1][dwarf][i].frozen) continue;
    if (b->pieces[b->to_move-1][dwarf][i].captured) continue;
    if (l.z<=1) {
      add_cap(b, dwarf, l, l.x, l.y, l.z+1, ms, &n, 0);
      if (s==white) {
        add_cap (b, dwarf, l, l.x+1, l.y+1, l.z, ms, &n, 0);
        add_cap (b, dwarf, l, l.x-1, l.y+1, l.z, ms, &n, 0);
        add_move(b, dwarf, l, l.x+1, l.y  , l.z, ms, &n, 0);
        add_move(b, dwarf, l, l.x  , l.y+1, l.z, ms, &n, 0);
        add_move(b, dwarf, l, l.x-1, l.y  , l.z, ms, &n, 0);
      } else {
        add_cap (b, dwarf, l, l.x+1, l.y-1, l.z, ms, &n, 0);
        add_cap (b, dwarf, l, l.x-1, l.y-1, l.z, ms, &n, 0);
        add_move(b, dwarf, l, l.x+1, l.y  , l.z, ms, &n, 0);
        add_move(b, dwarf, l, l.x  , l.y-1, l.z, ms, &n, 0);
        add_move(b, dwarf, l, l.x-1, l.y  , l.z, ms, &n, 0);
      }
    }
    add_move(b, dwarf, l, l.x, l.y, l.z-1, ms, &n, 0);
  }
  return n;
}

static int all_moves(const board* b, movet* ms) {
  int n = 0;
  n += dragon_moves(b,ms+n);
  n += oliphant_moves(b,ms+n);
  n += unicorn_moves(b,ms+n);
  n += hero_moves(b,ms+n);
  n += thief_moves(b,ms+n);
  n += cleric_moves(b,ms+n);
  n += mage_moves(b,ms+n);
  n += king_moves(b,ms+n);
  n += griffin_moves(b,ms+n);
  n += paladin_moves(b,ms+n);
  n += basilisk_moves(b,ms+n);
  n += elemental_moves(b,ms+n);
  n += warrior_moves(b,ms+n);
  n += dwarf_moves(b,ms+n);
  n += sylph_moves(b,ms+n);
  return n;
}
int gen_moves(const board* b, movet* ms) {
  only_captures = 0;
  return all_moves(b, ms);
}
int gen_moves_cap(const board* b, movet* ms) {
  only_captures = 1;
  return all_moves(b, ms);
}

/* ************************************************************ */
void init_hash() {
  int s, p, x, y, z;
  for (s=0; s<2; ++s)
    for (p=0; p<max_piece-1; ++p)
      for (z=0; z<3; ++z)
        for (y=0; y<8; ++y)
          for (x=0; x<12; ++x)
            hash_piece[s][p][z][y][x] = gen_hash();
  hash_to_move[0] = gen_hash();
  hash_to_move[1] = gen_hash();
}
hasht hash_board(const board* b) {
  int z, y, x;
  hasht h = hash_to_move[b->to_move - 1];
  for (z=0; z<3; ++z)
    for (y=0; y<8; ++y)
      for (x=0; x<12; ++x)
        if (b->b[z][y][x].s)
          h ^= hash_piece[b->b[z][y][x].s-1][b->b[z][y][x].p-1][z][y][x];
  return h;
}

/* ************************************************************ */

int terminal(const board* b) {
  int i;
  if (b->pieces[white-1][king][0].captured
      || b->pieces[black-1][king][0].captured)
    return 1;

  const hasht h = b->hash_stack[b->ply];
  for (i=b->ply-1; i>=0; --i)
    if (b->hash_stack[i] == h)
      return 1;
  return 0;
}

// from chessvariants.org
static evalt piece_values[max_piece] = {
    0,
// sylph, griffin, dragon,
  100,  500,  800,
// oliphant, unicorn, hero, thief, cleric, mage, king, paladin, warrior,
  500,  250,  450,  400,  900, 1100, 99999, 1000,  100,
// basilisk, elemental, dwarf,
  300,  400,  200
};

int simple_evaluator(evaluator* e) {
  memset(e, '\x00', sizeof(evaluator));
  memcpy(e->piece_values, piece_values, sizeof(piece_values));
  e->f_mobility = 1;
  e->f_material = 1;
  e->f_control = 0;
  e->f_king_tropism = 0;
  return 0;
}

static evalt evaluate_material(const evaluator* e, const board* b) {
  evalt eval = 0;
  int i, j;
  for (i=1; i<max_piece; ++i)
    for (j=0; b->pieces[white-1][i][j].valid; ++j)
      if (!b->pieces[white-1][i][j].captured)
        eval += e->piece_values[i];
  for (i=1; i<max_piece; ++i)
    for (j=0; b->pieces[black-1][i][j].valid; ++j)
      if (!b->pieces[black-1][i][j].captured)
        eval -= e->piece_values[i];
  return eval;
}

static evalt evaluate_mobility(const evaluator* e, const board* b) {
  evalt eval = 0;
  char brd[3][8][12];
  movet  ms[256];
  int n, i;
  int mob_w=0, mob_b=0;

  n = all_moves(b, ms);
  for (i=0; i<n; ++i) {
    mob_w += (ms[i].t_z==1) ? 2 : 1;
    mob_w += (ms[i].flags&f_cap) ? 2 : 0;
  }

  if (e->f_control) {
    memset(brd, '\x00', sizeof(brd));
    for (i=0; i<n; ++i)
      brd[ms[i].t_z][ms[i].t_y][ms[i].t_x] |= white;
  }

  movet nm = null_move();
  make_move((board*)b, nm);
  n = all_moves(b, ms);
  for (i=0; i<n; ++i) {
    mob_b -= (ms[i].t_z==1) ? 2 : 1;
    mob_b -= (ms[i].flags&f_cap) ? 2 : 0;
  }
  unmake_move((board*)b);

  eval += (mob_w/4) - (mob_b/4);

  if (e->f_control) {
    for (i=0; i<n; ++i)
      brd[ms[i].t_z][ms[i].t_y][ms[i].t_x] |= black;

    int x, y, z;
    for (z=0; z<3; ++z) {
      for (y=0; y<8; ++y) {
        for (x=0; x<12; ++x) {
          if (brd[z][y][x]==white)
            eval += (z==1) ? 3 : 1;
          else if (brd[z][y][x]==black)
            eval += (z==1) ? 3 : 1;
        }
      }
    }
  }
  return eval;
}

static evalt evaluate_king_tropism(const evaluator* e, const board* b) {
  evalt eval = 0;
  int i, j;
  if (b->pieces[black-1][king][0].valid) {
    int kx = b->pieces[black-1][king][0].x;
    int ky = b->pieces[black-1][king][0].y;
    int kz = b->pieces[black-1][king][0].z;
    for (i=1; i<max_piece; ++i) {
      for (j=0; b->pieces[white-1][i][j].valid; ++j) {
        if (!b->pieces[white-1][i][j].captured) {
          int px = b->pieces[white-1][i][0].x;
          int py = b->pieces[white-1][i][0].y;
          int pz = b->pieces[white-1][i][0].z;
          eval -= (int)floor(sqrt(abs(kx-px) + abs(ky-py) + abs(kz-pz))*10.0);
        }
      }
    }
  }
  if (b->pieces[white-1][king][0].valid) {
    int kx = b->pieces[white-1][king][0].x;
    int ky = b->pieces[white-1][king][0].y;
    int kz = b->pieces[white-1][king][0].z;
    for (i=1; i<max_piece; ++i) {
      for (j=0; b->pieces[black-1][i][j].valid; ++j) {
        if (!b->pieces[black-1][i][j].captured) {
          int px = b->pieces[black-1][i][0].x;
          int py = b->pieces[black-1][i][0].y;
          int pz = b->pieces[black-1][i][0].z;
          eval += (int)floor(sqrt(abs(kx-px) + abs(ky-py) + abs(kz-pz))*10.0);
        }
      }
    }
  }
  return eval;
}

static evalt standard_evaluate(const evaluator* e, const board* b, int ply) {
  evalt eval = 0;
  if (terminal(b)) {
    if (b->pieces[white-1][king][0].captured
        && b->pieces[black-1][king][0].captured) return DRAW;
    if (b->pieces[black-1][king][0].captured) return W_WIN-ply;
    if (b->pieces[white-1][king][0].captured) return B_WIN+ply;
    const hasht h = b->hash_stack[b->ply];
    int i;
    for (i=b->ply-1; i>=0; --i)
      if (b->hash_stack[i] == h)
        return DRAW;
  }
  if (e->f_material)
    eval += evaluate_material(e, b);
  if (e->f_mobility)
    eval += evaluate_mobility(e, b);
  if (e->f_king_tropism)
    eval += evaluate_king_tropism(e, b);

  eval += (board_hash(b) % 21) - 10;
  return eval;
}

static evalt monte_carlo_evaluate(const evaluator* e, const board* b, int n, int depth, int ply) {
  evaluator e2;
  simple_evaluator(&e2);
  double sum = 0.0;
  int i, j;
  for (i=0; i<n; ++i) {
    board bt;
    movet  ms[256];
    memcpy(&bt, b, sizeof(board));
    for (j=0; j<depth && !terminal(&bt); ++j) {
      int num_m = all_moves(&bt, ms);
      make_move(&bt, ms[lrand48() % num_m]);
    }
    sum += standard_evaluate(&e2, &bt, 0);
  }
  return (int)floor(sum/n);
}

static evalt monte_carlo_evaluate_2(const evaluator* e, const board* b, int n, int depth, int ply) {
  evaluator e2;
  simple_evaluator(&e2);
  double sum = 0.0;
  int i, j;
  for (i=0; i<n; ++i) {
    movet ms[256];
    for (j=0; j<depth && !terminal(b); ++j) {
      int num_m = all_moves(b, ms);
      int which = lrand48() % num_m;
      make_move((board*)b, ms[which]);
    }
    sum += standard_evaluate(&e2, b, 0);
    for (j=j-1; j>=0; --j) unmake_move((board*)b);
  }
  return (int)floor(sum/n);
}

evalt evaluate(const evaluator* e, const board* b, int ply) {
  /* TODO: monte-carlo params in e */
  switch (e->f_monte_carlo) {
    case 1: return monte_carlo_evaluate(e, b, 100, 10, ply);
    case 2: return monte_carlo_evaluate_2(e, b, 100, 10, ply);
    case 0:
    default: return standard_evaluate(e, b, ply);
  }
}
evalt evaluate_relative(const evaluator* e, const board* b, int ply) {
  return (b->to_move==white) ? evaluate(e, b, ply) : -evaluate(e, b, ply);
}
int evaluate_moves_for_search(const evaluator* e, const board* b,
                              const movet* ml, evalt* vl, int nm) {
  int i;
  /* TODO: MVV/LVA */
  for (i=0; i<nm; ++i)
    vl[i] = (nm-i) + ((ml[i].flags&f_cap) ? 1000 : 0);
  return 0;
}
int evaluate_moves_for_quiescence(const evaluator* e, const board* b,
                              const movet* ml, evalt* vl, int nm) {
  int i;
  /* TODO: MVV/LVA */
  for (i=0; i<nm; ++i)
    vl[i] = (nm-i);
  return 0;
}

/* ************************************************************ */
