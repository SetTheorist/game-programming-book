/* $Id: main.c,v 1.4 2010/12/23 23:59:43 apollo Exp apollo $ */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "board.h"
#include "search.h"
#include "util.h"

/* ************************************************************ */

int random_moves() {
  int i, j, k, n;
  int tot_moves = 0;
  int n_moves = 100;
  board b;

  setup_board(&b);

  for (i=0; i<n_moves; ++i) {
    movet ms[256];
    n = gen_moves(&b, ms);
    tot_moves += n;
    j=0;
    do {
      k = lrand48() % n;
    } while (j++<15 && b.b[ms[k].f_z][ms[k].f_y][ms[k].f_x].p!=basilisk);
    show_move_b(stdout, &b, ms[k]); printf("\n");
    if (!(i%5)) {
      for (j=0; j<n; ++j) {
        printf("%i.", j);
        show_move_b(stdout, &b, ms[j]);
        printf((j%8)==7 ? "\n" : "\t");
      }
      printf("\n\n");
    }
    make_move(&b, ms[k]);
    show_board(stdout, &b);
    if (!validate_board(&b)) {
      printf(" --> random_moves() ");
      show_move(stdout, ms[k]);
      printf(" \n");
    }
  }
  printf("---------------------------------------- ----------------------------------------\n");
  printf("Average number of moves: %.1f\n", (double)tot_moves / n_moves);
  printf("---------------------------------------- ----------------------------------------\n");
  for (i=n_moves-1; i>=0; --i) {
    //show_board(stdout, &b);
    unmake_move(&b);
    validate_board(&b);
  }
  //show_board(stdout, &b);
  return 0;
}

