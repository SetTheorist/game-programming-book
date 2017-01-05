#include <locale.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "board.h"
#include "player.h"
#include "search.h"
#include "td-lambda.h"
#include "wb.h"

/* ************************************************************ */

int in_winboard = 0;

#ifndef WINBOARD_ONLY
#define WINBOARD_ONLY 0
#endif

/* ************************************************************ */

static ui64 perft_general(board* b, int depth)
{
  if (terminal(b) || (depth<=0)) {
    return 1;
  }
  
  ui64 count = 0;
  movet ml[MAXNMOVES];
  int i, n = gen_moves(b, ml);
  for (i=0; i<n; ++i) {
    int mm = make_move(b, ml[i]);
    if (mm == invalid_move) continue;
    count += perft_general(b, depth-100);
    unmake_move(b);
  }
  return count;
}

int perft(int max_depth)
{
  board b;
  int depth;
  init_board(&b);
  for (depth=0; depth<=max_depth; depth+=100)
  {
    unsigned int tb = read_clock();
    ui64 count = perft_general(&b, depth);
    unsigned int te = read_clock();
    unsigned int tt = te - tb;
    printf("Depth=%5i  Nodes=%'15llu  Time=%9.2fs  NPS=%'11i\n",
        depth, count, tt*1e-6, (int)(count/(tt*1e-6)));
  }
  return 0;
}

typedef struct thread_data {
  ui64 count;
  board b;
  int depth;
  int num;
  int id;
} thread_data;

void * thread_func(void* arg)
{
  thread_data* data = (thread_data*)arg;
  board* b = &data->b;
  if (data->depth == 0)
  {
    data->count = (data->id==0);
    return NULL;
  }
  data->count = 0;
  movet ml[MAXNMOVES];
  int i, n = gen_moves(b, ml);
  for (i=0; i<n; ++i)
  {
    if ((i%data->num) != data->id) continue;
    int mm = make_move(b, ml[i]);
    if (mm == invalid_move) continue;
    data->count += perft_general(b, data->depth - 100);
    unmake_move(b);
  }
  return NULL;
}

int perft_threaded(int max_depth, int num_threads)
{
  thread_data args[num_threads];
  pthread_t threads[num_threads];
  pthread_attr_t attr;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  int i;
  for (i=0; i<num_threads; ++i)
  {
    args[i].num = num_threads;
    args[i].id = i;
    init_board(&args[i].b);
  }

  int depth;
  for (depth=0; depth<=max_depth; depth+=100)
  {
    unsigned int tb = read_clock();
    for (i=0; i<num_threads; ++i)
      args[i].depth = depth;

    for (i=0; i<num_threads; ++i)
      if (pthread_create(&threads[i], &attr, thread_func, (void*)&args[i]))
        fprintf(stderr, "Creating thread %i failed!", i);

    for (i=0; i<num_threads; ++i)
      if (pthread_join(threads[i], NULL))
        fprintf(stderr, "Joining thread %i failed!", i);

    ui64 count = 0;
    for (i=0; i<num_threads; ++i)
      count += args[i].count;

    unsigned int te = read_clock();
    unsigned int tt = te - tb;
    printf("Depth=%5i  Nodes=%'15llu  Time=%9.2fs  NPS=%'11i\n",
        depth, count, tt*1e-6, (int)(count/(tt*1e-6)));
  }
  return 0;
}

