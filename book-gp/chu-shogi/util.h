#ifndef    INCLUDED_UTIL_H_
#define    INCLUDED_UTIL_H_
/* $Id: util.h,v 1.2 2010/12/16 20:09:01 apollo Exp $ */
#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

#include <stdio.h>

typedef unsigned char       ui8;
typedef unsigned short      ui16;
typedef unsigned int        ui32;
typedef unsigned long long  ui64;
typedef unsigned long long  hasht;

/* in microseconds */
ui64 read_clock(void);

hasht gen_hash();

static inline int max(register const int a, register const int b) { return (a > b) ? a : b; }
static inline int min(register const int a, register const int b) { return (a < b) ? a : b; }
static inline double dmax(register const double a, register const double b) { return (a > b) ? a : b; }
static inline double dmin(register const double a, register const double b) { return (a < b) ? a : b; }

int fstatus(FILE* f, const char* fmt, ...);
int status(const char* fmt, ...);

int send(const char* fmt, ...);
int sendlog(const char* fmt, ...);
FILE* logfile();
extern int in_winboard;

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
