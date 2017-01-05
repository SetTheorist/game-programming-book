#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "search.h"

int main(int argc, char* argv[]) {
  srand48(argc>1 ? atoi(argv[1]) : -31415926);
  init_hash();

  if (0) {
    board b;
    int i, j, n;
    movet gm[64];
    movet ml[64];
    init_board(&b);
    for (j=0; j<20; ++j) {
      show_board(stdout, &b);
      n = gen_moves(&b, ml);
      for (i=0; i<n; ++i) {
        show_move(stdout, ml[i]);
        int mm = make_move(&b, ml[i]);
        if (mm==invalid_move) printf("X");
        else unmake_move(&b);
        printf(" ");
      }
      printf("\n");
      if (terminal(&b)) { break; }
      int mm;
      do {
        i = lrand48()%n;
        mm = make_move(&b, ml[i]);
      } while (mm==invalid_move);
      printf("*** "); show_move(stdout, ml[i]); printf(" ***\n");
      gm[j] = ml[i];
    }
    printf("========================================\n");
    show_board(stdout, &b);
    printf("========================================\n");
    for (--j; j>=0; --j) {
      unmake_move(&b);
      show_board(stdout, &b);
    }
  }

  if (1) {
    board b;
    search_settings ss;
    evaluator e;
    movet pv[256];
    movet ml[64];
    int pvl, i, n, d, ftt;
  
    init_board(&b);
    simple_evaluator(&e);
    show_board(stdout, &b);
    n = gen_moves(&b, ml);
    for (i=0; i<n; ++i) {
      show_move(stdout, ml[i]);
      int mm = make_move(&b, ml[i]);
      if (mm==invalid_move) printf("X");
      else unmake_move(&b);
      printf(" ");
    }
    printf("\n********** **********\n");
    make_move(&b, ml[0]);
    show_board(stdout, &b);
    printf("\n********** **********\n");
  
    for (d=100; d<=2000; d+=100) {
      for (ftt=0; ftt<5; ++ftt) {
        switch (ftt) {
          case 0: make_simple_negamax_searcher(&ss, d, 0); if (d>=500) continue; break;
          case 1: make_simple_negamax_searcher(&ss, d, 1); if (d>=600) continue; break;
          case 2: make_simple_alphabeta_searcher(&ss, d, 0); if (d>=600) continue; break;
          case 3: make_simple_alphabeta_searcher(&ss, d, 1); break;
          //case 4: make_simple_alphabeta_searcher(&ss, d, 1); ss.f_id=1; ss.id_base=(d%200==100)?100:200; ss.id_step=200; break;
          case 4: make_simple_alphabeta_searcher(&ss, d, 1); ss.f_id=1; ss.id_base=100; ss.id_step=100; break;
        }
        init_board(&b);
        make_move(&b, ml[1]);
        ttable_init();
        unsigned int tb = read_clock();
        evalt res = search(&b, &ss, &e, pv, &pvl);
        unsigned int te = read_clock();
        unsigned int tt = te - tb;
        show_settings(stdout, &ss); show_evaluator(stdout, &e);
        fstatus(stdout, "\n    <<%i>> %9in  %8.2fms  %6.1fknps   [%M]\n",
               res, nodes_searched, tt*1e-3, nodes_searched/(tt*1e-3), pv, pvl);
      }
    }
  }

  return 0;
}