int depth_chart(int max_depth)
{
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
  for (i=0; i<n; ++i)
  {
    show_move(stdout, ml[i]);
    int mm = make_move(&b, ml[i]);
    if (mm==invalid_move) printf("X");
    else unmake_move(&b);
    printf(" ");
  }
  printf("\n********** **********\n");
  for (i=0; i<10; ++i)
  {
    n = gen_moves(&b, ml);
    do { d = lrand48() % n; } while (make_move(&b, ml[d]) == invalid_move);
    show_move(stdout, ml[d]);
  }
  fprintf(stdout, "\n");
  show_board(stdout, &b);
  printf("\n********** **********\n");
  
  for (d=100; d<=max_depth; d+=100)
  {
    printf("\n----- DEPTH = %i -----\n", d);
    for (ftt=1; ftt<=5; ++ftt)
    {
      switch (ftt)
      {
        case 1: make_simple_alphabeta_searcher(&ss, d); break;
        case 2: make_simple_alphabeta_searcher(&ss, d);
          ss.f_id=1; ss.id_base=100; ss.id_step=100;
          ss.f_nmp = 1; ss.nmp_R1 = 200; ss.nmp_R2 = 100; ss.nmp_cutoff = 300;
          ss.qdepth = 200;
          ss.asp_width = 20;
          break;
        case 3: make_simple_alphabeta_searcher(&ss, d);
          ss.f_id=1; ss.id_base=100; ss.id_step=100;
          ss.f_nmp = 1; ss.nmp_R1 = 200; ss.nmp_R2 = 100; ss.nmp_cutoff = 300;
          ss.qdepth = 200;
          ss.asp_width = 20;
          ss.f_lmr = 1;
          break;
        case 4: make_simple_alphabeta_searcher(&ss, d);
          ss.f_id=1; ss.id_base=100; ss.id_step=100;
          ss.f_nmp = 1; ss.nmp_R1 = 200; ss.nmp_R2 = 100; ss.nmp_cutoff = 300;
          ss.qdepth = 1000;
          ss.asp_width = 20;
          break;
        case 5: make_simple_alphabeta_searcher(&ss, d);
          ss.f_id=1; ss.id_base=100; ss.id_step=100;
          ss.f_nmp = 1; ss.nmp_R1 = 200; ss.nmp_R2 = 100; ss.nmp_cutoff = 300;
          ss.qdepth = 1000;
          ss.asp_width = 20;
          ss.f_lmr = 1;
          break;
      }
      printf("\n");
      show_settings(stdout, &ss); printf("\n");
      ttable_init();
      init_board(&b);
      unsigned int tb = read_clock();
      hasht bh = b.h;
      evalt res = search(&b, &ss, &e, 0, pv, &pvl);
      hasht eh = b.h;
      if (bh!=eh) printf("!!! %016llX != %016llX !!!\n", bh, eh);
      unsigned int te = read_clock();
      unsigned int tt = te - tb;
      //show_evaluator(stdout, &e);
      printf("    <<%10i>> %'12llun  %8.2fms  %'9.1fknps",
             (int)res, (nodes_evaluated + qnodes_evaluated), tt*1e-3, (nodes_evaluated + qnodes_evaluated)/(tt*1e-3));
      fstatus(stdout, "\n    [ %M ]\n", pv, pvl);
    }
  }
  return 0;
}

int random_moves()
{
  int n_moves = 0, max_moves = 0, max_caps = 0;
  int game_len = 200;
  int xx;
  ansi_cursor_position(stdout, 1, 1);
  ansi_clear_to_eos(stdout);
  for (xx=0; xx<10; ++xx)
  {
    board b;
    int i, j, n, c;
    movet ml[MAXNMOVES];
    init_board(&b);
    for (i=0; i<game_len; ++i)
    {
      ansi_cursor_position(stdout, 1, 1);
      //ansi_clear_to_eos(stdout);
      show_board(stdout, &b);
      n = gen_moves(&b, ml);
      for (c=j=0; j<n; ++j) if (ml[j].capture) ++c;
      if (c>max_caps) max_caps = c;
      if (0) for (j=0; j<n; ++j)
      {
        show_move(stdout, ml[j]);
        printf("  ");
        if ((j%7)==6) printf("\n");

      }
      printf("\n");
      int j = lrand48()%n;
      make_move(&b, ml[j]);
      validate_board(&b);
      //fgets(buff, 256, stdin);
      n_moves += n;
      if (n > max_moves) max_moves = n;
    }
    ansi_cursor_position(stdout, 1, 1);
    ansi_clear_to_eos(stdout);
    show_board(stdout, &b);
    for (--i; i>=0; --i)
    {
      unmake_move(&b);
      ansi_cursor_position(stdout, 1, 1);
      //ansi_clear_to_eos(stdout);
      show_board(stdout, &b);
    }
  }
  printf("Average # moves: %i\n", n_moves/(game_len*xx));
  printf("Maximum # moves: %i\n", max_moves);
  printf("Maximum # captures: %i\n", max_caps);

  return 0;
}

