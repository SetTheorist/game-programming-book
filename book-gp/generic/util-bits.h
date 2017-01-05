#ifndef INCLUDED_UTIL_BITS_H_
#define INCLUDED_UTIL_BITS_H_
/* $Id: util-bits.h,v 1.1 2010/12/26 18:08:13 apollo Exp $ */

static const int debruijn_index64[64] = {
  63,  0, 58,  1, 59, 47, 53,  2,
  60, 39, 48, 27, 54, 33, 42,  3,
  61, 51, 37, 40, 49, 18, 28, 20,
  55, 30, 34, 11, 43, 14, 22,  4,
  62, 57, 46, 52, 38, 26, 32, 41,
  50, 36, 17, 19, 29, 10, 13, 21,
  56, 45, 25, 31, 35, 16,  9, 12,
  44, 24, 15,  8, 23,  7,  6,  5
};
// requires bb != 0
static inline int bit_scan_forward64(ui64 bb) {
  return debruijn_index64[((bb & -bb) * 0x07EDD5E59A4E28C2ULL) >> 58];
}
static const int debruijn_index32[32] = {
  0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
  31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};
// requires bb != 0
static inline int bit_scan_forward32(ui32 bb) {
  return debruijn_index32[((ui32)((bb & -bb) * 0x077CB531U)) >> 27];
}
static int  count_bits32(ui32 x) {
  x  = x - ((x >> 1) & 0x55555555);
  x  = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  x  = x + (x >> 4);
  x &= 0xF0F0F0F;
  return (x * 0x01010101) >> 24;
}
#define CB_m1  (0x5555555555555555ULL)
#define CB_m2  (0x3333333333333333ULL)
#define CB_m4  (0x0f0f0f0f0f0f0f0fULL)
#define CB_m8  (0x00ff00ff00ff00ffULL)
#define CB_m16 (0x0000ffff0000ffffULL)
#define CB_m32 (0x00000000ffffffffULL)
#define CB_hff (0xffffffffffffffffULL)
#define CB_h01 (0x0101010101010101ULL)
static int count_bits64(ui64 x) {
  x -= (x >> 1) & CB_m1;
  x = (x & CB_m2) + ((x >> 2) & CB_m2);
  x = (x + (x >> 4)) & CB_m4;
  return (x * CB_h01)>>56;
}

#endif /* INCLUDED_UTIL_BITS_H_ */
