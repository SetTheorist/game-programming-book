#ifndef INCLUDED_EVALT_H_
#define INCLUDED_EVALT_H_

#include <stdio.h>

/* **************************************** 
 * Simple integer-based evaluation type
 * - assumes that stalemate == draw == 0
 * **************************************** */
struct i_evalt
{
  int v;
  i_evalt(int v_=0) : v(v_) { }
  inline bool operator == (i_evalt b) const { return v == b.v; }
  inline bool operator != (i_evalt b) const { return v != b.v; }
  inline bool operator <= (i_evalt b) const { return v <= b.v; }
  inline bool operator >= (i_evalt b) const { return v >= b.v; }
  inline bool operator < (i_evalt b) const { return v < b.v; }
  inline bool operator > (i_evalt b) const { return v > b.v; }
  inline bool operator == (int i) const { return v == i; }
  inline bool operator != (int i) const { return v != i; }
  inline bool operator <= (int i) const { return v <= i; }
  inline bool operator >= (int i) const { return v >= i; }
  inline bool operator < (int i) const { return v < i; }
  inline bool operator > (int i) const { return v > i; }
  inline i_evalt operator + (int i) const { i_evalt x(v+i); return x; }
  inline i_evalt operator - (int i) const { i_evalt x(v-i); return x; }
  inline i_evalt operator * (int i) const { i_evalt x(v*i); return x; }
  inline i_evalt operator / (int i) const { i_evalt x(v/i); return x; }
  inline i_evalt operator - () const { i_evalt x(-v); return x; }
  inline operator int () const { return v; }
  inline const i_evalt& operator = (int i) { v=i; return *this; }
  inline const i_evalt& operator += (i_evalt i) { v+=i; return *this; }
  inline const i_evalt& operator -= (i_evalt i) { v-=i; return *this; }
  inline const i_evalt& operator *= (i_evalt i) { v*=i; return *this; }
  inline const i_evalt& operator /= (i_evalt i) { v/=i; return *this; }
  int PLIES_TO_WIN() const { return W_INF-abs(v); }
  static const i_evalt W_INF;
  static const i_evalt B_INF;
  static const i_evalt STALEMATE_RELATIVE;
  static i_evalt STALEMATE_RELATIVE_IN_N(int n) { i_evalt x(0-n); return x; }
  static i_evalt W_WIN_IN_N(int n) { i_evalt x(W_INF-n); return x; }
  static i_evalt B_WIN_IN_N(int n) { i_evalt x(B_INF+n); return x; }
  int showf(FILE* f) {
    if (*this>W_WIN_IN_N(1000))
      return fprintf(f, "Win-in-%i", PLIES_TO_WIN());
    else if (*this<B_WIN_IN_N(1000))
      return fprintf(f, "Lose-in-%i", PLIES_TO_WIN());
    else
      return fprintf(f, "v%i", v);
  }
};
const i_evalt i_evalt::W_INF( 1000000000 );
const i_evalt i_evalt::B_INF( -i_evalt::W_INF );
const i_evalt i_evalt::STALEMATE_RELATIVE( 0 );
/* **************************************** */


/* **************************************** 
 * Simple double-based evaluation type
 * - assumes that stalemate == draw == 0
 * **************************************** */
struct d_evalt
{
  double v;
  d_evalt(double v_=0.0) : v(v_) { }
  inline bool operator == (d_evalt b) const { return v == b.v; }
  inline bool operator != (d_evalt b) const { return v != b.v; }
  inline bool operator <= (d_evalt b) const { return v <= b.v; }
  inline bool operator >= (d_evalt b) const { return v >= b.v; }
  inline bool operator < (d_evalt b) const { return v < b.v; }
  inline bool operator > (d_evalt b) const { return v > b.v; }
  inline bool operator == (double d) const { return v == d; }
  inline bool operator != (double d) const { return v != d; }
  inline bool operator <= (double d) const { return v <= d; }
  inline bool operator >= (double d) const { return v >= d; }
  inline bool operator < (double d) const { return v < d; }
  inline bool operator > (double d) const { return v > d; }
  inline bool operator == (int i) const { return v == (double)i; }
  inline bool operator != (int i) const { return v != (double)i; }
  inline bool operator <= (int i) const { return v <= (double)i; }
  inline bool operator >= (int i) const { return v >= (double)i; }
  inline bool operator < (int i) const { return v < (double)i; }
  inline bool operator > (int i) const { return v > (double)i; }
  inline d_evalt operator + (double d) const { d_evalt x(v+d); return x; }
  inline d_evalt operator - (double d) const { d_evalt x(v-d); return x; }
  inline d_evalt operator * (double d) const { d_evalt x(v*d); return x; }
  inline d_evalt operator / (double d) const { d_evalt x(v/d); return x; }
  inline d_evalt operator + (int i) const { d_evalt x(v+i); return x; }
  inline d_evalt operator - (int i) const { d_evalt x(v-i); return x; }
  inline d_evalt operator * (int i) const { d_evalt x(v*i); return x; }
  inline d_evalt operator / (int i) const { d_evalt x(v/i); return x; }
  inline d_evalt operator - () const { d_evalt x(-v); return x; }
  inline operator double () const { return v; }
  inline const d_evalt& operator = (int i) { v=(double)i; return *this; }
  inline const d_evalt& operator = (double d) { v=d; return *this; }
  inline const d_evalt& operator += (d_evalt d) { v+=d; return *this; }
  inline const d_evalt& operator -= (d_evalt d) { v-=d; return *this; }
  inline const d_evalt& operator *= (d_evalt d) { v*=d; return *this; }
  inline const d_evalt& operator /= (d_evalt d) { v/=d; return *this; }
  int PLIES_TO_WIN() const { return (int)(W_INF-fabs(v)); }
  static const d_evalt W_INF;
  static const d_evalt B_INF;
  static const d_evalt STALEMATE_RELATIVE;
  static d_evalt STALEMATE_RELATIVE_IN_N(int n) { d_evalt x(0.0-n); return x; }
  static d_evalt W_WIN_IN_N(int n) { d_evalt x(W_INF-(double)n); return x; }
  static d_evalt B_WIN_IN_N(int n) { d_evalt x(B_INF+(double)n); return x; }
  int showf(FILE* f) {
    if (*this>W_WIN_IN_N(1000.0))
      return fprintf(f, "Win-in-%i", PLIES_TO_WIN());
    else if (*this<B_WIN_IN_N(1000.0))
      return fprintf(f, "Lose-in-%i", PLIES_TO_WIN());
    else
      return fprintf(f, "v%g", v);
  }
};
const d_evalt d_evalt::W_INF( 1000000000.0 );
const d_evalt d_evalt::B_INF( -d_evalt::W_INF );
const d_evalt d_evalt::STALEMATE_RELATIVE( 0.0 );
/* **************************************** */

#endif /*INCLUDED_EVALT_H_*/
