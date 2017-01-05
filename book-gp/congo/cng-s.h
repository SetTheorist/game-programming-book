#ifndef	CONGO_SEARCH_H
#define	CONGO_SEARCH_H
/* $Id: cng-s.h,v 1.1 2010/12/15 21:48:32 ahogan Exp $ */

#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

#include <stdio.h>
#include "cng-b.h"

void sort_moves(int n, move* ml, evalt* scorel);
inline void sort_it(int i, int n, move* ml, evalt* scorel);
evalt search_quiesce(board* b, int qdepth, evalt alpha, evalt beta, int ply);
evalt search_general(board* b, int depth, evalt alpha, evalt beta, int ply);
move search5(board* b, evalt* eval);
int show_game(FILE* f, board* b);

#ifdef  __cplusplus
};
#endif/*__cplusplus*/

#endif/*CONGO_SEARCH_H*/
