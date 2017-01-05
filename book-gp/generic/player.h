#ifndef INCLUDED_PLAYER_H_
#define INCLUDED_PLAYER_H_
/* $Id: player.h,v 1.1 2010/12/29 20:12:17 ahogan Exp ahogan $ */

#include <stdio.h>
#include <time.h>

#include "board.h"
#include "search.h"
#include "util.h"

typedef struct time_control
{
  int sec_starting;
  int sec_per_move;
} time_control;

typedef enum special_move
{
  regular_move, no_moves, resign, offer_draw, accept_draw, adjourn
} special_move;

typedef struct player
{
  char description[256];
  double elo;
  int wb_wld[2][3];
  search_settings ss;
  evaluator e;
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
  game_record_ply plies[512];
  movet pv_heap[16*1024];
  board b;
} game_record;

color play_match(player* p1, const time_control* tc1,
                 player* p2, const time_control* tc2, game_record* gm);

int show_game_record(FILE* f, const game_record* gm, int show_boards);


#endif /* INCLUDED_PLAYER_H_ */