int comparison() {
  int i, j;
  evaluator e;
  simple_evaluator(&e);
  e.f_material = 1;
  e.f_mobility = 1;
  e.f_control = 1;
  e.f_king_tropism = 1;
  search_settings s_nm, s_ab;
  make_simple_negamax_searcher(&s_nm, 100, 0);
  make_simple_alphabeta_searcher(&s_ab, 100, 0);
  movet pv[32];
  int pv_length;
  for (i=2; i<=6; ++i) {
    memset(pv, '\x00', sizeof(pv));
    pv_length = 0;
    s_nm.depth = i*100;
    s_ab.depth = i*100;
    printf("<<<<< depth = %i >>>>>\n", i);
    board b;
    int v;
    unsigned int tb;
    unsigned int te;
    unsigned int tt;

    if (i<=2) {
      setup_board(&b);
      s_nm.f_tt = 0;
      s_ab.f_id = 0;
      s_ab.f_asp = 0;
      ttable_init();
      tb = read_clock();
      v = search(&b, &s_nm, &e, pv, &pv_length);
      te = read_clock();
      tt = te - tb;
      show_settings(stdout, &s_nm);
      printf("  nodes=%i sec=%.2f nps=%.2f val=%i\n",
             nodes_searched, (double)tt*1e-6, (double)nodes_searched/(tt*1e-6), v);
      for (j=0; j<pv_length; ++j) {
        printf("  "); show_move_b(stdout, &b, pv[j]);
        make_move(&b, pv[j]);
      } for (j=pv_length-1; j>=0; --j) unmake_move(&b);
      printf("\n");
    }
    if (i<=3) {
      setup_board(&b);
      s_nm.f_tt = 1;
      s_ab.f_id = 0;
      s_ab.f_asp = 0;
      ttable_init();
      tb = read_clock();
      v = search(&b, &s_nm, &e, pv, &pv_length);
      te = read_clock();
      tt = te - tb;
      show_settings(stdout, &s_nm);
      printf("  nodes=%i sec=%.2f nps=%.2f val=%i\n",
             nodes_searched, (double)tt*1e-6, (double)nodes_searched/(tt*1e-6), v);
      printf("    tt: hits=%i shallow=%i false=%i exact=%i lower=%i upper=%i\n",
        tt_hit, tt_shallow, tt_false, tt_used_exact, tt_used_lower, tt_used_upper);
      for (j=0; j<pv_length; ++j) {
        printf("  "); show_move_b(stdout, &b, pv[j]);
        make_move(&b, pv[j]);
      } for (j=pv_length-1; j>=0; --j) unmake_move(&b);
      printf("\n");
    }
    if (i<=5) {
      setup_board(&b);
      s_ab.f_tt = 0;
      s_ab.f_id = 0;
      s_ab.f_asp = 0;
      ttable_init();
      tb = read_clock();
      v = search(&b, &s_ab, &e, pv, &pv_length);
      te = read_clock();
      tt = te - tb;
      show_settings(stdout, &s_ab);
      printf("  nodes=%i sec=%.2f nps=%.2f val=%i\n",
             nodes_searched, (double)tt*1e-6, (double)nodes_searched/(tt*1e-6), v);
      printf("    tt: hits=%i shallow=%i false=%i exact=%i lower=%i upper=%i\n",
        tt_hit, tt_shallow, tt_false, tt_used_exact, tt_used_lower, tt_used_upper);
      for (j=0; j<pv_length; ++j) {
        printf("  "); show_move_b(stdout, &b, pv[j]);
        make_move(&b, pv[j]);
      } for (j=pv_length-1; j>=0; --j) unmake_move(&b);
      printf("\n");
    }
    if (i<=7) {
      ttable_init();
      setup_board(&b);
      s_ab.f_tt = 1;
      s_ab.f_id = 0;
      s_ab.f_asp = 0;
      ttable_init();
      tb = read_clock();
      v = search(&b, &s_ab, &e, pv, &pv_length);
      te = read_clock();
      tt = te - tb;
      show_settings(stdout, &s_ab);
      printf("  nodes=%i sec=%.2f nps=%.2f val=%i\n",
             nodes_searched, (double)tt*1e-6, (double)nodes_searched/(tt*1e-6), v);
      printf("    tt: hits=%i shallow=%i false=%i exact=%i lower=%i upper=%i\n",
        tt_hit, tt_shallow, tt_false, tt_used_exact, tt_used_lower, tt_used_upper);
      for (j=0; j<pv_length; ++j) {
        printf("  "); show_move_b(stdout, &b, pv[j]);
        make_move(&b, pv[j]);
      } for (j=pv_length-1; j>=0; --j) unmake_move(&b);
      printf("\n");
    }
    if (i<=7) {
      ttable_init();
      setup_board(&b);
      s_ab.f_tt = 1;
      s_ab.f_id = 1;
      s_ab.id_base = 100;
      s_ab.id_step = 100;
      s_ab.f_asp = 1;
      s_ab.asp_width = 250;
      ttable_init();
      tb = read_clock();
      v = search(&b, &s_ab, &e, pv, &pv_length);
      te = read_clock();
      tt = te - tb;
      show_settings(stdout, &s_ab);
      printf("  nodes=%i sec=%.2f nps=%.2f val=%i\n",
             nodes_searched, (double)tt*1e-6, (double)nodes_searched/(tt*1e-6), v);
      printf("    tt: hits=%i shallow=%i false=%i exact=%i lower=%i upper=%i\n",
        tt_hit, tt_shallow, tt_false, tt_used_exact, tt_used_lower, tt_used_upper);
      for (j=0; j<pv_length; ++j) {
        printf("  "); show_move_b(stdout, &b, pv[j]);
        make_move(&b, pv[j]);
      } for (j=pv_length-1; j>=0; --j) unmake_move(&b);
      printf("\n");
    }
    if (i<=7) {
      ttable_init();
      setup_board(&b);
      s_ab.f_tt = 1;
      s_ab.f_id = 1;
      s_ab.id_base = i%2==1 ? 100 : 200;
      s_ab.id_step = 200;
      s_ab.f_asp = 1;
      s_ab.asp_width = 250;
      ttable_init();
      tb = read_clock();
      v = search(&b, &s_ab, &e, pv, &pv_length);
      te = read_clock();
      tt = te - tb;
      show_settings(stdout, &s_ab);
      printf("  nodes=%i sec=%.2f nps=%.2f val=%i\n",
             nodes_searched, (double)tt*1e-6, (double)nodes_searched/(tt*1e-6), v);
      printf("    tt: hits=%i shallow=%i false=%i exact=%i lower=%i upper=%i\n",
        tt_hit, tt_shallow, tt_false, tt_used_exact, tt_used_lower, tt_used_upper);
      for (j=0; j<pv_length; ++j) {
        printf("  "); show_move_b(stdout, &b, pv[j]);
        make_move(&b, pv[j]);
      } for (j=pv_length-1; j>=0; --j) unmake_move(&b);
      printf("\n");
    }
    // ab_tt_mo
    // ab_tt_mo_id
    // ab_tt_mo_2_id

#if 0
    if (i<=3) {
      setup_board(&b);
      m.i = 0;
      tb = read_clock();
      v = monte_carlo(&b, 10, &m, 100*(1+i/2));
      te = read_clock();
      tt = te - tb;
      printf("monte_carlo:     nodes=%i sec=%.2f nps=%.2f val=%i move=",
             nodes_searched, (double)tt*1e-6, (double)nodes_searched/(tt*1e-6), v);
      show_move_b(stdout, &b, m);
      printf("\n");
    }
    if (i<=3) {
      setup_board(&b);
      m.i = 0;
      tb = read_clock();
      v = monte_carlo_2(&b, 10, &m, 100*(1+i/2));
      te = read_clock();
      tt = te - tb;
      printf("monte_carlo_2:   nodes=%i sec=%.2f nps=%.2f val=%i move=",
             nodes_searched, (double)tt*1e-6, (double)nodes_searched/(tt*1e-6), v);
      show_move_b(stdout, &b, m);
      printf("\n");
    }
#endif
    fflush(stdout);
  }

  return 0;
}

