#ifndef	JUNGLE_SEARCH_H
#define	JUNGLE_SEARCH_H
/* $Id: jungle-search.h,v 1.3 2010/01/05 01:06:48 apollo Exp $ */

#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

#include <stdio.h>
#include "jungle-board.h"

void sort_moves(int n, move* ml, evalt* scorel);
inline void sort_it(int i, int n, move* ml, evalt* scorel);
evalt search_minmax(board* b, int depth, int ply);
evalt search_negamax(board* b, int depth, int ply);
evalt search_alphabeta(board* b, int depth, evalt alpha, evalt beta, int ply);
evalt search_quiesce(board* b, int qdepth, evalt alpha, evalt beta, int ply);
evalt search_general(board* b, int depth, evalt alpha, evalt beta, int ply);
move search1(board* b, int depth, evalt* eval);
move search2(board* b, int depth, evalt* eval);
move search3(board* b, int depth, evalt* eval);
move search5(board* b, evalt* eval);
int show_game(FILE* f, board* b);

#ifdef  __cplusplus
};
#endif/*__cplusplus*/

#endif/*JUNGLE_SEARCH_H*/
