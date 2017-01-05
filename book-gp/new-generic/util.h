#ifndef    INCLUDED_UTIL_H_
#define    INCLUDED_UTIL_H_

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

typedef unsigned char       ui8;
typedef unsigned short      ui16;
typedef unsigned int        ui32;
typedef unsigned long long  ui64;
typedef unsigned long long  hasht;

template <typename T>
inline T min(T a, T b) { return a<b ? a : b; }
template <typename T>
inline T max(T a, T b) { return a>b ? a : b; }

#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/
ui64 read_clock(void);

hasht gen_hash();
#ifdef  __cplusplus
};
#endif/*__cplusplus*/

// TODO: allow formatting parameters in q/m/M types
// (maybe _parse_ completely and give full structure of elements?)
template<typename movet, typename evalt>
int fstatus_internal(FILE* f, const char* fmt, va_list ap)
{
  int n = 0;
  //va_list ap;
  //va_start(ap, fmt);
  while (*fmt) {
    switch (*fmt) {
      case '%':
        ++fmt;
        switch (*fmt) {
          case 'q':
            {
              ++fmt;
              evalt v = va_arg(ap, evalt);
              n += v.showf(f);
            }
            break;
          case 'm':
            {
              ++fmt;
              movet m = va_arg(ap, movet);
              n += m.showf(f);
            }
            break;
          case 'M':
            {
              ++fmt;
              movet* ml = va_arg(ap, movet*);
              int i, nm = va_arg(ap, int);
              for (i=0; i<nm; ++i) {
                if (i) {n += fprintf(f, "  ");}
                n += ml[i].showf(f);
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

template<typename movet, typename evalt>
int fstatus(FILE* f, const char* fmt, ...)
{
  int n = 0;
  va_list ap;
  va_start(ap, fmt);
  n = fstatus_internal<movet,evalt>(f, fmt, ap);
  va_end(ap);
  return n;
}

template<typename movet, typename evalt>
int status(const char* fmt, ...)
{
  int n = 0;
  va_list ap;
  va_start(ap, fmt);
  n = fstatus_internal<movet>(stdout, fmt, ap);
  va_end(ap);
  return n;
}

#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/
void ansi_fg_black(FILE* f);
void ansi_fg_red(FILE* f);
void ansi_fg_green(FILE* f);
void ansi_fg_yellow(FILE* f);
void ansi_fg_blue(FILE* f);
void ansi_fg_magenta(FILE* f);
void ansi_fg_cyan(FILE* f);
void ansi_fg_white(FILE* f);
void ansi_fg_default(FILE* f);
void ansi_bg_black(FILE* f);
void ansi_bg_red(FILE* f);
void ansi_bg_green(FILE* f);
void ansi_bg_yellow(FILE* f);
void ansi_bg_blue(FILE* f);
void ansi_bg_magenta(FILE* f);
void ansi_bg_cyan(FILE* f);
void ansi_bg_white(FILE* f);
void ansi_bg_default(FILE* f);
void ansi_reset(FILE* f);
void ansi_normal(FILE* f);
void ansi_bright(FILE* f);
void ansi_faint(FILE* f);
void ansi_underline(FILE* f);
void ansi_cursor_up(FILE* f, int n);
void ansi_cursor_down(FILE* f, int n);
void ansi_cursor_forward(FILE* f, int n);
void ansi_cursor_back(FILE* f, int n);
void ansi_cursor_next_line(FILE* f, int n);
void ansi_cursor_prev_line(FILE* f, int n);
void ansi_cursor_position(FILE* f, int x, int y);
void ansi_hide_cursor(FILE* f);
void ansi_show_cursor(FILE* f);
void ansi_clear_to_eos(FILE* f);
void ansi_clear_to_bos(FILE* f);
void ansi_clear_screen(FILE* f);
void ansi_clear_to_eol(FILE* f);
void ansi_clear_to_bol(FILE* f);
void ansi_clear_line(FILE* f);
#ifdef  __cplusplus
};
#endif/*__cplusplus*/

#endif /* INCLUDED_UTIL_H_ */
