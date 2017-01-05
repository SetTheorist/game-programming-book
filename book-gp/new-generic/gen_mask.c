#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// for knight-jump only
#define NNE (1<<0)
#define EEN (1<<1)
#define EES (1<<2)
#define SSE (1<<3)
#define SSW (1<<4)
#define WWS (1<<5)
#define WWN (1<<6)
#define NNW (1<<7)

#define X1(n) ((n))
#define X2(n) ((n)<<8)
#define X3(n) ((n)<<16)
#define Xk(n) ((n)<<16) // overloaded - only for heavenly_horse = promoted liberated_horse
#define Xn(n) ((n)<<24)

const int step_delt[8][2] = { {+1,00},{+1,+1},{00,+1},{-1,+1},{-1,00},{-1,-1},{00,-1},{+1,-1} };
const int step_delt_k[8][2] = { {+2,+1},{+1,+2},{-1,+2},{-2,+1},{-2,-1},{-1,-2},{+1,-2},{+2,-1} };

unsigned int mask[11*11];
unsigned int mask_k[11*11];

int main() {
  int i, j, k;
  memset(mask, '\x00', sizeof(mask));
  memset(mask_k, '\x00', sizeof(mask));

  for (i=0; i<11; ++i) {
    for (j=0; j<11; ++j) {
      for (k=0; k<8; ++k) {
        int di = step_delt[k][0];
        int dj = step_delt[k][1];
        if ((i+di)>=0 && (i+di)<11 && (j+dj)>=0 && (j+dj)<11)
          mask[i*11+j] |= 1<<(   k);
        if ((i+2*di)>=0 && (i+2*di)<11 && (j+2*dj)>=0 && (j+2*dj)<11)
          mask[i*11+j] |= 1<<( 8+k);
        if ((i+3*di)>=0 && (i+3*di)<11 && (j+3*dj)>=0 && (j+3*dj)<11)
          mask[i*11+j] |= 1<<(16+k);
        if ((i+di)>=0 && (i+di)<11 && (j+dj)>=0 && (j+dj)<11)
          mask[i*11+j] |= 1<<(24+k);

        int ki = step_delt_k[k][0];
        int kj = step_delt_k[k][1];
        if ((i+ki)>=0 && (i+ki)<11 && (j+kj)>=0 && (j+kj)<11)
          mask_k[i*11+j] |= 1<<(16+k);
      }
    }
  }

  printf("static const unsigned int board_move_mask[11*11] = {\n");
  for (i=0; i<11; ++i) {
    printf("  ");
    for (j=0; j<11; ++j) {
      printf("0x%08X,", mask[i*11+j]);
    }
    printf("\n");
  }
  printf("};\n");
  printf("static const unsigned int board_move_mask_k[11*11] = {\n");
  for (i=0; i<11; ++i) {
    printf("  ");
    for (j=0; j<11; ++j) {
      printf("0x%08X,", mask_k[i*11+j]);
    }
    printf("\n");
  }
  printf("};\n");
  return 0;
}
