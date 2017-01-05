#include <pthread.h>
#include <regex.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "board.h"
#include "player.h"

/* ************************************************************ */
/* ************************************************************ */

/* Thread barriers:
 *
 * pthread_barrier_t barr;
 * if (pthread_barrier_init(&barr, NULL, num_threads)) error();
 * int rc = pthread_barrier_wait(&barr); //rc==0 or rc==PTHREAD_BARRIER_SERIAL_THREAD
 * ?? pthread_barrier_destry(&barr); ??
 */

/* Thread mutexes:
 *
 * pthread_mutex_t mutex;
 * if (pthread_mutex_inti(&mutex, NULL)) error();
 * pthread_mutex_lock(&mutex);
 * pthread_mutex_unlock(&mutex);
 * pthread_mutex_destroy(&mutex);
 */

/* Thread semaphores:
 * #include <semaphore.h>
 * sem_t semaphore;
 * if (sem_init(&semaphore, 0, 1)) error(); //(0=thread-shared, not process), 1 is initial-value
 * sem_wait(&semaphore); // decrement, block if negative, woken when posted
 * sem_post(&semaphore); // increment, wake waiter
 * sem_getvalue(&semaphore, &value);
 * sem_destroy(&semaphore);
 */

/* Thread condition vars:
 *
 * pthread_cond_t cond;
 * pthread_cond_init(&cond, NULL);
 * pthread_cond_wait(&cond, &mutex);
 * pthread_cond_broadcast(&cond);
 * pthread_cond_signal(&cond);
 * pthread_cond_destroy(&cond);
 */

/* ************************************************************ */
/* ************************************************************ */

extern int in_winboard;

typedef enum wb_worker_state {
  start_wbws=0, wait_wbws, work_wbws, stop_wbws
} wb_worker_state;

typedef struct wb_worker_workitem {
  char  move[64];
} wb_worker_workitem;

typedef struct wb_worker_data {
  int             id;
  wb_worker_state state;
  int             exit_flag;
  int             movenow_flag;
  int             force_flag;
  int             count;
  int             time;
  board           b;
  player          p;
  int             work_queue_num;
  wb_worker_workitem work_queue[16];
  pthread_mutex_t queue_mutex;
  pthread_cond_t  queue_cond;
  pthread_t       thread;
} wb_worker_data;
 
typedef enum wb_state {
  start_wbs       =0,     /* main */
  wait_wbs        =1,     /* main */
  think_wbs       =2,     /* worker */
  ponder_wbs      =4,     /* worker */
  ponder_done_wbs =8,     /* main */
  analyze_wbs     =16,    /* worker */
  analyze_done_wbs=32,    /* main */
  stop_wbs        =128    /* main */
} wb_state;

typedef struct wb_data_t {
  wb_state  state;
  int       time;
  int       otime;
  FILE*     flog;
  int       log_in_count;
  int       log_out_count;
  board           b;
  player          p;
  search_settings ss;
  evaluator       e;
  time_control    tc;
  wb_worker_data  worker;
} wb_data_t;
static wb_data_t wb_data;

static pthread_mutex_t send_mutex;
FILE* logfile()
{
#if 0
  static FILE* devnull = 0;
  if (!devnull) devnull = fopen("/dev/null", "w");
  return devnull;
#endif
#if 0
  return 0;
#endif
#if 1
  if (in_winboard)
    return wb_data.flog;
  else
    return stdout;
#endif
}
int send(const char* fmt, ...)
{
  if (in_winboard)
  {
    va_list args;

    pthread_mutex_lock(&send_mutex);

    fprintf(wb_data.flog, "%i:%i|%02x<", wb_data.log_in_count, ++wb_data.log_out_count, wb_data.state);

    va_start (args, fmt);
    vfprintf(wb_data.flog, fmt, args);
    va_end (args);

    va_start (args, fmt);
    vfprintf(stdout, fmt, args);
    va_end (args);

    pthread_mutex_unlock(&send_mutex);
  }
  else
  {
    va_list args;
    va_start (args, fmt);
    vfprintf(stdout, fmt, args);
    va_end (args);
  }
  return 0;
}
int sendlog(const char* fmt, ...)
{
  if (in_winboard)
  {
    va_list args;
    va_start (args, fmt);
    pthread_mutex_lock(&send_mutex);
    vfprintf(wb_data.flog, fmt, args);
    pthread_mutex_unlock(&send_mutex);
    va_end (args);
  }
  else
  {
    va_list args;
    va_start (args, fmt);
    vfprintf(stdout, fmt, args);
    va_end (args);
  }
  return 0;
}

