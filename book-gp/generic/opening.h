#ifndef INCLUDED_OPENING_H_
#define INCLUDED_OPENING_H_
/* $Id: opening.h,v 1.1 2010/12/23 20:23:40 apollo Exp $ */
#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

#include <stdio.h>
#include "board.h"

typedef enum ob_flags {
  obf_normal, obf_never, obf_always,
} ob_flags;
typedef struct ob_page {
  movet m;
  ob_flags obf;
  double prob;
} ob_page;
typedef struct opening_book {
} opening_book;

int read_opening_book(FILE* f, opening_book* o);
int write_opening_book(FILE* f, const opening_book* o);
int show_opening_book(FILE* f, const opening_book* o);

#ifdef  __cplusplus
};
#endif/*__cplusplus*/
#endif /* INCLUDED_OPENING_H_ */
