/* $Id: jungle.c,v 1.19 2010/01/16 17:03:36 apollo Exp apollo $ */

/*
 * TODO:
 *  - stalemate handling
 *  - all pieces eaten handling (special case of above)
 *  - incremental hash computation
 *  - incremental evaluation
 *  - refactor for different players, evaluations, etc.
 *  - better interactive (undo, set depth, "En" or "Me" style moves: E^><v Enews Eurld Eklhj ?)
 *  * TDLeaf(lambda) - get piece values (piece-square values)
 *    (use new players, simple-to-complex players...)
 *
 * NEW PLAYERS:
 *  - random player
 *  - simple MC move ranker
 *  - UCB1 player
 *  - UCT player
 *  - epsilon-greedy
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "jungle-board.h"
#include "jungle-search.h"

/* The Jungle Game (Dou Shou Qi)
 * 
 * board is:
 *
 *  ..*#*..
 *  ...*...
 *  .......
 *  .==.==.
 *  .==.==.
 *  .==.==.
 *  .......
 *  ...*...
 *  ..*#*..
 *
 * Pieces (in descending order of strength):
 *  Elephant, Tiger, Lion, Panther, Wolf, Dog, Cat, Rat
 *
 * with starting positions:
 *  l.*#*.t
 *  .d.*.c.
 *  r.p.w.e
 *  .==.==.
 *  .==.==.
 *  .==.==.
 *  E.W.P.R
 *  .C.*.D.
 *  T.*#*.L
 * 
 * Basic rules:
 *  - pieces move one step orthogonally
 *  - pieces capture any animal of equal or lesser
 *    strength and rat can capture elephant
 *  - only rat can enter water, cannot capture crossing in or out of water
 *  - lions and tigers jump over water (not over any rat) and can capture so
 *  - any piece on enemy's trap can be captured by any animal
 *
 * variant rules:
 *
 */

/* ************************************************************ */
int  test_td_lambda();
int  simple_monte_carlo();
int  tournament_1();
int  tournament_2();
int  interactive_play();
int  nodes_table();

int main(int argc, char* argv[]) {
  srand48(-131313);
  init_hash();

  if (0)
  {
    board  b;
    char  buff[128];
    init_board(&b);
    show_board(stdout, &b);
    board_to_fen(&b, buff);
    printf("%s\n", buff);
    fen_to_board(&b, buff);
    show_board(stdout, &b);
    return 0;
  }

  //test_td_lambda();
  //simple_monte_carlo();
  //tournament_1();
  //tournament_2();
  interactive_play();
  //nodes_table();

  return 0;
}

int nodes_table() {
  unsigned int  bt, et;
  int    depth;
  board  b;
  evalt  val;
  move  m;
  int  n;

  settings.depth = 100;
  settings.f_tt = 1;
  settings.f_hh = 1;
  settings.f_asp = 1;
  settings.asp_width = 5;
  settings.f_id = 1;
  settings.id_base = 200;
  settings.id_step = 200;
  settings.f_quiesce = 1;
  settings.qdepth = 400;
  settings.f_nmp = 1;
  settings.nmp_R1 = 300;
  settings.nmp_cutoff = 600;
  settings.nmp_R1 = 200;
  settings.evaluator = 1;
  printf(" --- "); show_settings(stdout, &settings); printf(" ---\n");
  for (depth=200; depth<=1300; depth+=100) {
  for (settings.f_nmp=0; settings.f_nmp<=1; ++settings.f_nmp) {
  for (settings.f_id=0; settings.f_id<=1; ++settings.f_id) {
  for (settings.f_hh=0; settings.f_hh<=1; ++settings.f_hh) {
  for (settings.f_asp=0; settings.f_asp<=1; ++settings.f_asp) {
    //if (depth<=200 && settings.f_id) continue;
    //if (depth<=300 && settings.f_asp) continue;
    if (depth>=800 && !settings.f_hh) continue;
    if (depth>=800 && !settings.f_id) continue;
    printf("%2i ", depth/100);
    printf("%s ", settings.f_nmp ? "NM" : "  ");
    printf("%s ", settings.f_id ? "ID" : "  ");
    printf("%s ", settings.f_hh ? "HH" : "  ");
    printf("%s ", settings.f_asp ? "ASP" : "   ");
    settings.depth = depth;
    settings.id_base = 100 + (depth%200);
    init_board(&b);
    nodes_evaluated = 0;
    tt_hit = tt_false = tt_deep = tt_shallow = tt_used = 0;
    init_ttable();
    bt = read_clock();
    m = search5(&b, &val);
    et = read_clock();
    //for (n=0; n<pv_length[0]; ++n) show_move(stdout, pv[0][n]); printf("\n");
    //printf("[%8in, %.4fs : %9.2fnps || tt:hit=%i false=%i deep=%i shallow=%i used=%i]",
    //  nodes_evaluated, (double)(et-bt)*1e-6, 1e6*(double)nodes_evaluated/(double)(et-bt),
    //  tt_hit, tt_false, tt_deep, tt_shallow, tt_used
    //  );
    printf("[%8in %8.4fs %8.2fknps]", nodes_evaluated, (double)(et-bt)*1e-6, nodes_evaluated/((double)(et-bt)*1e-3));
    printf("\n");
  }}}}}

  return 0;
}