static void* wb_worker(void* arg)
{
  wb_worker_data* data = (wb_worker_data*)arg;
  send("# Starting worker %i\n", data->id);
  pthread_mutex_lock(&data->queue_mutex);
  data->state = wait_wbws;
  while (data->state != stop_wbws)
  {
    while (data->state == wait_wbws)
      pthread_cond_wait(&data->queue_cond, &data->queue_mutex);
    pthread_mutex_unlock(&data->queue_mutex);

    data->state = work_wbws;
    send("# worker %i received work '%s' (%i)\n", data->id, data->work_queue[0].move, data->time);

    {
      movet m;
      movet pv[64];
      int   pvl;
      special_move sm;

      compute_time_for_move(&data->p.tc, &data->b);
      pthread_mutex_lock(&send_mutex);
      show_time_control(wb_data.flog, &data->p.tc); fprintf(wb_data.flog, "\n");
      pthread_mutex_unlock(&send_mutex);

      if (data->exit_flag) break;

      // Search for best move
      data->p.mover(&data->p, &data->b, pv, &pvl, &sm);
      m = pv[0];

      if (data->exit_flag) break;

      pthread_mutex_lock(&send_mutex);
      show_move(wb_data.flog, m); fprintf(wb_data.flog, "\n");
      pthread_mutex_unlock(&send_mutex);

      /* TODO!!!!! THIS NEEDS MUTEX ETC...  NEED TO COORDINATE
       *
       * maybe lock board, have tag with workitem, etc.
       */
      make_move(&wb_data.b, m);
      make_move(&data->b, m);

      char buff1[16], buff2[16];
      move_to_winboard(m, buff1, buff2);
      send("move %s\n", buff1);
      if (buff2[0]) send("move %s\n", buff2);

      pthread_mutex_lock(&send_mutex);
      show_board(wb_data.flog, &data->b);
      pthread_mutex_unlock(&send_mutex);
      if (1)
      {
        pthread_mutex_lock(&send_mutex);
        movet ml[MAXNMOVES];
        evalt scorel[MAXNMOVES];
        int k, n = gen_moves(&data->b, ml);
        evaluate_moves_for_search(&wb_data.e, &data->b, ml, scorel, n);
        for (k=0; k<n; ++k) {
          fstatus(wb_data.flog, "\t%m(%i)", ml[k], (int)scorel[k]);
          if ((k%5)==4) fprintf(wb_data.flog, "\n");
        }
        if (n%5) fprintf(wb_data.flog, "\n");
        pthread_mutex_unlock(&send_mutex);
      }

      if (terminal(&data->b))
      {
        if (!data->b.king_mask[white] && !data->b.prince_mask[white])
          send("0-1 {mate}\n");
        else if (!data->b.king_mask[black] && !data->b.prince_mask[black])
          send("1-0 {mate}\n");
        else
          send("1/2-1/2 {?}\n");
      }
    }

    data->state = data->exit_flag ? stop_wbws : wait_wbws;

    pthread_mutex_lock(&data->queue_mutex);
  }
  pthread_mutex_unlock(&data->queue_mutex);
  send("# Stopping worker %i\n", data->id);
  //pthread_exit(NULL);
  return NULL;
}

