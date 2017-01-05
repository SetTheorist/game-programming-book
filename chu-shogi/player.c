/* $Id: player.c,v 1.1 2010/12/29 20:12:17 ahogan Exp ahogan $ */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "board.h"
#include "evaluate.h"
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
  evalt val = search(b, &p->ss, &p->e, &p->tc, pv, pv_length);
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
                 game_record* gm,
                 int adjudicate,
                 int verbose_output)
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
      if (!verbose_output) fprintf(stderr, ".");
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
      gm->plies[gm->ply].nodes_searched = nodes_searched;
      gm->plies[gm->ply].qnodes_searched = qnodes_searched;
      gm->plies[gm->ply].nodes_evaluated = nodes_evaluated;
      gm->plies[gm->ply].qnodes_evaluated = qnodes_evaluated;
      gm->plies[gm->ply].m = gm->pv_heap[gm->pv_heap_top];
      gm->plies[gm->ply].pv_length = pvl;
      gm->plies[gm->ply].pv_index = gm->pv_heap_top;
      gm->plies[gm->ply].annotation[0] = '\x00';

      gm->pv_heap_top += pvl;

      switch (sm) {
        case no_moves:
        case resign:
          gm->result = black;
          break;
        default: break;
      }
    }
    else if (gm->b.to_move==black)
    {
      if (!verbose_output) fprintf(stderr, ",");
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
      gm->plies[gm->ply].nodes_searched = nodes_searched;
      gm->plies[gm->ply].qnodes_searched = qnodes_searched;
      gm->plies[gm->ply].nodes_evaluated = nodes_evaluated;
      gm->plies[gm->ply].qnodes_evaluated = qnodes_evaluated;
      gm->plies[gm->ply].m = gm->pv_heap[gm->pv_heap_top];
      gm->plies[gm->ply].pv_length = pvl;
      gm->plies[gm->ply].pv_index = gm->pv_heap_top;
      gm->plies[gm->ply].annotation[0] = '\x00';

      gm->pv_heap_top += pvl;

      switch (sm) {
        case no_moves:
        case resign:
          gm->result = white;
          break;
        default: break;
      }
    }
    else
    {
      break;
    }
    if (gm->plies[gm->ply].sm == regular_move)
    {
      make_move(&gm->b, gm->plies[gm->ply].m);
      if (validate_board(&gm->b))
        fstatus(stderr, "<<---play_match(...%m)\n", gm->plies[gm->ply].m);
    }
    if (verbose_output)
    {
      fprintf(stdout, "\033[2J"); /* clear screen */
      fprintf(stdout, "\033[1;1H"); /* move cursor to upper-left corner */
      {
        const evalt e1 = evaluate(&p1->e, &gm->b, 1, B_INF, W_INF);
        const evalt e2 = evaluate(&p2->e, &gm->b, 1, B_INF, W_INF);
        ansi_fg_yellow(stdout);
        fprintf(stdout, "***** Evaluation: W=%+'10i (%10.6f)   B=%'+10i (%10.6f) *****\n",
            (int)e1, tanh(1e-3 * e1), (int)e2, tanh(1e-3 * e2));
        ansi_fg_white(stdout);
      }
      {
        ansi_fg_cyan(stdout);
        show_game_record_ply(stdout, &gm->plies[gm->ply], gm->pv_heap, gm->ply);
        ansi_fg_white(stdout);
      }
      show_board(stdout, &gm->b);
    }

    /* adjudicate overwhelming force */
    if (adjudicate)
    {
      const evalt e1 = evaluate(&p1->e, &gm->b, 1, B_INF, W_INF);
      const evalt e2 = evaluate(&p2->e, &gm->b, 1, B_INF, W_INF);
      if ((fabs(e1) > 3000.0) && (fabs(e2) > 3000.0) && ((double)e1*(double)e2 > 0.0))
        break;
    }
  }
  if (!verbose_output) fprintf(stderr, "!");

  /* adjudicate the game */
  {
    evalt v = evaluate(&gm->players[0].e, &gm->b, 0, B_INF, W_INF);
    if (v>1000)       gm->result = white;
    else if (v<-1000) gm->result = black;
    else              gm->result = none;
  }

  return gm->result;
}

int show_game_record_ply(FILE* f, const game_record_ply* gmp, const movet* pv_heap, int ply)
{
  if (gmp->sm != regular_move)
  {
    switch (gmp->sm) {
      case no_moves: fprintf(f, "{stalemate}"); break;
      case resign: fprintf(f, "{resigns}"); break;
      default: fprintf(f, "{special:%i}", gmp->sm); break;
    }
  }
  else
  {
    if (gmp->pv_length > 1) {
      fstatus(f, "%i%s%m { %M }", ply+1, gmp->side==white ? "." : "...",
              pv_heap[gmp->pv_index], &pv_heap[gmp->pv_index+1], gmp->pv_length-1);
    } else {
      fstatus(f, "%i%s%m", ply+1, gmp->side==white ? "." : "...",
              pv_heap[gmp->pv_index]);
    }
    fprintf(f, "  {%'i} %.3fs %'lluns/%'lluqns %'llune/%'lluqne %'.1fnps(e)",
        (int)gmp->ev, gmp->time_spent*1e-6,
        gmp->nodes_searched, gmp->qnodes_searched,
        gmp->nodes_evaluated, gmp->qnodes_evaluated,
        (gmp->qnodes_evaluated+gmp->nodes_evaluated)/(gmp->time_spent*1e-6));
    fprintf(f, "\n");
  }
  return 0;
}

