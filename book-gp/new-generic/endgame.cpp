#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

/* chu-shogi endgame tablebase construction */

typedef unsigned long long ulli;

/*
template<typename T>
inline T min(T a, T b) { return a<b ? a : b; }
template<typename T>
inline T max(T a, T b) { return a>b ? a : b; }
*/

struct bitmap {
  unsigned int  n_bits;
  unsigned int  n_bytes;
  unsigned int  n_words;
  unsigned int* bits;
  bitmap(ulli n = 64) {
    n_bits = n;
    n_bytes = (n+7)/8;
    n_words = (n+31)/32;
    bits = new unsigned int[n_words];
    clear_all();
  }
  ~bitmap() {
    delete[] bits;
  }
  bitmap(const bitmap& that) {
    n_bits = that.n_bits;
    n_bytes = that.n_bytes;
    n_words = that.n_words;
    bits = new unsigned int[n_words];
    memcpy(bits, that.bits, n_bytes);
  }
  const bitmap& operator = (const bitmap& that) {
    delete[] bits;
    n_bits = that.n_bits;
    n_bytes = that.n_bytes;
    n_words = that.n_words;
    bits = new unsigned int[n_words];
    memcpy(bits, that.bits, n_bytes);
    return *this;
  }
  inline int get(ulli x) const {
    return (bits[x/32] >> (x%32))&1;
  }
  inline void set(ulli x) {
    bits[x/32] |= 1 << (x%32);
  }
  inline void clear(ulli x) {
    bits[x/32] &= ~(1 << (x%32));
  }
  void clear_all() {
    memset(bits, '\x00', n_bytes);
  }
  void set_all() {
    memset(bits, '\xFF', n_bytes);
  }
  struct iterator {
    const bitmap* bm;
    ulli    bit_number;
    ulli    word_number;
    int     is_done;

    iterator(const bitmap& bm_) : bm(&bm_) {
      reset();
    }
    void reset() {
      bit_number = 0;
      word_number = 0;
      is_done = 0;
      scan();
    }
    ulli operator *() const {
      return word_number*32 + bit_number;
    }
    void operator ++() {
      if (is_done) return;
      ++bit_number;
      scan();
    }
    operator bool() {
      return !is_done;
    }
    void scan() {
      for (; bit_number<32; ++bit_number)
        if (bm->bits[word_number] & (1<<bit_number))
          return;
      for (++word_number; word_number < bm->n_words; ++word_number) {
        unsigned int w = bm->bits[word_number];
        if (w) {
          for (bit_number=0; bit_number<32; ++bit_number)
            if (w & (1<<bit_number))
              return;
        }
      }
      is_done = 1;
    }
  };
};

/* captures represented by pieces with same location
 * to-move determines who captured whom
 * use confusing transpose order to assist with king symmetry
 */