static int changed_default_evaluator_file = 0;
static char default_evaluator_file[1024] = "evaluator.txt";
static void wb_ioloop()
{
  char  buff[16*1024];
  int             max_depth = 2000;
  int             random = 0;
  int             random_seed = 137999;
  int             pondering = 0;
  int             post = 1;
  srand48(random_seed);
  ttable_init();
  init_hash();
  init_board(&wb_data.b);

  {
    FILE* fe = fopen(default_evaluator_file, "r");
    if (fe)
    {
      load_evaluator(fe, &wb_data.e);
      fclose(fe);
    }
    else
    {
      simple_evaluator(&wb_data.e);
    }
#if 0
    pthread_mutex_lock(&send_mutex);
    fprintf(wb_data.flog, "*** Evaluation parameters: ***\n");
    show_evaluator(wb_data.flog, &wb_data.e);
    fprintf(wb_data.flog, "*** ***\n");
    pthread_mutex_unlock(&send_mutex);
#endif
  }

  make_simple_alphabeta_searcher(&wb_data.ss, max_depth);
  wb_data.ss.f_id=1; wb_data.ss.id_base=100; wb_data.ss.id_step=100;
  wb_data.ss.f_nmp = 1; wb_data.ss.nmp_R1 = 200; wb_data.ss.nmp_R2 = 100; wb_data.ss.nmp_cutoff = 300;
  wb_data.ss.qdepth = 400; wb_data.ss.asp_width = 25;
  wb_data.ss.f_lmr = 1; // LMR seems to give bad results against HaChu... misses key moves too often (maybe?)
  wb_data.ss.f_razoring = 0; wb_data.ss.f_limrazoring = 0;
  wb_data.ss.f_futility = 0; wb_data.ss.f_extfutility = 0;

  while ((wb_data.state != stop_wbs) && fgets(buff, sizeof(buff)-1, stdin))
  {
    buff[sizeof(buff)-1] = '\x00';

    pthread_mutex_lock(&send_mutex);
    fprintf(wb_data.flog, "%i:%i|%02X>%s", ++wb_data.log_in_count, wb_data.log_out_count, wb_data.state, buff);
    pthread_mutex_unlock(&send_mutex);

    int handled = 0;

    if (strncmp(buff, "xboard", 6)==0)
    {
      handled = 1;
    }
    else if (strncmp(buff, "protover", 8)==0)
    {
      handled = 1;
      send("done=%i\n", 0);
      send("feature ping=%i\n", 1);
      send("feature setboard=%i\n", 1);
      send("feature time=%i\n", 1);
      send("feature san=%i\n", 0);
      send("feature reuse=%i\n", 0);
      send("feature usermove=%i\n", 1);
      send("feature analyze=%i\n", 0);
      send("feature draw=%i\n", 0);
      send("feature myname=\"%s%s%s%s\"\n", "ChuPacabra v0.12.35", // VERSION
        changed_default_evaluator_file ? "[" : "",
        changed_default_evaluator_file ? default_evaluator_file: "",
        changed_default_evaluator_file ? "]" : ""
        );
      send("feature pause=%i\n", 0);
      send("feature nps=%i\n", 0);
      send("feature debug=%i\n", 1);
      send("feature memory=%i\n", 0);
      send("feature smp=%i\n", 0);
      send("feature sigint=%i\n", 0);
      send("feature sigterm=%i\n", 1);
      send("feature variants=\"%s\"\n", "chu");
      send("feature option=\"%s -slider %i %i %i\"\n", "Max_Depth", max_depth/100, 1, 20);
      send("feature option=\"%s -check %i\"\n", "NMP", wb_data.ss.f_nmp);
      send("feature option=\"%s -check %i\"\n", "LMR", wb_data.ss.f_lmr);
      send("feature option=\"%s -slider %i %i %i\"\n", "ASP_Window", wb_data.ss.asp_width, 1, 500);
      send("feature option=\"%s -slider %i %i %i\"\n", "QDepth", wb_data.ss.qdepth/100, 1, 20);
      send("feature option=\"%s -check %i\"\n", "Razoring", wb_data.ss.f_razoring);
      send("feature option=\"%s -check %i\"\n", "Limited_Razoring", wb_data.ss.f_limrazoring);
      send("feature option=\"%s -check %i\"\n", "Futility", wb_data.ss.f_futility);
      send("feature option=\"%s -check %i\"\n", "Extended_Futility", wb_data.ss.f_extfutility);
      send("feature option=\"%s -file %s\"\n", "Evaluation_Weights_File", default_evaluator_file);
      //send("feature option=\"%s -combo CA /// CB /// CC\"\n", "ComboTest");
      sleep(1);
      send("done=%i\n", 1);
      wb_data.state = wait_wbs;
    }
    else if (strncmp(buff, "quit", 4)==0)
    {
      handled = 1;
      stop_search = 1;
      /* immediately exit */
      wb_data.state = stop_wbs;
      pthread_mutex_lock(&wb_data.worker.queue_mutex);
      wb_data.worker.state = stop_wbws;
      wb_data.worker.exit_flag = 1;
      pthread_cond_signal(&wb_data.worker.queue_cond);
      pthread_mutex_unlock(&wb_data.worker.queue_mutex);
    }
    else if (strncmp(buff, "variant", 7)==0) // variant VARNAME
    {
      handled = 1;
      /* set variant type */
    }
    else if (strncmp(buff, "random", 6)==0)
    {
      handled = 1;
      /* toggle random mode (new sets random mode off) */
      random = !random;
      if (random) random_seed = read_clock(); else random_seed = 137999;
      sendlog("random_seed=%i\n", random_seed);
      srand48(random_seed);
      ttable_init();
      init_hash();
      init_board(&wb_data.b);
    }
    else if (strncmp(buff, "ping", 4)==0) // ping N
    {
      handled = 1;
      /* should be responded to by 'pong N', but not before completing all received commands (except pondering) */
      /* TODO: get this working right */
      int i;
      sscanf(buff, "ping %i", &i);
      send("pong %i\n", i);
    }
    else if (strncmp(buff, "new", 3)==0)
    {
      handled = 1;
      /* reset board to starting position, set white on-move, leave force mode and set engine to play black, reset clocks, don't ponder */
      init_board(&wb_data.b);
      random = 0;
      wb_data.state = wait_wbs;
      pthread_mutex_lock(&wb_data.worker.queue_mutex);
      wb_data.worker.state = wait_wbws;
      wb_data.worker.count = 0;
      pthread_mutex_unlock(&wb_data.worker.queue_mutex);
    }
    else if (strncmp(buff, "force", 5)==0)
    {
      handled = 1;
      /* set engine to play neither color, stops clocks, should check moves for validity but do nothing else */
      wb_data.state = wait_wbs;
      /* TODO: force */
    }
    else if (strncmp(buff, "go", 2)==0)
    {
      handled = 1;
      /* leave force mode and set engine to play the color that is on move, start thinking */
      wb_data.p.mover = search_mover;
      wb_data.p.ss = wb_data.ss;
      wb_data.p.tc = wb_data.tc;
      wb_data.p.e = wb_data.e;
      pthread_mutex_lock(&wb_data.worker.queue_mutex);
      wb_data.worker.p = wb_data.p;
      wb_data.worker.b = wb_data.b;
      wb_data.state = think_wbs;
      wb_data.worker.state = work_wbws;
      wb_data.worker.time = wb_data.time;
      pthread_cond_signal(&wb_data.worker.queue_cond);
      pthread_mutex_unlock(&wb_data.worker.queue_mutex);
    }
    else if (strncmp(buff, "white", 5)==0)
    {
      handled = 1;
      /* set white on-move, set engine to play black, stop clocks */
      /* TODO: white */
    }
    else if (strncmp(buff, "black", 5)==0)
    {
      handled = 1;
      /* set black on-move, set engine to play white, stop clocks */
      /* TODO: black */
    }
    else if (strncmp(buff, "playother", 9)==0)
    {
      handled = 1;
      /* leave force mode and set engine to play the color that is _not_ on move, start pondering (if enabled) */
      /* TODO: playother */
    }
    else if (strncmp(buff, "level", 5)==0) // level MPS BASE INC
    {
      handled = 1;
      /* set time controls: play MPS moves in BASE minutes with increment of INC seconds per move
       * MPS=moves per time control; '0'=whole game
       * BASE=base time; '5'=5 minutes, '0:20'=20 seconds
       * INC=increment (in seconds)
       */
      int mps, base, inc;
      /* TODO: handle 'nn:nn' format ! */
      sscanf(buff, "level %i %i %i", &mps, &base, &inc);
      wb_data.tc.starting_cs = base*6000;
      wb_data.tc.increment_cs = inc*100;
      wb_data.tc.moves_per = mps;

      pthread_mutex_lock(&send_mutex);
      show_time_control(wb_data.flog, &wb_data.tc); fprintf(wb_data.flog, "\n");
      pthread_mutex_unlock(&send_mutex);
    }
    else if (strncmp(buff, "st", 2)==0) // st TIME
    {
      handled = 1;
      /* set time controls: play each move in TIME seconds, no accumulation between turns */

      int byo;
      sscanf(buff, "st %i", &byo);
      wb_data.tc.byoyomi_cs = byo;

      pthread_mutex_lock(&send_mutex);
      show_time_control(wb_data.flog, &wb_data.tc); fprintf(wb_data.flog, "\n");
      pthread_mutex_unlock(&send_mutex);
    }
    else if (strncmp(buff, "sd", 2)==0) // sd DEPTH
    {
      handled = 1;
      /* set max ply depth to DEPTH play; note that this should be cumulative with time settings */
      int d;
      sscanf(buff, "sd %i", &d);
      wb_data.tc.max_depth = d;

      pthread_mutex_lock(&send_mutex);
      show_time_control(wb_data.flog, &wb_data.tc); fprintf(wb_data.flog, "\n");
      pthread_mutex_unlock(&send_mutex);
    }
    else if (strncmp(buff, "nps", 3)==0) // nps NODE_RATE
    {
      handled = 1;
      /* use node-counts for timing */
      /* TODO: nps */
    }
    else if (strncmp(buff, "option", 6)==0) // option NAME[=VALUE]
    {
      handled = 1;
      /* change setting of option */
      char option_name[256], option_value[256];
      sscanf(buff, "option %[^=]=%s", option_name, option_value);
      if (strcmp(option_name, "Max_Depth")==0)                  wb_data.ss.depth = 100 * atoi(option_value);
      else if (strcmp(option_name, "NMP")==0)                   wb_data.ss.f_nmp = atoi(option_value);
      else if (strcmp(option_name, "LMR")==0)                   wb_data.ss.f_lmr = atoi(option_value);
      else if (strcmp(option_name, "ASP_Window")==0)            wb_data.ss.asp_width = atoi(option_value);
      else if (strcmp(option_name, "QDepth")==0)                wb_data.ss.qdepth = 100 * atoi(option_value);
      else if (strcmp(option_name, "Razoring")==0)              wb_data.ss.f_razoring = atoi(option_value);
      else if (strcmp(option_name, "Limited_Razoring")==0)      wb_data.ss.f_limrazoring = atoi(option_value);
      else if (strcmp(option_name, "Futility")==0)              wb_data.ss.f_futility = atoi(option_value);
      else if (strcmp(option_name, "Extended_Futility")==0)     wb_data.ss.f_extfutility = atoi(option_value);
      else if (strcmp(option_name, "Evaluation_Weights_File")==0)
      {
        FILE* fe = fopen(option_value, "r");
        if (fe)
        {
          load_evaluator(fe, &wb_data.e);
          fclose(fe);
#if 0
          pthread_mutex_lock(&send_mutex);
          fprintf(wb_data.flog, "*** Evaluation parameters: ***\n");
          show_evaluator(wb_data.flog, &wb_data.e);
          fprintf(wb_data.flog, "*** ***\n");
          pthread_mutex_unlock(&send_mutex);
#endif
        }
        else
        {
          sendlog("Unable to open evaluation weights file '%s'\n", option_value);
        }
      }
      else send("# Unknown option '%s'\n", option_name);

      pthread_mutex_lock(&send_mutex);
      show_settings(wb_data.flog, &wb_data.ss); fprintf(wb_data.flog, "\n");
      pthread_mutex_unlock(&send_mutex);
    }
    else if (strncmp(buff, "time", 4)==0) // time N
    {
      /* set clock belonging to engine, N=# of centiseconds */
      handled = 1;
      int i;
      sscanf(buff, "time %i", &i);
      send("# time = %02i:%02i:%02i.%i\n",
          ((i/100)/60)/60, (((i%360000)/100)/60), (i%6000)/100, i%100);
      wb_data.time = i;
      wb_data.tc.remaining_cs = i;

      pthread_mutex_lock(&send_mutex);
      show_time_control(wb_data.flog, &wb_data.tc); fprintf(wb_data.flog, "\n");
      pthread_mutex_unlock(&send_mutex);
    }
    else if (strncmp(buff, "otim", 4)==0) // otim N
    {
      /* set clock belonging to opponent, N=# of centiseconds */
      handled = 1;
      int i;
      sscanf(buff, "otim %i", &i);
      send("# otime = %02i:%02i:%02i.%i\n",
          i/100/60/60, (i%360000)/60/100, (i%6000)/100, i%100);
      wb_data.otime = i;
    }
    else if (strncmp(buff, "usermove", 8)==0) // usermove MOVE
    {
      /* move format: e2e4 or e2e3,e3e4 or @@@@ (nullmove) */
      handled = 1;
      pthread_mutex_lock(&wb_data.worker.queue_mutex);
      memset(wb_data.worker.work_queue[0].move, '\x00', 64);
      sscanf(buff, "usermove %63s", wb_data.worker.work_queue[0].move);

      movet m = winboard_to_move(&wb_data.b, wb_data.worker.work_queue[0].move);
      pthread_mutex_lock(&send_mutex);
      show_move(wb_data.flog, m); fprintf(wb_data.flog, "\n");
      pthread_mutex_unlock(&send_mutex);
      if (!m.valid)
      {
        send("Illegal move: %s\n", wb_data.worker.work_queue[0].move);
      }
      else
      {
        if (make_move(&wb_data.b, m) == invalid_move)
        {
          send("Illegal move: %s\n", wb_data.worker.work_queue[0].move);
          pthread_mutex_unlock(&wb_data.worker.queue_mutex);
        }
        else if (terminal(&wb_data.b))
        {
          pthread_mutex_lock(&send_mutex);
          show_board(wb_data.flog, &wb_data.b);
          pthread_mutex_unlock(&send_mutex);

          pthread_mutex_unlock(&wb_data.worker.queue_mutex);
          if (!wb_data.b.king_mask[white] && !wb_data.b.prince_mask[white])
            send("0-1 {mate}\n");
          else if (!wb_data.b.king_mask[black] && !wb_data.b.prince_mask[black])
            send("1-0 {mate}\n");
          else
            send("1/2-1/2 {?}\n");
        }
        else
        {
          pthread_mutex_lock(&send_mutex);
          show_board(wb_data.flog, &wb_data.b);
          pthread_mutex_unlock(&send_mutex);

          if (1)
          {
            pthread_mutex_lock(&send_mutex);
            movet ml[MAXNMOVES];
            evalt scorel[MAXNMOVES];
            int k, n = gen_moves(&wb_data.b, ml);
            evaluate_moves_for_search(&wb_data.e, &wb_data.b, ml, scorel, n);
            for (k=0; k<n; ++k) {
              fstatus(wb_data.flog, "\t%m(%i)", ml[k], (int)scorel[k]);
              if ((k%5)==4) fprintf(wb_data.flog, "\n");
            }
            if (n%5) fprintf(wb_data.flog, "\n");
            pthread_mutex_unlock(&send_mutex);
          }

          wb_data.p.mover = search_mover;
          wb_data.p.ss = wb_data.ss;
          wb_data.p.e = wb_data.e;
          wb_data.p.tc = wb_data.tc;
          wb_data.worker.p = wb_data.p;
          wb_data.worker.b = wb_data.b;
          wb_data.state = think_wbs;
          wb_data.worker.state = work_wbws;
          wb_data.worker.time = wb_data.time;
          pthread_cond_signal(&wb_data.worker.queue_cond);
          pthread_mutex_unlock(&wb_data.worker.queue_mutex);
        }
      }
    }
    else if (strncmp(buff, "draw", 4)==0)
    {
      handled = 1;
      /* opponent offers draw, to accept send 'offer draw', don't assume it works, though */
    }
    else if (strncmp(buff, "result", 6)==0) // result RESULT {COMMENT}
    {
      handled = 1;
      /* RESULT is 1-0, 0-1, 1/2-1/2, or *, COMMENT is arbitrary string */
    }
    else if (strncmp(buff, "setboard", 8)==0) // setboard FEN
    {
      handled = 1;
      /* set board using FEN style string */
      /* Example:
       * setboard lfcsgekgscfl/a1b1txot1b1a/mvrhdqndhrvm/pppppppppppp/3i4i3/12/12/3I4I3/PPPPPPPPPPPP/MVRHDNQDHRVM/A1B1TOXT1B1A/LFCSGKEGSCFL w - 0 1
       */
      handled = 1;

      wbfen_to_board(&wb_data.b, buff+9);
      pthread_mutex_lock(&send_mutex);
      show_board(wb_data.flog, &wb_data.b);
      pthread_mutex_unlock(&send_mutex);

      random = 0;
      wb_data.state = wait_wbs;
      pthread_mutex_lock(&wb_data.worker.queue_mutex);
      wb_data.worker.state = wait_wbws;
      wb_data.worker.count = 0;
      pthread_mutex_unlock(&wb_data.worker.queue_mutex);
    }
    else if (strncmp(buff, "hint", 4)==0)
    {
      handled = 1;
      /* respond with 'Hint: xxx' if there is a hint */
    }
    else if (strncmp(buff, "bk", 2)==0)
    {
      handled = 1;
      /* ... */
    }
    else if (strncmp(buff, "undo", 4)==0)
    {
      handled = 1;
      /* back-up one user move, only in force mode */
      unmake_move(&wb_data.b);
      pthread_mutex_lock(&send_mutex);
      show_board(wb_data.flog, &wb_data.b);
      pthread_mutex_unlock(&send_mutex);
    }
    else if (strncmp(buff, "remove", 6)==0)
    {
      handled = 1;
      /* undo last two moves, only sent when user to move */
      unmake_move(&wb_data.b);
      unmake_move(&wb_data.b);
      pthread_mutex_lock(&send_mutex);
      show_board(wb_data.flog, &wb_data.b);
      pthread_mutex_unlock(&send_mutex);
    }
    else if (strncmp(buff, "hard", 4)==0)
    {
      handled = 1;
      /* turn on pondering */
      pondering = 1;
    }
    else if (strncmp(buff, "easy", 4)==0)
    {
      handled = 1;
      /* turn off pondering */
      pondering = 0;
    }
    else if (strncmp(buff, "post", 4)==0)
    {
      handled = 1;
      /* turn on thinking/pondering output */
      post = 1;
    }
    else if (strncmp(buff, "nopost", 6)==0)
    {
      handled = 1;
      /* turn off thinking/pondering output */
      post = 0;
    }
    else if (strncmp(buff, "analyze", 6)==0)
    {
      handled = 1;
      /* enter analyze mode */
      /* TODO: analyze mode... */
    }
    else if (strncmp(buff, "name", 4)==0) // name X
    {
      handled = 1;
      /* inform engine of opponents name */
    }
    else if (strncmp(buff, "rating", 6)==0) // rating ENGINE OPPONENT
    {
      handled = 1;
      /* inform engine of ratings */
    }
    else if (strncmp(buff, "?", 1)==0)
    {
      handled = 1;
      /* move now! */
      /* TODO: move now! */
    }
    else if (strncmp(buff, "accepted", 8)==0) // accepted F
    {
      handled = 1;
      /* feature F from engine was accepted */
    }
    else if (strncmp(buff, "rejected", 8)==0) // rejected F
    {
      handled = 1;
      /* feature F from engine was rejected */
    }

    if (!handled)
      send("Error (unknown command): %s", buff);
  }
}

