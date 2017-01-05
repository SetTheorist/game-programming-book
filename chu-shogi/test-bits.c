#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

typedef unsigned long long int ui64;
typedef unsigned int ui32;

#include "util-bits.h"

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
    return (ui64)(1000000LL * ts.tv_sec + ts.tv_nsec / 1000LL);
  }
#endif
}



typedef union a {
  unsigned int i;
  struct {
    unsigned a : 3;
    unsigned b : 4;
    unsigned c : 5;
    unsigned d : 6;
    unsigned e : 1;
    unsigned f : 1;
    unsigned g : 1;
  };
} a;

#define GET_AA(i)   ((i)&0x00007)
#define SET_AA(i,a) (i) = ((i)&~0x0007) | ((a)&0x00007)
#define GET_BB(i)   (((i)>>3)&0x0000F)
#define SET_BB(i,b) (i) = ((i)&~0x0078) | (((b)&0x0000F)<<3)

#define SET_AABB(i,a,b) (i) = ((i)&~0x007F) | (((a)&0x00007)|(((b)&0x0000F)<<3))


int main()
{
  int i, j, k;
  ui64 tt1 = 0, tt2 = 0, tt3 = 0;

  printf("sizeof(short int)=%i\n", sizeof(short int));
  printf("sizeof(int)=%i\n", sizeof(int));
  printf("sizeof(long int)=%i\n", sizeof(long int));
  printf("sizeof(long long int)=%i\n", sizeof(long long int));

  {
    int x = 0x00030510;
    while (x)
    {
      int b = bit_scan_forward32(x);
      printf("x=%08x  b=%i\n", x, b);
      x ^= 1<<b;
    }
  }


  for (k=0; k<1000; ++k)
  {
    {
      a a_arr[1024];
      memset(a_arr, '\x00', sizeof(a_arr));
      int sum = 0;
      ui64 bt = read_clock();
      for (i=0; i<10000; ++i)
      {
        for (j=0; j<1024; ++j)
        {
          a_arr[j].a = i;
          a_arr[j].b = j;
        }
        int sum = 0;
        for (j=0; j<1024; ++j)
        {
          sum += a_arr[j].a;
          sum += a_arr[j].b;
        }
      }
      ui64 et = read_clock();
      printf(".time=%'lli  sum=%i\n", et-bt, sum);
      tt1 += et - bt;
    }
    {
      int a_arr[1024];
      memset(a_arr, '\x00', sizeof(a_arr));
      int sum = 0;
      ui64 bt = read_clock();
      for (i=0; i<10000; ++i)
      {
        for (j=0; j<1024; ++j)
        {
          SET_AA(a_arr[j], i);
          SET_BB(a_arr[j], j);
        }
        int sum = 0;
        for (j=0; j<1024; ++j)
        {
          sum += GET_AA(a_arr[j]);
          sum += GET_BB(a_arr[j]);
        }
      }
      ui64 et = read_clock();
      printf("-time=%'lli  sum=%i\n", et-bt, sum);
      tt2 += et - bt;
    }
    {
      int a_arr[1024];
      memset(a_arr, '\x00', sizeof(a_arr));
      int sum = 0;
      ui64 bt = read_clock();
      for (i=0; i<10000; ++i)
      {
        for (j=0; j<1024; ++j)
        {
          SET_AABB(a_arr[j], i, j);
        }
        int sum = 0;
        for (j=0; j<1024; ++j)
        {
          sum += GET_AA(a_arr[j]);
          sum += GET_BB(a_arr[j]);
        }
      }
      ui64 et = read_clock();
      printf("+time=%'lli  sum=%i\n", et-bt, sum);
      tt3 += et - bt;
    }
  }

  printf("time1 = %'lli\n", tt1/k);
  printf("time2 = %'lli\n", tt2/k);
  printf("time3 = %'lli\n", tt3/k);


  return 0;
}
