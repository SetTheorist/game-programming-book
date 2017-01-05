/* $Id: tafl.c,v 1.7 2009/12/12 21:34:04 apollo Exp apollo $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "board.h"
#include "search.h"
#include "util.h"

int main(int argc, char* argv[]) {
  int  i, n, d;
  int  depths[2] = {500,200};
  board  b;
  evaluator e;
  search_settings ss;

  srand48(argc>1 ? atoi(argv[1]) : -13);
  init_hash();

  setup_board(&b);
  simple_evaluator(&e);
  make_simple_alphabeta_searcher(&ss, 500, 0);

  for(i=0; ; ++i) {
    unsigned int bt, et;
    movet  m;
    movet  pv[32];
    int pv_length;
    int  val;
    show_board(stdout, &b);
    //for (d=0; d<5; ++d) 
    d = 1;
    {
      e.use_mobility = (b.side==white);
      tt_hit = tt_false = tt_deep = tt_shallow = tt_used = 0;
      ttable_init();
      ss.depth = depths[b.side==white ? 0 : 1];
      bt = read_clock();
      switch (d) {
      case 0:
        printf(" --- Alpha-beta(tt+id++)(%i) ---\n", depths[b.side]);
        ss.f_tt = 1;
        ss.f_id = 1;
        ss.id_base = (d%200)==100 ? 100 : 200;
        ss.id_step = 200;
        break;
      case 1:
        printf(" --- Alpha-beta(tt+id)(%i) ---\n", depths[b.side]);
        ss.f_tt = 1;
        ss.f_id = 1;
        ss.id_base = 100;
        ss.id_step = 100;
        break;
      case 2:
        printf(" --- Alpha-beta(tt)(%i) ---\n", depths[b.side]);
        ss.f_tt = 1;
        ss.f_id = 0;
        break;
      case 3:
        printf(" --- Alpha-beta(id)(%i) ---\n", depths[b.side]);
        ss.f_tt = 0;
        ss.f_id = 1;
        ss.id_base = 100;
        ss.id_step = 100;
        break;
      case 4:
        printf(" --- Alpha-beta(simple)(%i) ---\n", depths[b.side]);
        ss.f_tt = 0;
        ss.f_id = 0;
        break;
      }
      val = search(&b, &ss, &e, pv, &pv_length);
      et = read_clock();
      show_settings(stdout, &ss); printf("\n");
      printf("< "); for (n=0; n<pv_length; ++n) show_move(stdout, pv[n]); printf(" >\n");
      m = pv[0];
      printf("[%8in, %.4fs : %9.2fnps || tt:hit=%i false=%i deep=%i shallow=%i used=%i] %i:%c",
        nodes_searched, (double)(et-bt)*1e-6, 1e6*(double)nodes_searched/(double)(et-bt),
        tt_hit, tt_false, tt_deep, tt_shallow, tt_used,
        i, "WBN"[b.side]
        );
      show_move(stdout, m); printf(" {{%i}}}\n", val);
      fflush(stdout);
    }
    int mm = make_move(&b, m);
    if (b.terminal) break;
  }
  show_board(stdout, &b);
  printf("\n [[[%i]]]\n", evaluate(&e, &b, 0));

  return 0;
}