static int wb_shell(int argc, const char* const argv[])
{
  int pid = getpid();
  char buff[64];
  sprintf(buff, "chupacabra-log-%i.txt", pid);

  /* handle command-line parameters */
  // TODO: unify this with WB options, etc.
  if (argc > 1)
  {
    int arg;
    for (arg=1; arg<argc; ++arg)
    {
      if (strcmp(argv[arg], "-winboard")==0)
      {
        continue;
      }
      else if (strcmp(argv[arg], "-evaluator")==0)
      {
        ++arg;
        if (arg < argc)
        {
          strcpy(default_evaluator_file, argv[arg]);
          changed_default_evaluator_file = 1;
        }
      }
    }
  }

  memset(&wb_data, '\x00', sizeof(wb_data));
  wb_data.flog = fopen(buff, "a");
  setvbuf(wb_data.flog, NULL, _IONBF, 0);

  fprintf(wb_data.flog, ">> Program started (%s : %i)\n", argv[0], getpid());
  {
    time_t curtime;
    struct tm *loctime;
    /* Get the current time.  */
    curtime = time (NULL);
    /* Convert it to local time representation.  */
    loctime = localtime (&curtime);
    /* Print out the date and time in the standard format.  */
    fprintf(wb_data.flog, ">> %s\n", asctime(loctime));
  }

  if (pthread_mutex_init(&send_mutex, NULL)) {
    fprintf(stderr, "Could not create send mutex\n");
    return -1;
  }

  {
    wb_data.worker.id = 0;
    if (pthread_mutex_init(&wb_data.worker.queue_mutex, NULL)) {
      fprintf(stderr, "Could not create mutex %i\n", wb_data.worker.id);
      return -1;
    }
    if (pthread_cond_init(&wb_data.worker.queue_cond, NULL)) {
      fprintf(stderr, "Could not create condition variable %i\n", wb_data.worker.id);
      return -1;
    }
    if (pthread_create(&wb_data.worker.thread, NULL, &wb_worker, &wb_data.worker)) {
      fprintf(stderr, "Could not create thread %i\n", wb_data.worker.id);
      return -1;
    }
  }

  wb_ioloop();

  {
    if (pthread_join(wb_data.worker.thread, NULL)) {
      fprintf(stderr, "Could not join thread %i\n", wb_data.worker.id);
      return -1;
    }
    if (pthread_cond_destroy(&wb_data.worker.queue_cond)) {
      fprintf(stderr, "Could not destroy condition variable %i\n", wb_data.worker.id);
      return -1;
    }
    if (pthread_mutex_destroy(&wb_data.worker.queue_mutex)) {
      fprintf(stderr, "Could not destroy mutex %i\n", wb_data.worker.id);
      return -1;
    }
  }

  if (pthread_mutex_destroy(&send_mutex)) {
    fprintf(stderr, "Could not destroy send mutex\n");
    return -1;
  }

  fprintf(wb_data.flog, ">> Program ended\n");
  fclose(wb_data.flog);

  return 0;
}

/* ************************************************************ */
/* ************************************************************ */

int wb_main(int argc, const char * const argv[])
{
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);
  in_winboard = 1;
  return wb_shell(argc,argv);
}

/* ************************************************************ */
/* ************************************************************ */
