/* $Id: player.c,v 1.1 2010/12/29 20:12:17 ahogan Exp ahogan $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "board.h"
#include "player.h"
#include "search.h"
#include "util.h"


evalt random_mover(const player* p, board* b, movet* pv, int* pv_length, special_move* sm)
{
  if (p==p){}
  *sm = regular_move;
  movet ml[MAXNMOVES];
  int n = gen_moves(b, ml);
  if (!n)
  {
    *sm = no_moves;
    *pv_length = 0;
    return 0;
  }
  int i = lrand48() % n;
  pv[0] = ml[i];
  *pv_length = 1;
  return 0;
}

evalt search_mover(const player* p, board* b, movet* pv, int* pv_length, special_move* sm)
{
  *sm = regular_move;
  evalt val = search(b, &p->ss, &p->e, pv, pv_length);
  if (*pv_length == 0) *sm = no_moves;
  return val;
}

evalt interactive_mover(const player* p, board* b, movet* pv, int* pv_length, special_move* sm)
{
  if (p==p) {}
  *sm = regular_move;
  char buff[1024];
  printf("\n");
  show_board(stdout, b);
  printf("\n");
  movet ml[MAXNMOVES];
  int i, n = gen_moves(b, ml);
  if (!n)
  {
    printf(" *** NO VALID MOVES ***\n");
    printf(" Press enter to continue: ");
    fgets(buff, sizeof(buff), stdin);
    *sm = no_moves;
    *pv_length = 0;
    return 0;
  }
  for (i=0; i<n; ++i)
  {
    ansi_underline(stdout);
    printf("%i.", i+1);
    ansi_reset(stdout);
    show_move(stdout, ml[i]);
    printf(((i%5)==4) ? "\n" : "  ");
  }
  if ((n-1%5)!=4) printf("\n");
  int mn = -1;
  while ((mn!=-99) && (mn<=0 || mn>n)) {
    printf(" Please enter move number [-99 to resign]: ");
    fgets(buff, sizeof(buff), stdin);
    mn = atoi(buff);
  }
  if (mn==-99) {
    *pv_length = 0;
    *sm = resign;
  } else {
    pv[0] = ml[mn-1];
    *pv_length = 1;
  }
  return 0;
}

void game_record_init(game_record* gm) {
  memset(gm, '\x00', sizeof(game_record));
  time_t tt = time(NULL);
  localtime_r(&tt, &gm->start_time);
  init_board(&gm->b);
  if (validate_board(&gm->b)) fprintf(stderr, "!*!*!*");
}

color play_match(player* p1, const time_control* tc1,
                 player* p2, const time_control* tc2,
                 game_record* gm)
{
  game_record_init(gm);
  gm->players[0] = *p1;
  gm->players[1] = *p2;
  gm->tc[0] = *tc1;
  gm->tc[1] = *tc2;

  gm->result = none;

  for (; gm->ply<512 && !terminal(&gm->b); ++(gm->ply))
  {
    ttable_init();
    if (gm->b.to_move==white)
    {
      fprintf(stderr, ".");
      ui64 bt, et, tt;
      special_move sm;
      int pvl = 0;
      bt = read_clock();
      evalt v = p1->mover(p1, &gm->b, &gm->pv_heap[gm->pv_heap_top], &pvl, &sm);
      et = read_clock();
      tt = et - bt;

      gm->plies[gm->ply].side = white;
      gm->plies[gm->ply].ev = v;
      gm->plies[gm->ply].sm = sm;
      gm->plies[gm->ply].time_spent = tt;
      gm->plies[gm->ply].m = gm->pv_heap[gm->pv_heap_top];
      gm->plies[gm->ply].pv_length = pvl;
      gm->plies[gm->ply].pv_index = gm->pv_heap_top;
      gm->plies[gm->ply].annotation[0] = '\x00';

      gm->pv_heap_top += pvl;

      switch (sm) {
        case no_moves:
        case resign:
          gm->result = black; break;
        default: break;
      }
    }
    else if (gm->b.to_move==black)
    {
      fprintf(stderr, ",");
      ui64 bt, et, tt;
      special_move sm;
      int pvl = 0;
      bt = read_clock();
      evalt v = p2->mover(p2, &gm->b, &gm->pv_heap[gm->pv_heap_top], &pvl, &sm);
      et = read_clock();
      tt = et - bt;

      gm->plies[gm->ply].side = black;
      gm->plies[gm->ply].ev = v;
      gm->plies[gm->ply].sm = sm;
      gm->plies[gm->ply].time_spent = tt;
      gm->plies[gm->ply].m = gm->pv_heap[gm->pv_heap_top];
      gm->plies[gm->ply].pv_length = pvl;
      gm->plies[gm->ply].pv_index = gm->pv_heap_top;
      gm->plies[gm->ply].annotation[0] = '\x00';

      gm->pv_heap_top += pvl;

      switch (sm) {
        case no_moves:
        case resign:
          gm->result = white; break;
        default: break;
      }
    }
    else
    {
      break;
    }
    if (gm->plies[gm->ply].sm==regular_move)
    {
      make_move(&gm->b, gm->plies[gm->ply].m);
      if (validate_board(&gm->b))
        fstatus(stderr, "<<---play_match(...%m)\n", gm->plies[gm->ply].m);
    }
    //fprintf(stderr, "%i", gm->plies[gm->ply].sm);
    show_board(stdout, &gm->b);
  }
  fprintf(stderr, "!");
  if (terminal(&gm->b)) {
    evalt v = evaluate(&gm->players[0].e, &gm->b, 0);
    if (v>0) gm->result = white;
    else if (v<0) gm->result = black;
    else gm->result = none;
  }

  return gm->result;
}

int show_game_record(FILE* f, const game_record* gm, int show_boards) {
  board b;
  int i;
  fprintf(f, "[White \"%s\"]\n", gm->players[0].description);
  fprintf(f, "[Black \"%s\"]\n", gm->players[1].description);
  char asc[64] = {0};
  //_asctime_r(&gm->start_time, asc);
  asc[strlen(asc)-1] = '\x00';
  fprintf(f, "[Date \"%s\"]\n", asc);
  if (show_boards) init_board(&b);
  for (i=0; i<gm->ply; ++i) {
    if (gm->plies[i].sm != regular_move) {
      switch (gm->plies[i].sm) {
        case no_moves: fprintf(f, "{stalemate}"); break;
        case resign: fprintf(f, "{resigns}"); break;
      }
    } else {
      if (gm->plies[i].pv_length > 1) {
        fstatus(f, "%i%s%m{%M}", i+1, gm->plies[i].side==white ? "." : "...",
                gm->pv_heap[gm->plies[i].pv_index],
                &gm->pv_heap[gm->plies[i].pv_index+1], gm->plies[i].pv_length-1);
        fprintf(f, "(%.3f)\n", gm->plies[i].time_spent*1e-6);
      } else {
        fstatus(f, "%i%s%m", i+1, gm->plies[i].side==white ? "." : "...",
                gm->pv_heap[gm->plies[i].pv_index]);
        fprintf(f, "(%.3f)\n", gm->plies[i].time_spent*1e-6);
      }
    }
    if (show_boards) {
      if (gm->plies[i].sm==regular_move) {
        //ansi_clear_screen(f);
        make_move(&b, gm->plies[i].m);
        show_board(f, &b);
        validate_board(&b);
      }
    }
  }
  switch (gm->result) {
    case white: fprintf(f, "{1-0}"); break;
    case black: fprintf(f, "{0-1}"); break;
    case none: fprintf(f, "{1/2}"); break;
    default: fprintf(f, "{???}"); break;
  }
  fprintf(f, "\n");
  if (!show_boards) {
    show_board(f, &gm->b);
    fprintf(f, "\n");
  }
  return 0;
}
