#include <stdio.h>

#include "board.h"
#include "util.h"
#include "util-bits.h"

int main()
{
  {
    int r, c, d;

    printf("static const unsigned int square_neighbor_map[144] = {\n");
    for (r=0; r<12; ++r)
    {
      printf("  ");
      for (c=0; c<12; ++c)
      {
        int map = 0;
        for (d=0; d<8; ++d)
        {
          const int cc = c + dir_map_dx[d+1];
          const int rr = r + dir_map_dy[d+1];
          if ((cc >= 0) && (cc < 12) && (rr >= 0) && (rr < 12))
          {
            map |= (1 << (d+0));
            map |= (1 << (d+16));
          }
          const int cc2 = c + 2*dir_map_dx[d+1];
          const int rr2 = r + 2*dir_map_dy[d+1];
          if ((cc2 >= 0) && (cc2 < 12) && (rr2 >= 0) && (rr2 < 12))
          {
            map |= (1 << (d+8));
          }
        }
        printf("0x%06X, ", map);
      }
      printf("\n");
    }
    printf("};\n");
  }

  {
    int r, c;
    printf("static const unsigned in promotion_square_map[144] = {\n");
    for (r=0; r<12; ++r)
    {
      printf("  ");
      for (c=0; c<12; ++c)
      {
        int map = 0;
        if (r >= 8)
        {
          map |= (white<<24);
          if (r==11) map |= (1<<28);
          map |= PROMOTION_MASK;
          if (r==9 || r==10) map &= ~(1<<pawn);
        }
        else if (r<4)
        {
          map |= (black<<24);
          if (r==0) map |= (1<<28);
          map |= PROMOTION_MASK;
          if (r==1 || r==2) map &= ~(1<<pawn);
        }
        printf("0x%08X, ", map);
      }
      printf("\n");
    }
    printf("};\n");
  }

  return 0;
}