void match(
    int d1, int qd1, int d2, int qd2,
    td_lambda* tdl,
    const evaluator* e1, const evaluator* e2, const evaluator* work_e, evaluator* new_e)
{
  player p1, p2;
  time_control tc1, tc2;
  game_record gr;

  memset(&tc1, '\x00', sizeof(time_control));
  tc1.remaining_cs  =  100000;
  tc1.allocated_cs  =  100000;
  tc1.panic_cs      = 1000000;
  tc1.force_stop_cs = 1000000;
  tc1.moves_remaining = 1;
  memset(&tc2, '\x00', sizeof(time_control));
  tc2.remaining_cs  =  100000;
  tc2.allocated_cs  =  100000;
  tc2.panic_cs      = 1000000;
  tc2.force_stop_cs = 1000000;
  tc2.moves_remaining = 1;

  sprintf(p1.description, "AB%ik", d1);
  p1.mover = search_mover;
  p1.e = *e1;
  make_simple_alphabeta_searcher(&p1.ss, d1);
  p1.ss.qdepth = qd1;
  p1.ss.f_id = 1;
  p1.ss.id_base = 100;
  p1.ss.id_step = 100;
  p1.ss.f_nmp = 1;
  p1.ss.nmp_R1 = 200;
  p1.ss.nmp_R2 = 100;
  p1.ss.nmp_cutoff = 300;
  p1.ss.asp_width = 5;
  p1.ss.f_lmr = 1;
  p1.tc = tc1;

  if (1)
  {
    sprintf(p2.description, "AB%i", d2);
    p2.mover = search_mover;
    p2.e = *e2;
    make_simple_alphabeta_searcher(&p2.ss, d2);
    p2.ss.qdepth = qd2;
    p2.ss.f_id = 1;
    p2.ss.id_base = 100;
    p2.ss.id_step = 100;
    p2.ss.asp_width = 5;
    p2.ss.f_lmr = 1;
    p2.tc = tc2;
  }
  else
  {
    make_simple_alphabeta_searcher(&p2.ss, d2);
    strcpy(p2.description, "Human");
    p2.mover = interactive_mover;
  }

  play_match(&p1, &tc1, &p2, &tc2, &gr, 1, 1);
  //color res = play_match(&p2, &tc2, &p1, &tc1, &gr, 1, 1);

  show_game_record(stdout, &gr, 0);

  if (tdl) {
    printf("TD(lambda) processing beginning\n");
    td_lambda_process_game(&gr, tdl, work_e, new_e);
    printf("TD(lambda) processing completed\n");
  }
}

int analyze_position(const char* wbfen, int max_depth)
{
  board b;
  search_settings ss;
  evaluator e;
  movet pv[256];
  int pvl;
  
  simple_evaluator(&e);
  make_simple_alphabeta_searcher(&ss, max_depth);
  ss.f_id=1; ss.id_base=100; ss.id_step=100;
  ss.f_nmp = 1; ss.nmp_R1 = 200; ss.nmp_R2 = 100; ss.nmp_cutoff = 300;
  ss.qdepth = 200;
  ss.asp_width = 100;
  ss.f_lmr = 1;
  ttable_init();

  wbfen_to_board(&b, wbfen);
  printf("\n********** **********\n");
  show_board(stdout, &b);
  printf("\n********** **********\n");
  show_settings(stdout, &ss); printf("\n");

  ui64 tb = read_clock();
  evalt res = search(&b, &ss, &e, 0, pv, &pvl);
  ui64 te = read_clock();
  unsigned int tt = te - tb;
  printf("    <<%10i>> %'12llun  %8.2fms  %'9.1fknps",
         (int)res, (nodes_evaluated + qnodes_evaluated), tt*1e-3, (nodes_evaluated + qnodes_evaluated)/(tt*1e-3));
  fstatus(stdout, "\n    [ %M ]\n", pv, pvl);

  return 0;
}