static const int steps[8] = {+1-12,+1,+1+12,+12,-1+12,-1,-1-12,-12};
static const unsigned char k_mask = 0xFF;
static const unsigned char g_mask = 0xAF;
static const unsigned char g_mask_r = 0xFA; // reversed for un-moving
static const unsigned char col_mask[12] = {0x3E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE3};
static const unsigned char row_mask[12] = {0x8F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8};
struct position_kg_k {
  enum { NUM = 72llu*144llu*144llu };
  unsigned char wk;
  unsigned char wg;
  unsigned char bk;
  inline void set(unsigned char wk_, unsigned char wg_, unsigned char bk_)
  {
    wk = wk_;
    wg = wg_;
    bk = bk_;
    normalize();
  }
  inline void normalize()
  {
    if ((wk/12)>=6) {
      wk = (wk%12) + (11-(wk/12))*12;
      wg = (wg%12) + (11-(wg/12))*12;
      bk = (bk%12) + (11-(bk/12))*12;
    }
  }
  int successors(position_kg_k* pl, int wtm)
  {
    if (wtm) {
      int n = 0;
      const unsigned char km = k_mask & row_mask[wk%12] & col_mask[wk/12];
      for (int i=0; i<8; ++i)
        if (km & (1<<i))
          if (wk+steps[i]!=wg)
            pl[n++].set(wk + steps[i], wg, bk);
      const unsigned char gm = g_mask & row_mask[wg%12] & col_mask[wg/12];
      for (int i=0; i<8; ++i)
        if (gm & (1<<i))
          if (wg+steps[i]!=wk)
            pl[n++].set(wk, wg + steps[i], bk);
      return n;
    } else {
      int n = 0;
      const unsigned char km = k_mask & row_mask[bk%12] & col_mask[bk/12];
      for (int i=0; i<8; ++i)
        if (km & (1<<i))
          pl[n++].set(wk, wg, bk + steps[i]);
      return n;
    }
  }
  int predecessors(position_kg_k* pl, int wtm)
  {
    if (!wtm) {
      int n = 0;
      if (bk!=wg || wk==wg) {
        const unsigned char km = k_mask & row_mask[wk%12] & col_mask[wk/12];
        for (int i=0; i<8; ++i)
          if (km & (1<<i))
            if ((wk+steps[i]!=wg) && (wk+steps[i]!=bk))
              pl[n++].set(wk + steps[i], wg, bk);
      }
      if (bk!=wk) {
        const unsigned char gm = g_mask_r & row_mask[wg%12] & col_mask[wg/12];
        for (int i=0; i<8; ++i)
          if (gm & (1<<i))
            if ((wg+steps[i]!=wk) && (wg+steps[i]!=bk))
              pl[n++].set(wk, wg + steps[i], bk);
      }
      return n;
    } else {
      int n = 0;
      const unsigned char km = k_mask & row_mask[bk%12] & col_mask[bk/12];
      for (int i=0; i<8; ++i)
        if (km & (1<<i))
          if ((bk+steps[i]!=wk) && (bk+steps[i]!=wg))
            pl[n++].set(wk, wg, bk + steps[i]);
      return n;
    }
  }
  int is_terminal(int wtm, int& result) const {
    result = 0;
    if (wk==bk) {
      result = (wtm ? -(1<<12) : +(1<<12));
      return 1;
    } else if (wg==bk) {
      if (!wtm) {
        result = +(1<<12);
      } else {
        if (abs((wk%12)-(bk%12))<=1 && abs((wk/12)-(bk/12))<=1)
          result = +(1<<12);
        else
          result = 0;
      }
      return 1;
    } else {
      return 0;
    }
  }
  int is_valid(int wtm) const {
    return (wk<72) && (wg<144) && (bk<144)
        && (   ( wtm && (wk!=wg))
            || (!wtm && (wk==bk || wg==bk || wk!=wg))
           );
  }
  ulli to_index() const {
    // assumes normalized!
    return (wk)*144llu*144llu + wg*144llu + bk;
  }
  void from_index(ulli x) {
    bk = (x%144llu);
    wg = (x/144llu)%144llu;
    wk = (x/144llu)/144llu;
  }
  int showf(FILE* ff) const {
    int n = 0;
    n += fprintf(ff, "[wk=%c%-2i wg=%c%-2i | bk=%c%-2i]",
        /*wk,*/ 'a'+(wk/12), 1+wk%12,
        /*wg,*/ 'a'+(wg/12), 1+wg%12,
        /*bk,*/ 'a'+(bk/12), 1+bk%12);
    return n;
  }
};

struct entry {
  unsigned  valid   : 1;
  unsigned  known   : 1;
  signed    result  : 14;
  int showf(FILE* ff) const {
    int n = 0;
    n += fprintf(ff, "[%c%c%+4i]", valid?'V':'x', known?'!':'?', result);
    return n;
  }
};
struct table_kg_k {
  entry entries[position_kg_k::NUM][2];
  //bitmap  changed;
  table_kg_k() //: changed(position_kg_k::NUM)
  { }
  void initialize() {
    for (int i=0; i<position_kg_k::NUM; ++i) {
      int res;
      position_kg_k pos;
      pos.from_index(i);
      res = 0;
      entries[i][0].valid = pos.is_valid(0);
      entries[i][0].known = pos.is_terminal(0, res);
      entries[i][0].result = res;

      res = 0;
      entries[i][1].valid = pos.is_valid(1);
      entries[i][1].known = pos.is_terminal(1, res);
      entries[i][1].result = res;

    }
  }
  int one_step() {
    // naive approach
    // - just iterate through _entire_ space of positions and _forward_ generate to find lines
    int change_flag = 0;
    for (ulli x=0; x<position_kg_k::NUM; ++x) {
      if (entries[x][0].valid && !entries[x][0].known) {
        position_kg_k p, pl[16];
        p.from_index(x);
        int n = p.successors(pl, 0);
        int all_determined = 1;
        int best_result = +(1<<12)+1;
        for (int i=0; i<n; ++i) {
          ulli xs = pl[i].to_index();
          if (entries[xs][1].known) {
            best_result = min(best_result, entries[xs][1].result);
          } else {
            all_determined = 0;
          }
        }
        if (all_determined || best_result<0 ) {
          ++change_flag;
          entries[x][0].known = 1;
          if (best_result>0)
            entries[x][0].result = best_result-1;
          else if (best_result<0)
            entries[x][0].result = best_result+1;
          else
            entries[x][0].result = best_result;
        }
      }

      if (entries[x][1].valid && !entries[x][1].known) {
        position_kg_k p, pl[16];
        p.from_index(x);
        int n = p.successors(pl, 1);
        int all_determined = 1;
        int best_result = -(1<<12)-1;
        for (int i=0; i<n; ++i) {
          ulli xs = pl[i].to_index();
          if (entries[xs][0].known) {
            best_result = max(best_result, entries[xs][0].result);
          } else {
            all_determined = 0;
          }
        }
        if (all_determined || best_result>0 ) {
          ++change_flag;
          entries[x][1].known = 1;
          if (best_result>0)
            entries[x][1].result = best_result-1;
          else if (best_result<0)
            entries[x][1].result = best_result+1;
          else
            entries[x][1].result = best_result;
        }
      }
    }
    return change_flag;
  }

