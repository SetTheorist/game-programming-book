#ifndef INCLUDED_PLAYER_H_
#define INCLUDED_PLAYER_H_
/* $Id: player.h,v 1.1 2010/12/29 20:12:17 ahogan Exp ahogan $ */

#include <stdio.h>
#include <time.h>

#include "board.h"
#include "search.h"
#include "util.h"

typedef enum special_move
{
  regular_move, no_moves, resign, offer_draw, accept_draw, adjourn
} special_move;

typedef struct player
{
  char            description[256];
  double          elo;
  int             wb_wld[2][3];
  search_settings ss;
  evaluator       e;
  time_control    tc;
  evalt (*mover)(const struct player* p, board* b, movet* pv, int* pv_length, special_move* sm);
} player;

evalt random_mover(const player* p, board* b, movet* pv, int* pv_length, special_move* sm);
evalt search_mover(const player* p, board* b, movet* pv, int* pv_length, special_move* sm);
evalt interactive_mover(const player* p, board* b, movet* pv, int* pv_length, special_move* sm);

typedef struct game_record_ply
{
  color side;
  evalt ev;
  movet m;
  int pv_length;
  int pv_index;
  ui64 time_spent;
  ui64 nodes_searched;
  ui64 qnodes_searched;
  ui64 nodes_evaluated;
  ui64 qnodes_evaluated;
  special_move sm;
  char annotation[16];
} game_record_ply;

typedef struct game_record
{
  struct tm start_time;
  time_control  tc[2];
  int ply;
  color result;
  player  players[2];
  int pv_heap_top;
  game_record_ply plies[1024];
  movet pv_heap[32*1024];
  board b;
} game_record;

color play_match(player* p1, const time_control* tc1,
                 player* p2, const time_control* tc2, game_record* gm,
                 int adjudicate,
                 int verbose_output);

int show_game_record_ply(FILE* f, const game_record_ply* gmp, const movet* pv_heap, int ply);
int show_game_record(FILE* f, const game_record* gm, int show_boards);

int compute_time_for_move(time_control* tc, const board* b);

int show_time_control(FILE* f, const time_control* tc);


#endif /* INCLUDED_PLAYER_H_ */