int  test_td_lambda()
{
  board  b;
  game_move  gml[512];
  int  n_moves;
  int  xx, i;
  eval_params  epin, epout;

  init_board(&b);

    setup_default_eval_params(&eparams);

    settings.depth = 400;
    settings.f_tt = 1;
    settings.f_id = 1;
    settings.f_quiesce = 1;
    settings.qdepth = 200;
    settings.f_nmp = 1;
    settings.nmp_R1 = 300;
    settings.nmp_cutoff = 600;
    settings.nmp_R1 = 200;
    settings.evaluator = 2;

    printf("# "); show_settings(stdout, &settings); printf("\n");

    printf("# pieces*8 trap\n");
    for (xx=0; xx<1000; ++xx) {
      init_hash();
      colort res = self_play_game(&n_moves, gml);

      /*
      for (i=0; i<n_moves; ++i) {
        if (i%2) printf("...");
        else printf("%2i.", 1+i/2);
        show_move(stdout, gml[i].move_made);
        printf(" [%+4i] {", (int)gml[i].evaluation);
        for (j=0; j<gml[i].n_pv; ++j) {
          show_move(stdout, gml[i].pv[j]);
        }
        printf(" }\n");
      }
      */

      epin = eparams;
      do_td_lambda_leaf(n_moves, gml, res, &epin, &epout);

      /*
      printf("pieces ");
      for (i=0; i<8; ++i) printf(" %8.2f", epout.piece_value[i]);
      printf("\n");
      printf("trap    %8.2f\n", epout.protect_trap_bonus);
      */

      for (i=0; i<8; ++i) printf("%-8.2f ", epout.piece_value[i]);
      printf("%-8.2f\n", epout.protect_trap_bonus);
      fflush(stdout);

      /*
      for (i=0; i<9; ++i) {
        for (j=0; j<7; ++j) { printf("%6.2f ", epout.piece_square_bonus[6][i*7+j]); }
        printf("\n");
      }
      */

      eparams = epout;
    }

    return 0;
}

int simple_monte_carlo()
{
  board  b;
  int  w_wins=0, b_wins=0, draws=0;
  init_board(&b);
  int  i, j, n, d;
  move  ml[64];

    for (i=0; i<1000; ++i) {
      init_hash();
      init_board(&b);
      b.pieces[0] = empty;
      b.colors[0] = none;
      b.pieces[6] = empty;
      b.colors[6] = none;
      for (j=0; j<512; ++j) {
        n = gen_moves(&b, ml);
        if (n==0) break;
        d = ((uint32)lrand48())%n;
        make_move(&b, ml[d]);
        if (b.side==none) break;
      }
      if (b.side==none) {
        if (b.xside==white) {
          ++w_wins;
          printf("+");
        } else if (b.xside==black) {
          ++b_wins;
          printf("-");
        } else {
          ++draws;
          printf("=");
        }
      } else {
        ++draws;
        printf("0");
      }
      fflush(stdout);
    }
    printf("white=%i black=%i draw=%i\n", w_wins, b_wins, draws);
    return 0;
}