int main(int argc, const char * const argv[])
{
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);
  setlocale(LC_ALL,"");

  if (0)
  {
    printf("sizeof(board)=%lu\n", sizeof(board));
    printf("sizeof(movet)=%lu\n", sizeof(movet));
    printf("sizeof(square)=%lu\n", sizeof(square));
    printf("sizeof(piece_info)=%lu\n", sizeof(piece_info));
    printf("sizeof(evaluator)=%lu\n", sizeof(evaluator));
    printf("sizeof(time_control)=%lu\n", sizeof(time_control));
    printf("sizeof(search_settings)=%lu\n", sizeof(search_settings));
    printf("sizeof(player)=%lu\n", sizeof(player));
    printf("sizeof(game_record_ply)=%lu\n", sizeof(game_record_ply));
    printf("sizeof(game_record)=%lu\n", sizeof(game_record));
    printf("sizeof(ttentry)=%lu\n", sizeof(ttentry));
    printf("sizeof(ttentry)*TTNUM=%lu\n", sizeof(ttentry)*TTNUM);
  }

  if (WINBOARD_ONLY || (argc>1 && strcmp(argv[1],"-winboard")==0))
    return wb_main(argc,argv);

  srand48(argc>1 ? atoi(argv[1]) : -1379);
  init_hash();

  if (0) {
    char buff[256];
    evaluator e;
    board b;
    init_board(&b);
    simple_evaluator(&e);
    show_board(stdout, &b);
    evalt ev = evaluate(&e, &b, 0, B_INF, W_INF);
    printf("Evaluation = %i\n", (int)ev);
    board_to_fen(&b, buff); printf("%s\n", buff);
    movet ml[MAXNMOVES];
    int n = gen_moves(&b, ml);
    int j = lrand48()%n;
    make_move(&b, ml[j]);
    show_board(stdout, &b);
    ev = evaluate(&e, &b, 0, B_INF, W_INF);
    printf("Evaluation = %i\n", (int)ev);
    board_to_fen(&b, buff); printf("%s\n", buff);
    return 0;
  }
  //random_moves();

  if (argc>5)
  {
    const char* wbfen = "11l/5xc1t2a/6m1m2c/5p1r1k2/6+O1pegp/7pi3/12/5H3XN1/12/9R2/12/K11 w - 0 1";
    analyze_position(wbfen, 2000);
  }
  else if (argc>4)
  {
    int num_threads = 3;
    if (atoi(argv[4]) > 1) num_threads = atoi(argv[4]);
    perft_threaded(2000, num_threads);
  }
  else if (argc>3)
  {
    perft(2000);
  }
  else if (argc>2)
  {
    depth_chart(2000);
  }
  else
  {
    evaluator orig_e, work_e, new_e;
    td_lambda tdl;

    simple_evaluator(&orig_e);
    {
      FILE* fe = fopen("evaluator-simple.txt", "w");
      if (fe)
      {
        save_evaluator(fe, &orig_e);
        fclose(fe);
      }
    }
    {
      FILE* fe = fopen("evaluator.txt", "r");
      if (fe)
      {
        load_evaluator(fe, &orig_e);
        fclose(fe);
      }
    }

    memcpy(&work_e, &orig_e, sizeof(evaluator));

    FILE* f_weights = fopen("weights.txt", "w");
    //show_evaluator_pieces_only(f_weights, &orig_e); fprintf(f_weights, "\n"); fflush(f_weights);
    show_evaluator(f_weights, &orig_e); fprintf(f_weights, "\n"); fflush(f_weights);

    init_td_lambda(&tdl, sqrt(0.99), 1e-3, 100.0, 0.1, 1);
    // tune _only_ material values
    //memset(tdl.tune_flag, '\x00', sizeof(tdl.tune_flag));
    //memset(tdl.tune_flag, '\x01', num_piece*2*sizeof(*tdl.tune_flag));

    int i, j;
    //for (i=0; i<12*100; ++i)
    for (i=0; i<3*100; ++i)
    {
      //init_td_lambda(&tdl, 0.9000, 1e-3, 50.0 - i/24.0, 0.1, 0);

      evaluator rand_e;
      memcpy(&rand_e, &orig_e, sizeof(evaluator));
      for (j=0; j<NUM_WEIGHTS; ++j) rand_e.weights[j] += (drand48()*11 - 5)/100.0;

      init_hash();

      switch (i%3)//(i%12)
      {
        case 0:
          match(200, 200, 200, 200, &tdl, &rand_e, &work_e, &work_e, &new_e); break;
        case 1:
          match(200, 200, 200, 200, &tdl, &work_e, &rand_e, &work_e, &new_e); break;
        case 2:
          match(200, 200, 200, 200, &tdl, &work_e, &work_e, &work_e, &new_e); break;

        //case 0:
        //  match(300, 200, 300, 200, &tdl, &rand_e, &work_e, &work_e, &new_e); break;
        //case 1:
        //  match(300, 200, 300, 200, &tdl, &work_e, &rand_e, &work_e, &new_e); break;
        //case 2:
        //  match(300, 200, 300, 200, &tdl, &work_e, &work_e, &work_e, &new_e); break;

        case 3:
          match(400, 200, 300, 200, &tdl, &rand_e, &work_e, &work_e, &new_e); break;
        case 4:
          match(400, 200, 300, 200, &tdl, &work_e, &rand_e, &work_e, &new_e); break;
        case 5:
          match(400, 200, 300, 200, &tdl, &work_e, &work_e, &work_e, &new_e); break;

        case 6:
          match(300, 200, 400, 200, &tdl, &rand_e, &work_e, &work_e, &new_e); break;
        case 7:
          match(300, 200, 400, 200, &tdl, &work_e, &rand_e, &work_e, &new_e); break;
        case 8:
          match(300, 200, 400, 200, &tdl, &work_e, &work_e, &work_e, &new_e); break;

        case 9:
          match(400, 200, 400, 200, &tdl, &rand_e, &work_e, &work_e, &new_e); break;
        case 10:
          match(400, 200, 400, 200, &tdl, &work_e, &rand_e, &work_e, &new_e); break;
        case 11:
          match(400, 200, 400, 200, &tdl, &work_e, &work_e, &work_e, &new_e); break;
      }

      printf("\n::: Original evaluation weights :::\n");
      show_evaluator(stdout, &orig_e);
      printf("\n::: Randomized evaluation weights :::\n");
      show_evaluator(stdout, &rand_e);
      printf("\n::: Evaluation weights after :::\n");
      show_evaluator(stdout, &new_e);
      printf("\n::: Evaluation weights renormalized :::\n");
      renormalize_evaluator(&new_e);
      show_evaluator(stdout, &new_e);

      memcpy(&work_e, &new_e, sizeof(evaluator));
      //show_evaluator_pieces_only(f_weights, &new_e); fprintf(f_weights, "\n"); fflush(f_weights);
      show_evaluator(f_weights, &new_e); fprintf(f_weights, "\n"); fflush(f_weights);
      {
        FILE* fl = fopen("td-lambda.txt", "w");
        show_td_lambda(fl, &tdl); fflush(fl);
        fclose(fl);
      }
      {
        FILE* fe = fopen("evaluator-new.txt", "w");
        save_evaluator(fe, &new_e);
        fclose(fe);
      }
    }
    show_td_lambda(stdout, &tdl);
    fclose(f_weights);

  }

  return 0;
}
