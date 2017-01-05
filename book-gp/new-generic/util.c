#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "util.h"

ui64 read_clock(void) {
#if 1
  {
    struct timeval timeval;
    struct timezone timezone;
    gettimeofday(&timeval, &timezone);
    return (ui64)timeval.tv_sec * 1000000ULL + (ui64)(timeval.tv_usec);
  }
#else
  {
    const clockid_t id = CLOCK_MONOTONIC;
    struct timespec ts;
    clock_gettime(id, &ts);
    return (ui64)(1e6*((double)ts.tv_sec + (double)ts.tv_nsec * 1e-9));
  }
#endif
}

hasht gen_hash() {
  hasht h = 0;
  int i;
  for (i=0; i<32; ++i)
    h = h ^ ((hasht)(lrand48())<<(2*i)) ^ ((hasht)(lrand48())>>(2*i));
  return h;
}

void ansi_fg_black(FILE* f) {fprintf(f,"\033[30m");}
void ansi_fg_red(FILE* f) {fprintf(f,"\033[31m");}
void ansi_fg_green(FILE* f) {fprintf(f,"\033[32m");}
void ansi_fg_yellow(FILE* f) {fprintf(f,"\033[33m");}
void ansi_fg_blue(FILE* f) {fprintf(f,"\033[34m");}
void ansi_fg_magenta(FILE* f) {fprintf(f,"\033[35m");}
void ansi_fg_cyan(FILE* f) {fprintf(f,"\033[36m");}
void ansi_fg_white(FILE* f) {fprintf(f,"\033[37m");}
void ansi_fg_default(FILE* f) {fprintf(f,"\033[39m");}
void ansi_bg_black(FILE* f) {fprintf(f,"\033[40m");}
void ansi_bg_red(FILE* f) {fprintf(f,"\033[41m");}
void ansi_bg_green(FILE* f) {fprintf(f,"\033[42m");}
void ansi_bg_yellow(FILE* f) {fprintf(f,"\033[43m");}
void ansi_bg_blue(FILE* f) {fprintf(f,"\033[44m");}
void ansi_bg_magenta(FILE* f) {fprintf(f,"\033[45m");}
void ansi_bg_cyan(FILE* f) {fprintf(f,"\033[46m");}
void ansi_bg_white(FILE* f) {fprintf(f,"\033[47m");}
void ansi_bg_default(FILE* f) {fprintf(f,"\033[49m");}
void ansi_reset(FILE* f) {fprintf(f,"\033[0m");}
void ansi_normal(FILE* f) {fprintf(f,"\033[22m");}
void ansi_bright(FILE* f) {fprintf(f,"\033[1m");}/* maybe only with fg/bg? */
void ansi_faint(FILE* f) {fprintf(f,"\033[2m");}/* maybe only with fg/bg? */
void ansi_underline(FILE* f) {fprintf(f,"\033[4m");}
void ansi_cursor_up(FILE* f, int n) {fprintf(f,"\033[%iA",n);}
void ansi_cursor_down(FILE* f, int n) {fprintf(f,"\033[%iB",n);}
void ansi_cursor_forward(FILE* f, int n) {fprintf(f,"\033[%iC",n);}
void ansi_cursor_back(FILE* f, int n) {fprintf(f,"\033[%iD",n);}
void ansi_cursor_next_line(FILE* f, int n) {fprintf(f,"\033[%iE",n);}
void ansi_cursor_prev_line(FILE* f, int n) {fprintf(f,"\033[%iF",n);}
void ansi_cursor_position(FILE* f, int x, int y) {fprintf(f,"\033[%i;%iH",x,y);}
void ansi_hide_cursor(FILE* f) {fprintf(f,"\033[?25l");}
void ansi_show_cursor(FILE* f) {fprintf(f,"\033[?25h");}
void ansi_clear_to_eos(FILE* f) {fprintf(f,"\033[0J");}
void ansi_clear_to_bos(FILE* f) {fprintf(f,"\033[1J");}
void ansi_clear_screen(FILE* f) {fprintf(f,"\033[2J");}
void ansi_clear_to_eol(FILE* f) {fprintf(f,"\033[0K");}
void ansi_clear_to_bol(FILE* f) {fprintf(f,"\033[1K");}
void ansi_clear_line(FILE* f) {fprintf(f,"\033[2K");}