int tournament_1() {
  board  b;
#define  MD  8
#define  BASE  3
#define  NROUND  30
  int  i, j, n;
  int  d1, d2, i1, i2;
  int  w1[MD][MD], w2[MD][MD], dr[MD][MD];
  search_settings  loc_settings[4];
  search_settings  settings1, settings2;
  init_board(&b);
  memset(loc_settings, '\x00', sizeof(loc_settings));

//{ int depth int f_tt; int f_id; int f_quiesce; int qdepth;
//  int f_nmp; int nmp_R1; int nmp_R2; int nmp_cutoff; }

    loc_settings[0].f_tt = 1;
    loc_settings[0].f_id = 1;
    loc_settings[0].f_quiesce = 0;
    loc_settings[0].f_nmp = 0;

    loc_settings[1].f_tt = 1;
    loc_settings[1].f_id = 1;
    loc_settings[1].f_quiesce = 1;
    loc_settings[1].qdepth = 400;
    loc_settings[1].f_nmp = 0;

    loc_settings[2].f_tt = 1;
    loc_settings[2].f_id = 1;
    loc_settings[2].f_quiesce = 0;
    loc_settings[2].f_nmp = 1;
    loc_settings[2].nmp_R1 = 300;
    loc_settings[2].nmp_cutoff = 600;
    loc_settings[2].nmp_R2 = 200;

    loc_settings[3].f_tt = 1;
    loc_settings[3].f_id = 1;
    loc_settings[3].f_quiesce = 1;
    loc_settings[3].qdepth = 400;
    loc_settings[3].f_nmp = 1;
    loc_settings[3].nmp_R1 = 300;
    loc_settings[3].nmp_cutoff = 600;
    loc_settings[3].nmp_R2 = 200;

    for (i1=0; i1<MD; ++i1) {
      for (i2=0; i2<MD; ++i2) {
        w1[i1][i2] = w2[i1][i2] = dr[i1][i2] = 0;
        d1 = 100*(BASE+i1/4);
        settings1 = loc_settings[i1%4];
        settings1.depth = d1;
        d2 = 100*(BASE+i2/4);
        settings2 = loc_settings[i2%4];
        settings2.depth = d2;
        printf("********************************************************************************\n");
        printf("********************************************************************************\n");
        printf("** "); show_settings(stdout, &settings1);
        printf(" vs "); show_settings(stdout, &settings2);
        printf(" **\n");
        printf("********************************************************************************\n");
        for (i=0; i<NROUND; ++i) {
          if(!SILENT)printf("--------------------------------------------------------------------------------\n");
          init_hash();
          init_board(&b);
          for (j=0; j<512; ++j) {
            unsigned int  bt, et;
            evalt  val;
            move  m;
            if (b.side==white) {
              settings = settings1;
            } else if (b.side==black) {
              settings = settings2;
            } else {
              break;
            }
            nodes_evaluated = 0;
            tt_hit = tt_false = tt_deep = tt_shallow = tt_used = 0;
            init_ttable();
            bt = read_clock();
            if (!SILENT) {printf(" --- "); show_settings(stdout, &settings); printf(" ---\n");}
            m = search5(&b, &val);
            et = read_clock();
            if (!SILENT) {for (n=0; n<pv_length[0]; ++n) show_move(stdout, pv[0][n]); printf("\n");}
            if (!SILENT) printf("[%8in, %.4fs : %9.2fnps || tt:hit=%i false=%i deep=%i shallow=%i used=%i]",
              nodes_evaluated, (double)(et-bt)*1e-6, 1e6*(double)nodes_evaluated/(double)(et-bt),
              tt_hit, tt_false, tt_deep, tt_shallow, tt_used
              );
            if (SILENT) printf(".");
            else {printf("%i:%c", j, "WBN"[b.side]); show_move(stdout, m); printf(" {{%i}}\n", (int)val);}
            m = make_move(&b, m);
            if (b.side==none) break;
            if (check3rep(&b)) {
              printf("\n!!!!! 3-time repetition not caught properly! !!!!!!\n");
              break;
            }
            if (!SILENT) fflush(stdout);
          }
          if (b.side==none && b.xside==white) {
            ++w1[i1][i2];
            printf(">>>> Red Wins <<<<\n");
          } else if (b.side==none && b.xside==black) {
            ++w2[i1][i2];
            printf(">>>> Blue Wins <<<<\n");
          } else {
            ++dr[i1][i2];
            printf(">>>> Draw <<<<\n");
          }
          show_game(stdout, &b);
        }
      }
    }
    printf("         ");
    for (i2=0; i2<MD; ++i2) 
      printf("%4i%4s    ", 100*(BASE+i2/4),
        (i2%4==3 ? "(NQ)" : i2%4==2 ? "(N)" : i2%4==1 ? "(Q)" : "   "));
    printf("\n");
    for (i1=0; i1<MD; ++i1) {
      printf("%4i%4s ", 100*(BASE+i1/4),
        (i1%4==3 ? "(NQ)" : i1%4==2 ? "(N)" : i1%4==1 ? "(Q)" : "   "));
      for (i2=0; i2<MD; ++i2) {
        printf("(+%-2i-%-2i=%-2i) ", w1[i1][i2], w2[i1][i2], dr[i1][i2]);
      }
      printf("\n");
    }
    return 0;
#undef  MD
#undef  BASE
#undef  NROUND
}