int match(int show, int d1, int qd1, int d2, int qd2) {
  board b;
  int i, j, k, v;
  unsigned int tb, te, tt;
  setup_board(&b);
  evaluator e;
  simple_evaluator(&e);
  movet pv[64];
  int pv_length;
  search_settings ss;
  make_simple_alphabeta_searcher(&ss, 100, 1);

  if (show) printf("---------------------------------------- ----------------------------------------\n");
  {
    setup_board(&b);
    for (j=0; j<500; ++j) {
      //ansi_cursor_position(stdout, 0,0);
      //ansi_clear_to_eos(stdout);
      if (show) {
        e.f_material = 1;
        e.f_mobility = 0;
        e.f_control = 0;
        e.f_king_tropism = 0;
        printf("[[[1111:%i]]]", evaluate(&e, &b, 0));
      }if (show) {
        e.f_material = 0;
        e.f_mobility = 1;
        e.f_control = 0;
        e.f_king_tropism = 0;
        printf("[[[0100:%i]]]", evaluate(&e, &b, 0));
      }if (show) {
        e.f_material = 0;
        e.f_mobility = 1;
        e.f_control = 1;
        e.f_king_tropism = 0;
        printf("[[[0110:%i]]]", evaluate(&e, &b, 0));
      }if (show) {
        e.f_material = 0;
        e.f_mobility = 0;
        e.f_control = 0;
        e.f_king_tropism = 1;
        printf("[[[0001:%i]]]", evaluate(&e, &b, 0));
      }if (show) {
        e.f_material = 1;
        e.f_mobility = 1;
        e.f_control = 1;
        e.f_king_tropism = 1;
        printf("[[[1111:%i]]]\n", evaluate(&e, &b, 0));
      }
      if (show) show_board(stdout, &b);
      if (terminal(&b)) break;
      ttable_init();
      char* s;
      if (show) printf((b.to_move==white ? "%i." : "%i..."), (j/2)+1);
      fflush(stdout);
      tb = read_clock();
      memset(pv, '\x00', sizeof(pv));
      pv_length = 0;
      ttable_init();
      if (b.to_move==white) {
        ss.f_quiesce = qd1>0;
        ss.qdepth = qd1;
        ss.depth = d1;
        e.f_material = 1;
        e.f_mobility = 1;
        e.f_control = 1;
        e.f_king_tropism = 1;
      } else {
        ss.f_quiesce = qd2>0;
        ss.qdepth = qd2;
        ss.depth = d2;
        e.f_material = 1;
        e.f_mobility = 1;
        e.f_control = 0;
        e.f_king_tropism = 0;
      }
      v = search(&b, &ss, &e, pv, &pv_length);
      te = read_clock();
      tt = te - tb;
      if (b.to_move==black) v = -v;
      if (show) show_move_b(stdout, &b, pv[0]);
      if (show) printf(" {%i}", v);
      if (show) printf("  ( ");
      for (k=0; k<pv_length; ++k) {
        if (show) show_move_b(stdout, &b, pv[k]);
        if (show) printf(" ");
        make_move(&b, pv[k]);
      } for (k=pv_length-1; k>=0; --k) unmake_move(&b);
      if (show) printf(")\n");

      if (show) show_settings(stdout, &ss);
      if (show) printf("\n");
      if (show) printf("     nodes=%i sec=%.2f nps=%.2f\n",
             nodes_searched, (double)tt*1e-6, (double)(nodes_searched)/(tt*1e-6));
      if (ss.f_tt)
        if (show) printf("    tt: hit=%i shallow=%i false=%i exact=%i lower=%i upper=%i\n",
               tt_hit, tt_shallow, tt_false, tt_used_exact, tt_used_lower, tt_used_upper);
      if (show) if (!validate_board(&b)) {
        printf("  --> match()...\n");
      }
      make_move(&b, pv[0]);
      if (0){
        char buff[128];
        fgets(buff, 128, stdin);
      }
    }
    if (b.pieces[white-1][king][0].captured) {
      if (show) printf("**** BLACK WIN ****\n");
      return -1;
    } else if (b.pieces[black-1][king][0].captured) {
      if (show) printf("**** WHITE WIN ****\n");
      return 1;
    } else {
      if (show) printf("**** DRAW ****\n");
      return 0;
    }
  }
}