  void show_best(ulli x, int wtm) {
    for (int parity=0;;parity^=1) {
      position_kg_k p;
      p.from_index(x);

      printf("%+4i  %c  ", entries[x][wtm].result, wtm?'W':'B');
      p.showf(stdout);
      printf(parity ? "\n" : "\t");

      int dummy;
      if (p.is_terminal(wtm,dummy)) break;

      position_kg_k pl[16];
      int n = p.successors(pl,wtm);
      if (wtm) {
        int bestv = -(1<<12)-1;
        int besti = 0;
        for (int i=0; i<n; ++i) {
          ulli xs = pl[i].to_index();
          if (entries[xs][!wtm].result > bestv) {
            bestv = entries[xs][!wtm].result;
            besti = i;
          }
        }
        x = pl[besti].to_index();
      } else {
        int bestv = +(1<<12)+1;
        int besti = 0;
        for (int i=0; i<n; ++i) {
          ulli xs = pl[i].to_index();
          if (entries[xs][!wtm].result < bestv) {
            bestv = entries[xs][!wtm].result;
            besti = i;
          }
        }
        x = pl[besti].to_index();
      }
      wtm = !wtm;
    }
  }
};

bitmap  W(position_kg_k::NUM);
bitmap* Wi[64];
bitmap  B(position_kg_k::NUM);
bitmap* Bi[64];
bitmap  J(position_kg_k::NUM);
void thompson_approach()
{
  //////////////////////
  // Setup
  printf("(Setup)\n"); fflush(stdout);
  Wi[0] = new bitmap(position_kg_k::NUM); // unused
  Bi[0] = new bitmap(position_kg_k::NUM);
  // set all BTM/lost
  {
    int new_b = 0;
    for (ulli x=0; x<position_kg_k::NUM; ++x) {
      int res;
      position_kg_k pos;
      pos.from_index(x);
      res = 0;
      if (pos.is_valid(0) && pos.is_terminal(0, res)) {
        if (res>0) {
          B.set(x);
          Bi[0]->set(x);
          ++new_b;
        }
      }
    }
    printf("b%i", new_b);
  }
  int i = 0;
  int changed = 1;
  while (changed) {
    printf("\n***** %3i *****\n", i); fflush(stdout);
    changed = 0;
    Wi[i+1] = new bitmap(position_kg_k::NUM);
    Bi[i+1] = new bitmap(position_kg_k::NUM);

    //////////////////////
    // (A) for each Bi position, generate predecessors
    //     and add them to W and any positions not already
    //     there, form Wi+1
    {
      printf("(A)"); fflush(stdout);
      int new_w = 0;
      for (bitmap::iterator bi_it(*(Bi[i])); bi_it; ++bi_it) {
        ulli x = *bi_it;
        position_kg_k pos;
        pos.from_index(x);
        position_kg_k pl[16];
        int n = pos.predecessors(pl, 0);
        //pos.showf(stdout); printf(" (in B%i)\n", i);
        for (int k=0; k<n; ++k) {
          //printf(" <--- "); pl[k].showf(stdout); printf(" (to W,W%i)\n", i+1);
          ulli xk = pl[k].to_index();
          if (!W.get(xk)) {
            ++new_w;
            changed = 1;
            W.set(xk);
            Wi[i+1]->set(xk);
          }
        }
      }
      printf("\t+%iw", new_w);
    }
    //////////////////////
    // (B) for each Wi+1 position, generate predecessors into Ji+1
    {
      int new_j = 0;
      printf("(B)"); fflush(stdout);
      J.clear_all();
      for (bitmap::iterator wi1_it(*(Wi[i+1])); wi1_it; ++wi1_it) {
        ulli x = *wi1_it;
        position_kg_k pos;
        pos.from_index(x);
        position_kg_k pl[16];
        int n = pos.predecessors(pl, 1);
        //pos.showf(stdout); printf(" (in W%i)\n", i+1);
        for (int k=0; k<n; ++k) {
          //printf(" <--- "); pl[k].showf(stdout); printf(" (to J)\n");
          ulli xk = pl[k].to_index();
          J.set(xk);
          ++new_j;
        }
      }
      printf("j%i", new_j);
    }
    //////////////////////
    // (C) for each Ji+1 position, generate successors.
    //     if all are in W, then add position to Bi+1
    {
      int new_b = 0;
      printf("(C)"); fflush(stdout);
      for (bitmap::iterator ji1_it(J); ji1_it; ++ji1_it) {
        ulli x = *ji1_it;
        position_kg_k pos;
        pos.from_index(x);
        position_kg_k pl[16];
        int n = pos.successors(pl, 0);
        int all_in_w = 1;
        for (int k=0; k<n; ++k) {
          ulli xk = pl[k].to_index();
          if (!W.get(xk)) {
            all_in_w = 0;
            break;
          }
        }
        if (all_in_w) {
          Bi[i+1]->set(x);
          ++new_b;
          //pos.showf(stdout); printf(" (in J)\n");
          //for (int k=0; k<n; ++k) { printf(" --> "); pl[k].showf(stdout); printf(" (in W)\n"); }
        }
      }
      printf("b%i", new_b);
    }
    //////////////////////
    // (D) finally, add Bi+1 to B
    {
      printf("(D)"); fflush(stdout);
      int new_b = 0;
      for (bitmap::iterator bi1_it(*Bi[i+1]); bi1_it; ++bi1_it) {
        ulli x = *bi1_it;
        if (!B.get(x)) {
          B.set(x);
          changed = 1;
          ++new_b;
        }
      }
      printf("\t+%ib", new_b);
    }
    ++i;
    printf("\n"); fflush(stdout);
  }
  printf("Done\n"); fflush(stdout);
}