int tournament_2() {
  board  b;
#define  NP  32
#define  BASE  3
#define  NROUND  1000
#define  KFACTOR  (50.0 - ((double)i/(double)NROUND)*40.0)
//#define  KFACTOR  32.0
  int  i, j, n, i1, i2;
  int  num_players = 0;
  int  w_r[NP], w_b[NP], d_r[NP], d_b[NP], num[NP];
  double  elo[NP];
  search_settings  player_settings[NP];
  search_settings  settings1, settings2;
  memset(player_settings, '\x00', sizeof(player_settings));
  init_board(&b);

  {
    search_settings  loc_settings[4];
    memset(loc_settings, '\x00', sizeof(loc_settings));
    loc_settings[1].f_tt = 1;
    loc_settings[1].f_id = 1;
    loc_settings[1].id_base = 100;
    loc_settings[1].id_step = 100;
    loc_settings[1].f_quiesce = 0;
    loc_settings[1].f_nmp = 0;

    loc_settings[3].f_tt = 1;
    loc_settings[3].f_id = 1;
    loc_settings[3].id_base = 100;
    loc_settings[3].id_step = 100;
    loc_settings[3].f_quiesce = 1;
    loc_settings[3].qdepth = 400;
    loc_settings[3].f_nmp = 0;

    loc_settings[0].f_tt = 1;
    loc_settings[0].f_id = 1;
    loc_settings[0].id_base = 100;
    loc_settings[0].id_step = 100;
    loc_settings[0].f_quiesce = 0;
    loc_settings[0].f_nmp = 1;
    loc_settings[0].nmp_R1 = 300;
    loc_settings[0].nmp_cutoff = 600;
    loc_settings[0].nmp_R2 = 200;

    loc_settings[2].f_tt = 1;
    loc_settings[2].f_id = 1;
    loc_settings[2].id_base = 100;
    loc_settings[2].id_step = 100;
    loc_settings[2].f_quiesce = 1;
    loc_settings[2].qdepth = 400;
    loc_settings[2].f_nmp = 1;
    loc_settings[2].nmp_R1 = 300;
    loc_settings[2].nmp_cutoff = 600;
    loc_settings[2].nmp_R2 = 200;

    for (i=0; i<NP; ++i) {
      player_settings[num_players] = loc_settings[i%4];
      player_settings[num_players].depth = 100 * (BASE + i/8);
      player_settings[num_players].evaluator = (i/4)%2;
      if (player_settings[num_players].depth>300 || player_settings[num_players].f_nmp==0)
        ++num_players;
    }
    /* ... */
    if (0) {
    num_players = 0;
    for (i=0; i<NP; ++i) {
      player_settings[num_players] = loc_settings[3];
      player_settings[num_players].depth = 100*BASE;
      player_settings[num_players].qdepth = i*100;
      player_settings[num_players].evaluator = 0;
      ++num_players;
    }
    }
  }
  for (i=0; i<num_players; ++i) {
    elo[i] = 1500.0;
    w_r[i] = w_b[i] = d_r[i] = d_b[i] = num[i] = 0;
  }

  for (i=0; i<NROUND; ++i) {
    do {
      i1 = abs(lrand48()) % num_players;
      i2 = abs(lrand48()) % num_players;
    } while (i1==i2);

    settings1 = player_settings[i1];
    settings2 = player_settings[i2];
    printf("** "); show_settings(stdout, &settings1);
    printf(" vs "); show_settings(stdout, &settings2);
    printf(" **\n");
    init_hash();
    init_board(&b);
    for (j=0; j<256; ++j) {
      unsigned int  bt, et;
      evalt  val;
      move  m;
      if (b.side==white) {
        settings = settings1;
      } else if (b.side==black) {
        settings = settings2;
      } else {
        break;
      }
      nodes_evaluated = 0;
      tt_hit = tt_false = tt_deep = tt_shallow = tt_used = 0;
      init_ttable();
      bt = read_clock();
      if (!SILENT) {printf(" --- "); show_settings(stdout, &settings); printf(" ---\n");}
      m = search5(&b, &val);
      et = read_clock();
      if (!SILENT) {for (n=0; n<pv_length[0]; ++n) show_move(stdout, pv[0][n]); printf("\n");}
      if (!SILENT) printf("[%8in, %.4fs : %9.2fnps || tt:hit=%i false=%i deep=%i shallow=%i used=%i]",
        nodes_evaluated, (double)(et-bt)*1e-6, 1e6*(double)nodes_evaluated/(double)(et-bt),
        tt_hit, tt_false, tt_deep, tt_shallow, tt_used
        );
      if (SILENT) {}//printf(".");
      else {printf("%i:%c", j, "WBN"[b.side]); show_move(stdout, m); printf(" {{%i}}\n", (int)val);}
      m = make_move(&b, m);
      if (b.side==none) break;
      if (check3rep(&b)) {
        printf("\n!!!!! 3-time repetition not caught properly! !!!!!!\n");
        break;
      }
      if (!SILENT) fflush(stdout);
    }
    double  q1 = pow(10.0, elo[i1]/400.0);
    double  q2 = pow(10.0, elo[i2]/400.0);
    ++num[i1];
    ++num[i2];
    if (b.side==none && b.xside==white) {
      //printf(">>>> Red Wins <<<<\n");
      ++w_r[i1];
      elo[i1] += KFACTOR*(1.0 - q1/(q1+q2));
      elo[i2] += KFACTOR*(0.0 - q2/(q1+q2));
    } else if (b.side==none && b.xside==black) {
      //printf(">>>> Blue Wins <<<<\n");
      ++w_b[i2];
      elo[i1] += KFACTOR*(0.0 - q1/(q1+q2));
      elo[i2] += KFACTOR*(1.0 - q2/(q1+q2));
    } else {
      //printf(">>>> Draw <<<<\n");
      ++d_r[i1];
      ++d_b[i2];
      elo[i1] += KFACTOR*(0.5 - q1/(q1+q2));
      elo[i2] += KFACTOR*(0.5 - q2/(q1+q2));
    }
    show_game(stdout, &b);

    {
      int  idx[num_players];
      int  k1, k2;
      for (k1=0; k1<num_players; ++k1) idx[k1] = k1;

      for (k1=0; k1<num_players; ++k1) {
        for (k2=k1+1; k2<num_players; ++k2) {
          if (elo[idx[k2]]<elo[idx[k1]]) {
            int  t = idx[k1];
            idx[k1] = idx[k2];
            idx[k2] = t;
          }
        }
      }

      for (j=0; j<num_players; ++j) {
        printf("%4i (%3i) ", (int)elo[idx[j]], num[idx[j]]);
        printf("+%-3i/%-3i  =%-3i/%-3i", w_r[idx[j]], w_b[idx[j]], d_r[idx[j]], d_b[idx[j]]);
        printf("  ");
        show_settings(stdout, &(player_settings[idx[j]]));
        printf("\n");
      }
      fflush(stdout);
    }
  }
  return 0;
#undef  NP
#undef  BASE
#undef  NROUND
#undef  KFACTOR
}
  