int tourny1() {
  int i, j, k;
#define NP  6
  int res[NP][NP][3];
  memset(res, 0, sizeof(res));
  for (i=0; i<10; ++i) {
    for (j=0; j<NP; ++j) {
      for (k=0; k<NP; ++k) {
        init_hash();
        ++res[j][k][1+match(0, 100*(j/3+1), (j%3)*100, 100*(k/3+1), (k%3)*100)];
        { int jj, kk;
          printf("     ");
          for (kk=0; kk<NP; ++kk) printf("     %iq%i     ", kk/3+1, (kk%3));
          printf("\n");
          for (jj=0; jj<NP; ++jj) {
            printf("%iq%i  ", jj/3+1, (jj%3));
            for (kk=0; kk<NP; ++kk) {
              printf("+%2i/=%2i/-%2i  ", res[jj][kk][2], res[jj][kk][1], res[jj][kk][0]);
            }
            printf("\n");
          }
          printf("\n");
        }
      }
    }
  }
#undef  NP
  return 0;
}

int tournament_2() {
  const int SILENT = 1;
  board    b;
#define    NP    32
#define    BASE    3
#define    NROUND    1000
//#define    KFACTOR    (32.0 - ((double)i/(double)NROUND)*24.0)
#define    KFACTOR    32.0
  int    i, j, n, i1, i2;
  int    num_players = 0;
  int    w_r[NP], w_b[NP], d_r[NP], d_b[NP], num[NP], num_moves[NP];
  double    elo[NP];
  double    nodes_spent[NP];
  double    time_spent[NP];
  search_settings    player_settings[NP];
  evaluator          player_evaluators[NP];
  search_settings    settings1, settings2;
  evaluator          evaluator1, evaluator2;

  setup_board(&b);
  for (i=0; i<NP; ++i) {
    if ((i&15)==0 || ((i&4)&&!(i&2))) continue;
    make_simple_alphabeta_searcher(&player_settings[num_players], 100*(BASE+i/32), 1);
    player_settings[num_players].f_tt = 1;
    player_settings[num_players].f_id = 0;
    //player_settings[num_players].id_base = 100;
    //player_settings[num_players].id_step = 100;
    player_settings[num_players].f_quiesce = (i&16) ? 1 : 0;
    player_settings[num_players].qdepth = 200;
    player_settings[num_players].f_nmp = 0;
    simple_evaluator(&player_evaluators[num_players]);
    player_evaluators[num_players].f_material = (i&1) ? 1 : 0;
    player_evaluators[num_players].f_mobility = (i&2) ? 1 : 0;
    player_evaluators[num_players].f_control = (i&4) ? 1 : 0;
    player_evaluators[num_players].f_king_tropism = (i&8) ? 1 : 0;
    elo[num_players] = 1500.0;
    w_r[num_players] = w_b[num_players] = d_r[num_players] = d_b[num_players] = 0;
    num[num_players] = num_moves[num_players] = 0;
    nodes_spent[num_players] = time_spent[num_players] = 0;
    ++num_players;
  }

  for (i=0; i<NROUND; ++i) {
    do {
      i1 = abs(lrand48()) % num_players;
      i2 = abs(lrand48()) % num_players;
    } while (i1==i2);

    settings1 = player_settings[i1];
    settings2 = player_settings[i2];
    evaluator1 = player_evaluators[i1];
    evaluator2 = player_evaluators[i2];
    printf("** "); show_settings(stdout, &settings1); show_evaluator(stdout, &evaluator1);
    printf(" vs "); show_settings(stdout, &settings2); show_evaluator(stdout, &evaluator2);
    printf(" **\n");
    init_hash();
    setup_board(&b);

    for (j=0; j<100; ++j) {
      search_settings settings;
      evaluator eval;
      unsigned int    bt, et;
      evalt    val;
      movet   pv[64];
      int pv_length;

      if (b.to_move==white) {
          settings = settings1;
          eval = evaluator1;
      } else if (b.to_move==black) {
          settings = settings2;
          eval = evaluator2;
      } else {
      break;
      }
      nodes_searched = 0;
      tt_hit = tt_false = tt_deep = tt_shallow = tt_used = 0;
      ttable_init();
      bt = read_clock();
      if (!SILENT) {printf(" --- "); show_settings(stdout, &settings); printf(" ---\n");}
      val = search(&b, &settings, &eval, pv, &pv_length);
      et = read_clock();
      if (!SILENT) {for (n=0; n<pv_length; ++n) show_move(stdout, pv[n]); printf("\n");}
      if (!SILENT) printf("[%8in, %.4fs : %9.2fnps || tt:hit=%i false=%i deep=%i shallow=%i used=%i]",
          nodes_searched, (double)(et-bt)*1e-6, 1e6*(double)nodes_searched/(double)(et-bt),
          tt_hit, tt_false, tt_deep, tt_shallow, tt_used
          );
      if (SILENT) {}//printf(".");
      else {printf("%i:%c", j, "NWB"[b.to_move]); show_move(stdout, pv[0]); printf(" {{%i}}\n", (int)val);}
      nodes_spent[b.to_move==white ? i1 : i2] += (double)nodes_searched;
      time_spent[b.to_move==white ? i1 : i2] += (double)(et-bt)*1e-6;
      make_move(&b, pv[0]);
      if (b.to_move==none) break;
      if (!SILENT) fflush(stdout);
    }
    double    q1 = pow(10.0, elo[i1]/400.0);
    double    q2 = pow(10.0, elo[i2]/400.0);
    ++num[i1];
    ++num[i2];
    num_moves[i1] += j;
    num_moves[i2] += j;
    if (b.pieces[white-1][king][0].captured) {
      ++w_r[i1];
      elo[i1] += KFACTOR*(1.0 - q1/(q1+q2));
      elo[i2] += KFACTOR*(0.0 - q2/(q1+q2));
    } else if (b.pieces[black-1][king][0].captured) {
      ++w_b[i2];
      elo[i1] += KFACTOR*(0.0 - q1/(q1+q2));
      elo[i2] += KFACTOR*(1.0 - q2/(q1+q2));
    } else {
      ++d_r[i1];
      ++d_b[i2];
      elo[i1] += KFACTOR*(0.5 - q1/(q1+q2));
      elo[i2] += KFACTOR*(0.5 - q2/(q1+q2));
    }
    //show_game(stdout, &b);

    {
      int    idx[num_players];
      int    k1, k2;
      for (k1=0; k1<num_players; ++k1) idx[k1] = k1;

      for (k1=0; k1<num_players; ++k1) {
        for (k2=k1+1; k2<num_players; ++k2) {
          if (elo[idx[k2]]<elo[idx[k1]]) {
            int t = idx[k1];
            idx[k1] = idx[k2];
            idx[k2] = t;
          }
        }
      }

      for (j=0; j<num_players; ++j) {
        printf("%4i (%3i) [%9in %5.2fs]", (int)elo[idx[j]], num[idx[j]],
            (int)(nodes_spent[idx[j]]/num_moves[idx[j]]), (time_spent[idx[j]]/num_moves[idx[j]]));
        printf("+%-3i/%-3i  =%-3i/%-3i", w_r[idx[j]], w_b[idx[j]], d_r[idx[j]], d_b[idx[j]]);
        printf("  ");
        show_settings(stdout, &player_settings[idx[j]]);
        show_evaluator(stdout, &player_evaluators[idx[j]]);
        printf("\n");
      }
      fflush(stdout);
    }
  }
  return 0;
#undef    NP
#undef    BASE
#undef    NROUND
#undef    KFACTOR
}

////////////////////////////////////////////////////////////////////////////////


int main(int argc, char* argv[]) {
  board b;
  int  i;
  evaluator e;

  setvbuf(stdout, NULL, _IONBF, 0);
  srand48(argc>1 ? atoi(argv[1]) : 131719);
  init_hash();
  simple_evaluator(&e);

  if (1) {
    movet ms[256];
    int vs[256];
    setup_board(&b);
    int n = gen_moves(&b, ms);
    show_board(stdout, &b);
    //dump_board(stdout, &b);
    evaluate_moves_for_search(&e, &b, ms, vs, n);
    for (i=0; i<n; ++i) {
      printf("%3i.", i);
      show_move_b(stdout, &b, ms[i]);
      printf("{%3i}", vs[i]);
      make_move(&b, ms[i]);
      printf("[%3i]", evaluate(&e, &b, 0));
      unmake_move(&b);
      printf((i%5)==4 ? "\n" : "  ");
    }
    printf("\n");
  }

  //random_moves();
  comparison();
  // tourny1();
  //tournament_2();

  return 0;
}