table_kg_k  tkgk;
int main(int argc, char* argv[])
{
  srand48(argc>1 ? atoi(argv[1]) : -13);
  printf("%i\n", position_kg_k::NUM); fflush(stdout);

  printf("*** thompson approach ***\n");
  ui64 bt_th = read_clock();
  thompson_approach();
  ui64 et_th = read_clock();

  printf("*** naive approach ***\n");
  ui64 bt_na = read_clock();
  tkgk.initialize();
  int change_flag = 0;
  int iter = 0;
  do {
    ++iter;
    change_flag = tkgk.one_step();
    printf("%3i  %12i\n", iter, change_flag); fflush(stdout);
  } while (change_flag);
  ui64 et_na = read_clock();

  // find longest white win
  int maxv=(1<<12)+1;
  ulli maxx=0;
  for (ulli x=0; x<position_kg_k::NUM; ++x) {
    if (tkgk.entries[x][1].result > 0) {
      if (tkgk.entries[x][1].result < maxv) {
        maxv = tkgk.entries[x][1].result;
        maxx = x;
      }
    }
  }
  tkgk.show_best(maxx, 1);

  ulli w_win_th = 0, b_lose_th = 0;
  ulli w_win_na = 0, b_lose_na = 0;
  for (ulli x=0; x<position_kg_k::NUM; ++x) {
    if (W.get(x))
      ++w_win_th;
    if (tkgk.entries[x][1].valid && tkgk.entries[x][1].result>0)
      ++w_win_na;
    if (B.get(x))
      ++b_lose_th;
    if (tkgk.entries[x][0].valid && tkgk.entries[x][0].result>0)
      ++b_lose_na;

    if (W.get(x) != (tkgk.entries[x][1].valid && tkgk.entries[x][1].result>0)) {
      position_kg_k p; p.from_index(x);
      printf("W %i  %i,%i  ", W.get(x), tkgk.entries[x][1].valid, tkgk.entries[x][1].result);
      p.showf(stdout); printf("\n");
    }
    if (B.get(x) != (tkgk.entries[x][0].valid && tkgk.entries[x][0].result>0)) {
      position_kg_k p; p.from_index(x);
      printf("B %i  %i,%i  ", B.get(x), tkgk.entries[x][0].valid, tkgk.entries[x][0].result);
      p.showf(stdout); printf("\n");
    }
  }

  printf("%llu %llu\n", w_win_th, w_win_na);
  printf("%llu %llu\n", b_lose_th, b_lose_na);

  printf("Thompson = %8.4fs\n", (et_th-bt_th)*1e-6);
  printf("Naive    = %8.4fs\n", (et_na-bt_na)*1e-6);

  return 0;
}