int interactive_play()
{
  board  b;
  int  i, j, n;
  //int  depths[2] = {800,1200};
  int  depths[2] = {600,800};
  move  ml[128];
  int  interactive = 0;
  int  comp_move = 1;
  int  comp_method = -3;
  char buff[1024];

  init_board(&b);

  if (interactive) {
    printf("\033[2J"); /* clear screen */
    printf("\033[1;1H"); /* move cursor to upper-left corner */
  }

  for(i=0; ; ++i) {
    unsigned int bt, et;
    move  m;
    evalt  val;

    if (interactive) {
      printf("\033[2J"); /* clear screen */
      printf("\033[1;1H"); /* move cursor to upper-left corner */
    }

    show_board(stdout, &b);

    if (interactive)
    {
      int  tm=-1;
      n = gen_cap_moves(&b, ml);
      for (j=0; j<n; ++j) {
        printf("  %2i!", j+1); show_move(stdout, ml[j]);
        if (j%5==4) printf("\n");
      }
      printf("\n");
      n = gen_moves(&b, ml);
      for (j=0; j<n; ++j) {
        printf("  %2i:", j+1); show_move(stdout, ml[j]);
        if (j%5==4) printf("\n");
      }
      if (j%5) printf("\n");
      printf(">> ");
      fgets(buff, sizeof(buff), stdin);
      sscanf(buff, "%i", &tm);
      if (tm>=1 && tm<=n) {
        comp_move = 0;
        m = ml[tm-1];
      } else {
        comp_move = 1;
      }
    }

    if (comp_move)
    {
      //for (comp_method=0; comp_method>=-3; --comp_method)
      {
      nodes_evaluated = 0;
      tt_hit = tt_false = tt_deep = tt_shallow = tt_used = 0;
      init_ttable();
      bt = read_clock();
      switch (comp_method) {
      case -3:
        settings.f_id = 1;
        settings.id_base = 200;
        settings.id_step = 200;
        settings.f_tt = 1;
        settings.f_nmp = 1;
        settings.nmp_R1 = 300;
        settings.nmp_cutoff = 400;
        settings.nmp_R2 = 200;
        settings.depth = depths[b.side];
        settings.f_quiesce = 1;
        settings.qdepth = 400;
        settings.evaluator = 0;
        printf(" --- "); show_settings(stdout, &settings); printf(" ---\n");
        m = search5(&b, &val);
        break;
      case -2:
        printf(" --- Alpha-beta(tt+id++/ANMP)(%i) ---\n", depths[b.side]);
        //m = search4ttx_anmp(&b, depths[b.side], &val);
        break;
      case -1:
        printf(" --- Alpha-beta(tt+id++/NMP)(%i) ---\n", depths[b.side]);
        //m = search4ttx_nmp(&b, depths[b.side], &val);
        break;
      case 0:
        printf(" --- Alpha-beta(tt+id++)(%i) ---\n", depths[b.side]);
        //m = search4ttx(&b, depths[b.side], &val);
        break;
      case 1:
        printf(" --- Alpha-beta(tt+id)(%i) ---\n", depths[b.side]);
        //m = search4tt(&b, depths[b.side], &val);
        break;
      case 2:
        printf(" --- Alpha-beta(tt)(%i) ---\n", depths[b.side]);
        //m = search3tt(&b, depths[b.side], &val);
        break;
      case 3:
        printf(" --- Alpha-beta(id)(%i) ---\n", depths[b.side]);
        //m = search4(&b, depths[b.side], &val);
        break;
      case 4:
        printf(" --- Alpha-beta(simple)(%i) ---\n", depths[b.side]);
        m = search3(&b, depths[b.side], &val);
        break;
      case 5:
        printf(" --- Negamax(%i) ---\n", depths[b.side]);
        m = search2(&b, depths[b.side], &val);
        break;
      case 6:
        printf(" --- Minmax(%i) ---\n", depths[b.side]);
        m = search1(&b, depths[b.side], &val);
        break;
      }
      et = read_clock();
      for (n=0; n<pv_length[0]; ++n) show_move(stdout, pv[0][n]); printf("\n");
      printf("[%8in, %.4fs : %9.2fnps || tt:hit=%i false=%i deep=%i shallow=%i used=%i] %i:%c",
        nodes_evaluated, (double)(et-bt)*1e-6, 1e6*(double)nodes_evaluated/(double)(et-bt),
        tt_hit, tt_false, tt_deep, tt_shallow, tt_used,
        i, "WBN"[b.side]
        );
      show_move(stdout, m); printf(" {{%i}}\n", (int)val);
      fflush(stdout);
      }

      {
        int  ll, cnt=0;
        for (ll=0; ll<NTT; ++ll) {
          if (ttable[ll].flags&tt_valid)
            ++cnt;
        }
        printf("***** %i out of %i TT entries 'valid' *****\n", cnt, NTT);
      }

      if (interactive) {
        printf(">> ENTER TO CONTINUE <<\n");
        fgets(buff, sizeof(buff), stdin);
      }
    }
    m = make_move(&b, m);
    if (b.side==none) break;
    if (check3rep(&b)) {
      printf("\n!!!!! 3-time repetition not caught properly! !!!!!!\n");
      break;
    }
  }
  if (interactive) {
    printf("\033[2J"); /* clear screen */
    printf("\033[1;1H"); /* move cursor to upper-left corner */
  }
  show_board(stdout, &b); printf("\n");

  return 0;
}
