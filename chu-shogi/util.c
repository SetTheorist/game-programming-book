/* $Id: util.c,v 1.2 2010/12/16 20:09:01 apollo Exp $ */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "board.h"
#include "util.h"

ui64 read_clock(void) {
#if 0
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

static int fstatus_internal(FILE* f, const char* fmt, va_list ap)
{
  int n = 0;
  //va_list ap;
  //va_start(ap, fmt);
  while (*fmt) {
    switch (*fmt) {
      case '%':
        ++fmt;
        switch (*fmt) {
          case 'm':
            {
              ++fmt;
              movet m = va_arg(ap, movet);
              n += show_move(f, m);
            }
            break;
          case 'k':
            {
              ++fmt;
              movet m = va_arg(ap, movet);
              n += show_move_kanji(f, m);
            }
            break;
          case 'q':
            {
              ++fmt;
              movet m = va_arg(ap, movet);
              n += show_move_eng(f, m);
            }
            break;
          case 'M':
            {
              ++fmt;
              movet* ml = va_arg(ap, movet*);
              int i, nm = va_arg(ap, int);
              for (i=0; i<nm; ++i) {
                if (i) {n += fprintf(f, "  ");}
                n += show_move(f, ml[i]);
              }
            }
            break;
          case 'K':
            {
              ++fmt;
              movet* ml = va_arg(ap, movet*);
              int i, nm = va_arg(ap, int);
              for (i=0; i<nm; ++i) {
                if (i) {n += fprintf(f, "  ");}
                n += show_move_kanji(f, ml[i]);
              }
            }
            break;
          case 'Q':
            {
              ++fmt;
              movet* ml = va_arg(ap, movet*);
              int i, nm = va_arg(ap, int);
              for (i=0; i<nm; ++i) {
                if (i) {n += fprintf(f, "  ");}
                n += show_move_eng(f, ml[i]);
              }
            }
            break;
          default:
            {
              char buff[64];
              int l;
              buff[0] = '%';
              for (l=1; *fmt && l<63 && !strchr("cdiouxXeEfFgGspaAn%",*fmt); ++l)
                buff[l] = *fmt++;
              buff[l++] = *fmt++;
              buff[l] = '\x00';
              if (strchr(buff, '*'))
              {
                int ii = va_arg(ap, int);
                switch (buff[l-1])
                {
                  case 'c':
                  case 'd':
                  case 'i':
                    if (strstr(buff,"ll"))
                      n += fprintf(f, buff, ii, va_arg(ap, long long int));
                    else if (strstr(buff,"l"))
                      n += fprintf(f, buff, ii, va_arg(ap, long int));
                    else
                      n += fprintf(f, buff, ii, va_arg(ap, int));
                    break;
                  case 'o':
                  case 'x':
                  case 'X':
                  case 'u':
                    if (strstr(buff,"ll"))
                      n += fprintf(f, buff, ii, va_arg(ap, long long unsigned int));
                    else if (strstr(buff,"l"))
                      n += fprintf(f, buff, ii, va_arg(ap, long unsigned int));
                    else
                      n += fprintf(f, buff, ii, va_arg(ap, unsigned int));
                    break;
                  case 'A':
                  case 'a':
                  case 'F':
                  case 'f':
                  case 'G':
                  case 'g':
                  case 'E':
                  case 'e': n += fprintf(f, buff, ii, va_arg(ap, double));
                    break;
                  case 's': n += fprintf(f, buff, ii, va_arg(ap, const char*));
                    break;
                  case 'p': n += fprintf(f, buff, ii, va_arg(ap, void*));
                    break;
                  case '%': n += fprintf(f, buff, ii);
                    break;
                  case 'n': *(va_arg(ap, int*)) = n;
                    break;
                  default: fprintf(stderr, "***** UNSUPPORTED PRINTF CODE IN FSTATUS: %s *****\n", buff); break;
                }
              }
              else
              {
                switch (buff[l-1])
                {
                  case 'c':
                  case 'd':
                  case 'i':
                    if (strstr(buff,"ll"))
                      n += fprintf(f, buff, va_arg(ap, long long int));
                    else if (strstr(buff,"l"))
                      n += fprintf(f, buff, va_arg(ap, long int));
                    else
                      n += fprintf(f, buff, va_arg(ap, int));
                    break;
                  case 'o':
                  case 'x':
                  case 'X':
                  case 'u':
                    if (strstr(buff,"ll"))
                      n += fprintf(f, buff, va_arg(ap, long long unsigned int));
                    else if (strstr(buff,"l"))
                      n += fprintf(f, buff, va_arg(ap, long unsigned int));
                    else
                      n += fprintf(f, buff, va_arg(ap, unsigned int));
                    break;
                  case 'A':
                  case 'a':
                  case 'F':
                  case 'f':
                  case 'G':
                  case 'g':
                  case 'E':
                  case 'e':
                    n += fprintf(f, buff, va_arg(ap, double));
                    break;
                  case 's':
                    n += fprintf(f, buff, va_arg(ap, const char*));
                    break;
                  case 'p':
                    n += fprintf(f, buff, va_arg(ap, void*));
                    break;
                  case '%':
                    n += fprintf(f, buff);
                    break;
                  case 'n':
                    *(va_arg(ap, int*)) = n;
                    break;
                  default: fprintf(stderr, "***** UNSUPPORTED PRINTF CODE IN FSTATUS: %s *****\n", buff); break;
                }
              }
              //n += vfprintf(f, buff, ap);
            }
        }
        break;
      default: fprintf(f, "%c", *fmt++); ++n; break;
    }
  }
  //va_end(ap);
  return n;
}

int fstatus(FILE* f, const char* fmt, ...)
{
  int n = 0;
  va_list ap;
  va_start(ap, fmt);
  n = fstatus_internal(f, fmt, ap);
  va_end(ap);
  return n;
}

int status(const char* fmt, ...)
{
  int n = 0;
  va_list ap;
  va_start(ap, fmt);
  n = fstatus_internal(stdout, fmt, ap);
  va_end(ap);
  return n;
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