int show_game_record(FILE* f, const game_record* gm, int show_boards)
{
  board b;
  int i, n=0;
  n += fprintf(f, "[White \"%s\"]\n", gm->players[0].description);
  n += fprintf(f, "[Black \"%s\"]\n", gm->players[1].description);
  char asc[64] = {0};
  asctime_r(&gm->start_time, asc);
  asc[sizeof(asc)-1] = '\x00';
  n += fprintf(f, "[Date \"%s\"]\n", asc);
  if (show_boards) init_board(&b);
  for (i=0; i<gm->ply; ++i)
  {
    n += show_game_record_ply(f, &gm->plies[i], gm->pv_heap, i);
    if (show_boards)
    {
      if (gm->plies[i].sm==regular_move)
      {
        make_move(&b, gm->plies[i].m);
        n += show_board(f, &b);
        validate_board(&b);
      }
    }
  }
  switch (gm->result)
  {
    case white: n += fprintf(f, "{1-0}"); break;
    case black: n += fprintf(f, "{0-1}"); break;
    case none: n += fprintf(f, "{1/2}"); break;
    default: n += fprintf(f, "{???}"); break;
  }
  n += fprintf(f, "\n");
  if (!show_boards) {
    show_board(f, &gm->b);
    n += fprintf(f, "\n");
  }
  return n;
}

int show_time_control(FILE* f, const time_control* tc)
{
  fprintf(f, "<");

  if (tc->starting_cs)
    fprintf(f, "%.2f", tc->starting_cs/100.0);
  if (tc->increment_cs)
    fprintf(f, "+%.2f", tc->increment_cs/100.0);
  if (tc->byoyomi_cs)
    fprintf(f, "B%.2f", tc->byoyomi_cs/100.0);
  if (tc->moves_per)
    fprintf(f, "M:%i", tc->moves_per);

  if (tc->max_depth) fprintf(f, "D:%i|", tc->max_depth);
  if (tc->max_nodes) fprintf(f, "N:%'llu|", tc->max_nodes);

  if (tc->moves_remaining) fprintf(f, "m:%i|", tc->moves_remaining);
  if (tc->remaining_cs) fprintf(f, "(%.2f)", tc->remaining_cs/100.0);

  fprintf(f, "[%.2f/%.2f/%.2f]", tc->allocated_cs/100.0, tc->panic_cs/100.0, tc->force_stop_cs/100.0);
  //tc->start_clock

  fprintf(f, ">");

  return 0;
}

int compute_time_for_move(time_control* tc, const board* b)
{
  if (tc->moves_per)
  {
    const int turns = (b->ply + 1)/2;
    tc->moves_remaining = 1 + tc->moves_per - (turns % tc->moves_per);
    sendlog("$ turns=%i  tc->moves_remaining=%i\n", turns, tc->moves_remaining);
  }

  const int moves = tc->moves_remaining ? tc->moves_remaining
                  : (b->ply > 150)      ? 25
                  : 1+(200 - b->ply)/2;
  const int per = (tc->byoyomi_cs > 10) ? 0 : (10 - tc->byoyomi_cs);
  sendlog("$ moves=%i  per=%i\n", moves, per);

  int base_time = tc->byoyomi_cs + (1*tc->increment_cs/2) + (tc->remaining_cs / (moves+2));
  const int base_time_save = base_time;
  if (base_time > tc->remaining_cs - 5*per*(moves+1))
    base_time = tc->remaining_cs - 5*per*(moves+1);
  /* bare minimum time */
  if (base_time < 5)
    base_time = 5;
  /* save a bit of extra time */
  if (base_time > 1000)
    base_time -= 250;
  sendlog("$ base_time_save=%i  base_time=%i\n", base_time_save, base_time);

  int panic_time = base_time * (moves>10 ? 8 : moves>5 ? 6 : 4);
  const int panic_time_save = panic_time;
  if (panic_time > tc->remaining_cs - per*moves)
    panic_time = tc->remaining_cs + tc->byoyomi_cs - per*moves;
  if (panic_time < 10)
    panic_time = 10;
  sendlog("$ panic_time_save=%i  panic_time=%i\n", panic_time_save, panic_time);

  tc->allocated_cs = base_time;
  tc->panic_cs = panic_time;
  tc->force_stop_cs = min(max(10,(tc->remaining_cs + tc->byoyomi_cs - per*moves)), 12*panic_time/10);

  sendlog("$ compute_time_for_move(): moves=%i  per=%i  base_time=%i(%i)  panic_time=%i(%i)  force_stop=%i\n",
      moves, per, base_time, base_time_save, panic_time, panic_time_save, tc->force_stop_cs);

  return 0;
}
