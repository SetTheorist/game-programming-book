#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "search.h"


int depth_chart() {
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
  
  for (d=100; d<=1000; d+=100) {
    for (ftt=0; ftt<5; ++ftt) {
      switch (ftt) {
        case 0: make_simple_negamax_searcher(&ss, d, 0); if (d>=400) continue; break;
        case 1: make_simple_negamax_searcher(&ss, d, 1); if (d>=500) continue; break;
        case 2: make_simple_alphabeta_searcher(&ss, d, 0); if (d>=600) continue; break;
        case 3: make_simple_alphabeta_searcher(&ss, d, 1); break;
        case 4: make_simple_alphabeta_searcher(&ss, d, 1); ss.f_id=1; ss.id_base=100; ss.id_step=100; break;
      }
      init_board(&b);
      make_move(&b, ml[1]);
      ttable_init();
      unsigned int tb = read_clock();
      hasht bh = b.h;
      evalt res = search(&b, &ss, &e, pv, &pvl);
      hasht eh = b.h;
      if (bh!=eh) printf("!!! %016llX != %016llX !!!\n", bh, eh);
      unsigned int te = read_clock();
      unsigned int tt = te - tb;
      show_settings(stdout, &ss);
      //show_evaluator(stdout, &e);
      printf("\n    <<%i>> %9in  %8.2fms  %6.1fknps",
             res, nodes_searched, tt*1e-3, nodes_searched/(tt*1e-3));
      printf("   [ ");
      for (i=0; i<pvl; ++i) {
        show_move(stdout, pv[i]);
        printf(" ");
      }
      printf("]\n");
    }
  }
  return 0;
}

int random_moves() {
  printf("%i %i\n", (int)sizeof(board), (int)sizeof(movet));
  int n_moves = 0, max_moves = 0, max_caps = 0;
  int game_len = 200;
  int xx;
  for (xx=0; xx<10; ++xx) {
    char buff[256];
    board b;
    int i, j, n, c;
    movet ml[1024];
    init_board(&b);
    for (i=0; i<game_len; ++i) {
      ansi_cursor_position(stdout, 1, 1);
      ansi_clear_to_eos(stdout);
      show_board(stdout, &b);
      n = gen_moves(&b, ml);
      for (c=j=0; j<n; ++j) if (ml[j].capture) ++c;
      if (c>max_caps) max_caps = c;
      if (0)
      for (j=0; j<n; ++j) {
        show_move(stdout, ml[j]);
        printf("  ");
        if ((j%7)==6) printf("\n");

      }
      printf("\n");
      int j = lrand48()%n;
      make_move(&b, ml[j]);
      //fgets(buff, 256, stdin);
      n_moves += n;
      if (n > max_moves) max_moves = n;
    }
    ansi_cursor_position(stdout, 1, 1);
    ansi_clear_to_eos(stdout);
    show_board(stdout, &b);
    for (--i; i>=0; --i) {
      unmake_move(&b);
      ansi_cursor_position(stdout, 1, 1);
      ansi_clear_to_eos(stdout);
      show_board(stdout, &b);
    }

  }
  printf("Average # moves: %i\n", n_moves/(game_len*xx));
  printf("Maximum # moves: %i\n", max_moves);
  printf("Maximum # captures: %i\n", max_caps);

  return 0;
}

int main(int argc, char* argv[]) {
  srand48(argc>1 ? atoi(argv[1]) : -31415926);
  init_hash();

  board b;
  init_board(&b);
  show_board(stdout, &b);
  printf("\n");
  movet ml[64];
  int n = gen_moves(&b, ml);
  int i;
  for (i=0; i<n; ++i) {
    printf("  ");
    show_move(stdout, ml[i]);
  }
  printf("\n");

  //depth_chart();

  return 0;
}
