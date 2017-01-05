#include <ctype.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "evalt.h"
#include "gen-search.h"
#include "opening.h"
#include "player.h"
#include "td-lambda.h"

/* ************************************************************ */
/* ************************************************************ */

/* ****************************************
 * Alapo rewrite
 * **************************************** */

namespace alapo {

//typedef i_evalt eval;
typedef d_evalt eval;

enum pieceft
{
  diag=1, ortho=2, runner=4,
};
enum piecet
{
  empty  = 0, ferz   = diag,        wazir = ortho,        king  = diag|ortho,
  border = 4, bishop = diag|runner, rook  = ortho|runner, queen = diag|ortho|runner,
  NPIECES=8
};
/* ferz, wazir, king, bishop, rook, queen */
static const char piece_abbr[] = " FWK#BRQ?";
static const char* piece_name[] = { "empty",  "ferz",   "wazir", "king", "border", "bishop", "rook",  "queen", };
enum color
{
  none=0, white=1, black=2, wall=3,
  NCOLORS,
};
struct move
{
  union {
    int i;
    struct {
      unsigned w  : 1;  //  1    white moving?
      unsigned p  : 3;  //  2- 4 piece
      unsigned f  : 6;  //  5-10 from square
      unsigned t  : 6;  // 11-16 to square
      unsigned fc : 1;  // 17    capture?
      unsigned cp : 3;  // 18-20 captured piece
    };
  };
  move(int ii=0) : i(ii) { }
  bool valid() { return true; }
  bool null() { return f==t; }
  static const move nullmove;
  int showf(FILE* ff) const {
    fprintf(ff, "%c", "bw"[w]);
    if (f==t) {
      return fprintf(ff, "<pass>");
    } else {
      int tot = 0;
      tot += fprintf(ff, "%c", piece_abbr[p]);
      tot += fprintf(ff, "%c%c", 'a'+((f%8)-1), '1'+((f/8)-1));
      if (fc)
        tot += fprintf(ff, "*%c", piece_abbr[cp]);
      else
        tot += fprintf(ff, "%c", '-');
      tot += fprintf(ff, "%c%c", 'a'+((t%8)-1), '1'+((t/8)-1));
      if ((w && t>=6*8)||(!w && t<2*8)) tot += fprintf(ff, "+");
      return tot;
    }
  }
};
const move move::nullmove(0);
/* complicated board representation: 8x8 with border of "border" pieces... */
struct board
{
  static const int MAXNMOVES = 64;
  static const piecet init_pieces[8*8];
  static const color init_colors[8*8];
  static const eval init_piece_vals[NPIECES];
  static const eval init_piece_square_vals[8*8][NPIECES];

  piecet  pieces[8*8];
  color   colors[8*8];

  color   to_move;
  int     is_terminal;
  color   result;
  int     piece_counts[2];

  int     num_moves;
  move    move_stack[512];
  hasht   hash_stack[512];

  eval material_value;
  eval piece_vals[NPIECES];
  eval square_value;
  eval piece_square_vals[8*8][NPIECES];

  hasht   piece_hash[8*8][NCOLORS][NPIECES];
  hasht   to_move_hash[NCOLORS];
  hasht   to_move_hash_wb;

  board() {
    init();
    init_hash();
    hash_stack[num_moves] = compute_hash();
  }

  void init_hash() {
    for (int i=0; i<8*8; ++i)
      for (int j=0; j<NCOLORS; ++j)
        for (int k=0; k<NPIECES; ++k)
          piece_hash[i][j][k] = gen_hash();
    for (int i=0; i<8*8; ++i)
      piece_hash[i][none][empty] = 0;
    for (int i=0; i<NCOLORS; ++i)
      to_move_hash[i] = gen_hash();
    to_move_hash_wb = to_move_hash[white]^to_move_hash[black];
  }

  void init() {
    memset(this, '\x00', sizeof(*this));
    memcpy(pieces, init_pieces, sizeof(pieces));
    memcpy(colors, init_colors, sizeof(colors));
    memcpy(piece_vals, init_piece_vals, sizeof(piece_vals));
    memcpy(piece_square_vals, init_piece_square_vals, sizeof(piece_square_vals));
    to_move = white;
    result = none;
    piece_counts[0] = piece_counts[1] = 12;
  }

  eval compute_material_value() const {
    eval val = 0;
    for (int i=1*8+1; i<7*8; ++i)
      if (colors[i]==white)
        val += piece_vals[pieces[i]];
      else if (colors[i]==black)
        val -= piece_vals[pieces[i]];
    return val;
  }

  void set_piece_vals(const eval* new_vals) {
    memcpy(piece_vals, new_vals, sizeof(piece_vals));
    material_value = compute_material_value();
  }

  eval compute_square_value() const {
    eval val = 0;
    for (int i=1*8+1; i<7*8; ++i)
      if (colors[i]==white)
        val += piece_square_vals[i][pieces[i]];
      else if (colors[i]==black)
        val -= piece_square_vals[8*8-1-i][pieces[i]];
    return val;
  }

  void set_piece_square_vals(const eval* new_vals) {
    memcpy(piece_square_vals, new_vals, sizeof(piece_square_vals));
    // recompute piece-square value
    square_value = compute_square_value();
  }

  int in_check(color c) const { 
    int base = (c==white) ? (1*8+1) : (6*8+1);
    color xc = (color)(c^3);
    for (int i=0; i<6; ++i)
      if (colors[base+i]==xc)
        return true;
    return false;
  }

  inline void add_move(move ml[], int* n, int ff, int delta, piecet p, int run) const {
    int t = ff+delta;
    int ok = 1;
    while (ok && !(colors[t] & to_move)) {
      ml[*n].i = 0;
      ml[*n].p = p;
      ml[*n].w = to_move==white;
      ml[*n].f = ff;
      ml[*n].t = t;
      if (colors[t]!=none) {
        ok = 0;
        ml[*n].fc = 1;
        ml[*n].cp = pieces[t];
      }
      ++*n;
      t += delta;
      if (!run) break;
    }
  }
  int gen_moves(move ml[]) const {
    if (is_terminal) return 0;
    int n = 0;
    for (int i=1*8+1; i<8*7; ++i) {
      if (colors[i]==to_move) {
        if (pieces[i]&ortho) {
          add_move(ml, &n, i, +1, pieces[i], pieces[i]&runner);
          add_move(ml, &n, i, -1, pieces[i], pieces[i]&runner);
          add_move(ml, &n, i, +8, pieces[i], pieces[i]&runner);
          add_move(ml, &n, i, -8, pieces[i], pieces[i]&runner);
        }
        if (pieces[i]&diag) {
          add_move(ml, &n, i, +1+8, pieces[i], pieces[i]&runner);
          add_move(ml, &n, i, -1+8, pieces[i], pieces[i]&runner);
          add_move(ml, &n, i, +1-8, pieces[i], pieces[i]&runner);
          add_move(ml, &n, i, -1-8, pieces[i], pieces[i]&runner);
        }
      }
    }
    if (n>64) fprintf(stderr, "!!!!! %i>64 moves generated !!!!!\n", n);
    return n;
  }
  inline void add_cap(move ml[], int* n, int ff, int delta, piecet p, int run) const {
    int t = ff+delta;
    int ok = 1;
    while (ok && !(colors[t] & to_move)) {
      if (colors[t]!=none || (to_move==white && t>6*8) || (to_move==black && t<2*8)) {
        ml[*n].i = 0;
        ml[*n].p = p;
        ml[*n].w = to_move==white;
        ml[*n].f = ff;
        ml[*n].t = t;
        if (colors[t]!=none) {
          ok = 0;
          ml[*n].fc = 1;
          ml[*n].cp = pieces[t];
        }
        ++*n;
      }
      t += delta;
      if (!run) break;
    }
  }
  int gen_moves_quiescence(move ml[]) const {
    if (is_terminal) return 0;
    int n = 0;
    for (int i=1*8+1; i<8*7; ++i) {
      if (colors[i]==to_move) {
        if (pieces[i]&ortho) {
          add_cap(ml, &n, i, +1, pieces[i], pieces[i]&runner);
          add_cap(ml, &n, i, -1, pieces[i], pieces[i]&runner);
          add_cap(ml, &n, i, +8, pieces[i], pieces[i]&runner);
          add_cap(ml, &n, i, -8, pieces[i], pieces[i]&runner);
        }
        if (pieces[i]&diag) {
          add_cap(ml, &n, i, +1+8, pieces[i], pieces[i]&runner);
          add_cap(ml, &n, i, -1+8, pieces[i], pieces[i]&runner);
          add_cap(ml, &n, i, +1-8, pieces[i], pieces[i]&runner);
          add_cap(ml, &n, i, -1-8, pieces[i], pieces[i]&runner);
        }
      }
    }
    return n;
  }
  int make_moves(int n, const move* ml) {
    for (int i=0; i<n; ++i)
      make_move(ml[i]);
    return n;
  }
  make_move_flags make_move(move m) {
    hasht h = hash_stack[num_moves];

    if (m.f != m.t) {
      h ^= piece_hash[m.f][colors[m.f]][pieces[m.f]]
         ^ piece_hash[m.t][colors[m.t]][pieces[m.t]]
         ^ piece_hash[m.t][colors[m.f]][pieces[m.f]];
      colors[m.t] = colors[m.f];
      pieces[m.t] = pieces[m.f];
      colors[m.f] = none;
      pieces[m.f] = empty;

      if (m.w) {
        square_value -= piece_square_vals[m.f][m.p];
        square_value += piece_square_vals[m.t][m.p];
      } else {
        square_value -= -piece_square_vals[8*8-1-m.f][m.p];
        square_value += -piece_square_vals[8*8-1-m.t][m.p];
      }

      if (m.fc) {
        --piece_counts[m.w];
        material_value += m.w ? piece_vals[m.cp] : -piece_vals[m.cp];
        if (m.w) square_value -= -piece_square_vals[8*8-1-m.t][m.cp];
        else     square_value -=  piece_square_vals[m.t][m.cp];
      }
    }

    if (in_check(to_move)) {
      is_terminal = 1;
      result = (color)(to_move^3);
    } else if (!piece_counts[m.w]) {
      is_terminal = 1;
      result = to_move;
    }

    move_stack[num_moves++] = m;
    to_move = (color)(to_move^3);
    h ^= to_move_hash_wb;
    //hash_stack[num_moves] = compute_hash();
    hash_stack[num_moves] = h;

    // check for repetition
    for (int k=0; k<num_moves; ++k) {
      if (hash_stack[k] == hash_stack[num_moves]) {
        is_terminal = 1;
        //result = none;
        result = to_move;
      }
    }

    return ok_move;
  }
  int unmake_moves(int n) {
    for (int i=0; i<n; ++i)
      unmake_move();
    return n;
  }
  make_move_flags unmake_move() {
    if (!num_moves) return invalid_move;
    move m = move_stack[--num_moves];
    to_move = (color)(to_move^3);
    is_terminal = 0;
    result = none;
    if (m.f != m.t) {
      colors[m.f] = colors[m.t];
      pieces[m.f] = pieces[m.t];
      colors[m.t] = none;
      pieces[m.t] = empty;
      if (m.w) {
        square_value += piece_square_vals[m.f][m.p];
        square_value -= piece_square_vals[m.t][m.p];
      } else {
        square_value += -piece_square_vals[8*8-1-m.f][m.p];
        square_value -= -piece_square_vals[8*8-1-m.t][m.p];
      }
      if (m.fc) {
        colors[m.t] = (color)(to_move^3);
        pieces[m.t] = (piecet)m.cp;
        ++piece_counts[m.w];
        material_value -= m.w ? piece_vals[m.cp] : -piece_vals[m.cp];
        if (m.w) square_value += -piece_square_vals[8*8-1-m.t][m.cp];
        else     square_value +=  piece_square_vals[m.t][m.cp];
      }
    }
    return ok_move;
  }

  bool terminal() const {
    return is_terminal;
  }

  hasht hash() const { return hash_stack[num_moves]; }

  hasht compute_hash() const {
    hasht h = to_move_hash[to_move];
    for (int i=0; i<8*8; ++i)
      h ^= piece_hash[i][colors[i]][pieces[i]];
    return h;
  }

  move last_move() const { return move_stack[num_moves-1]; }

  int showf(FILE* ff) const {
    int n = 0;
    n += fprintf(ff, "     a     b     c     d     e     f   \n");
    n += fprintf(ff, "  +-----+-----+-----+-----+-----+-----+\n");
    for (int r=6; r>=1; --r) {
      n += fprintf(ff, "%i | ",r);
      for (int c=1; c<=6; ++c) {
        n += fprintf(ff, "%c", " wb@"[colors[r*8+c]]);
        n += fprintf(ff, " ");
        if (colors[r*8+c]) {
          n += fprintf(ff, "%c", piece_abbr[pieces[r*8+c]]);
        } else {
          n += fprintf(ff, " ");
        }
        n += fprintf(ff, c==6 ? " |" : " + ");
      }
      if (r==5) n += fprintf(ff, "[%016llX][%016llX]", hash(), compute_hash());
      if (r==3) n += fprintf(ff, "[%2i]", num_moves);
      if (r==2) n += fstatus<move,eval>(ff, "{%q:%q}{%q:%q} (%i,%i)",
          material_value, compute_material_value(), square_value, compute_square_value(),
          piece_counts[0], piece_counts[1]); 
      if (r==1) n += fprintf(ff, "%c %c", in_check(white)?'W':' ', in_check(black)?'B':' ');
      n += fprintf(ff, "\n");
    }
    n += fprintf(ff, "  +-----+-----+-----+-----+-----+-----+\n");
    if (is_terminal) {
      n += fprintf(ff, "** GAME OVER: %s **\n",
          result==none?"1/2" : result==white?"1-0" : result==black?"0-1" : "???");
    } else {
      n += fprintf(ff, "%s to move\n", to_move==white?"White":to_move==black?"Black":"?");
    }
    return 0;
  }
};
const eval board::init_piece_vals[NPIECES] = {
//  0, 100, 200, 350, 0, 500, 700, 1100
  0, 100, 100, 215, 0, 225, 400, 500
};
const eval board::init_piece_square_vals[8*8][NPIECES];
const piecet board::init_pieces[8*8] = {
  border, border, border, border, border, border, border, border, 
  border, rook,   bishop, queen,  queen,  bishop, rook,   border,
  border, wazir,  ferz,   king,   king,   ferz,   wazir,  border,
  border, empty,  empty,  empty,  empty,  empty,  empty,  border,
  border, empty,  empty,  empty,  empty,  empty,  empty,  border,
  border, wazir,  ferz,   king,   king,   ferz,   wazir,  border,
  border, rook,   bishop, queen,  queen,  bishop, rook,   border,
  border, border, border, border, border, border, border, border, 
};
const color board::init_colors[8*8] = {
  wall, wall,  wall,  wall,  wall,  wall,  wall,  wall,
  wall, white, white, white, white, white, white, wall,
  wall, white, white, white, white, white, white, wall,
  wall, none,  none,  none,  none,  none,  none,  wall,
  wall, none,  none,  none,  none,  none,  none,  wall,
  wall, black, black, black, black, black, black, wall,
  wall, black, black, black, black, black, black, wall,
  wall, wall,  wall,  wall,  wall,  wall,  wall,  wall,
};

struct evaluator {
  static const eval init_piece_vals[NPIECES];
  static const eval init_piece_square_vals[8*8][NPIECES];

  enum {
    piece_vals = 0,
    num_piece_vals = NPIECES,
    piece_square_vals = (piece_vals+num_piece_vals),
    num_piece_square_vals = 8*8*NPIECES,
    };

  eval weights[num_piece_vals+num_piece_square_vals];
  int check_extension;
  int use_piece_square_vals;

  evaluator(int ce=50, int upsv=1) : check_extension(ce), use_piece_square_vals(upsv) {
    memcpy(weights+piece_vals, init_piece_vals, sizeof(eval)*num_piece_vals);
    memcpy(weights+piece_square_vals, init_piece_square_vals, sizeof(eval)*num_piece_square_vals);
  }

  const eval* get_piece_vals() const { return weights+piece_vals; }
  const eval* get_piece_square_vals() const { return weights+piece_square_vals; }

  int num_weights() const { return num_piece_vals+num_piece_square_vals; }
  const eval* get_weights() const { return weights; }
  eval get_weight(int i) const { return weights[i]; }
  void set_weights(const eval* new_weights) {
    memcpy(weights, new_weights, sizeof(weights));
    normalize_weights();
  }
  void set_weight(int i, eval new_weight) {
    weights[i] = new_weight;
  }
  void get_weights_double(double* darr) const {
    for (int i=0; i<num_weights(); ++i)
      darr[i] = (double)weights[i];
  }
  double get_weight_double(int i) const { return (double)weights[i]; }
  void set_weights_double(const double* darr) {
    for (int i=0; i<num_weights(); ++i)
      weights[i] = darr[i];
    normalize_weights();
  }
  void set_weight_double(int i, double d) {
    weights[i] = d;
  }

  void normalize_weights() {
    (weights+piece_vals)[empty] = (weights+piece_vals)[runner] = 0.0;
    // normalize ferz = 100
    {
      double w = (double)(weights+piece_vals)[ferz]/100.0;
      for (int i=0; i<num_piece_vals; ++i)
        (weights+piece_vals)[i] /= w;
    }
    // normalize each piece square adj at starting = 0
    if (use_piece_square_vals)
    {
      { double w = ((weights+piece_square_vals)[(2*8+1)*NPIECES+wazir] + (weights+piece_square_vals)[(2*8+6)*NPIECES+wazir])/2.0;
        for (int i=0; i<8*8; ++i) (weights+piece_square_vals)[i*NPIECES+wazir] -= w; }
      { double w = ((weights+piece_square_vals)[(2*8+2)*NPIECES+ferz] + (weights+piece_square_vals)[(2*8+5)*NPIECES+ferz])/2.0;
        for (int i=0; i<8*8; ++i) (weights+piece_square_vals)[i*NPIECES+ferz] -= w; }
      { double w = ((weights+piece_square_vals)[(2*8+3)*NPIECES+king] + (weights+piece_square_vals)[(2*8+4)*NPIECES+king])/2.0;
        for (int i=0; i<8*8; ++i) (weights+piece_square_vals)[i*NPIECES+king] -= w; }
      { double w = ((weights+piece_square_vals)[(1*8+1)*NPIECES+rook] + (weights+piece_square_vals)[(1*8+6)*NPIECES+rook])/2.0;
        for (int i=0; i<8*8; ++i) (weights+piece_square_vals)[i*NPIECES+rook] -= w; }
      { double w = ((weights+piece_square_vals)[(1*8+2)*NPIECES+bishop] + (weights+piece_square_vals)[(1*8+5)*NPIECES+bishop])/2.0;
        for (int i=0; i<8*8; ++i) (weights+piece_square_vals)[i*NPIECES+bishop] -= w; }
      { double w = ((weights+piece_square_vals)[(1*8+3)*NPIECES+queen] + (weights+piece_square_vals)[(1*8+4)*NPIECES+queen])/2.0;
        for (int i=0; i<8*8; ++i) (weights+piece_square_vals)[i*NPIECES+queen] -= w; }
    }
    // normalize for horizontal symmetry of piece square adj
    if (use_piece_square_vals)
    {
      for (int c=1; c<=3; ++c) {
        for (int r=1; r<=6; ++r) {
          for (int p=0; p<NPIECES; ++p) {
            double w = ((weights+piece_square_vals)[(r*8+c)*NPIECES+p] + (weights+piece_square_vals)[(r*8+(7-c))*NPIECES+p])/2.0;
            (weights+piece_square_vals)[(r*8+c)*NPIECES+p] = w;
            (weights+piece_square_vals)[(r*8+(7-c))*NPIECES+p] = w;
          }
        }
      }
    }
  }
  
  void evaluate_moves_for_search(board* /*b*/, move* ml, eval* scorel, int n) const {
    for (int i=0; i<n; ++i)
      scorel[i] = i + (ml[i].fc ? (1000+10*((weights+piece_vals)[ml[i].cp] - (weights+piece_vals)[ml[i].p])) : 0);
  }
  eval compute_evaluation_absolute(const board* b, int ply, eval /*alpha*/, eval /*beta*/) const {
    if (b->terminal()) {
      if (!b->result) return eval::STALEMATE_RELATIVE_IN_N(ply);
      if (b->result == white)
        return eval::W_WIN_IN_N(ply);
      else
        return eval::B_WIN_IN_N(ply);
    }
    eval total = (int)((b->hash()%11)-5);
    for (int i=1*8+1; i<7*8; ++i)
      if (b->colors[i]==white)
        total += (weights+piece_vals)[b->pieces[i]] + (weights+piece_square_vals)[(i)*NPIECES + b->pieces[i]]*use_piece_square_vals;
      else if (b->colors[i]==black)
        total -= (weights+piece_vals)[b->pieces[i]] + (weights+piece_square_vals)[(8*8-1-i)*NPIECES + b->pieces[i]]*use_piece_square_vals;
    return total;
  }
  eval compute_evaluation_relative(const board* b, int ply, eval alpha, eval beta) const {
    eval absev = compute_evaluation_absolute(b, ply, alpha, beta);
    return (b->to_move==white) ? absev : -absev;
  }
  eval evaluate_absolute(const board* b, int ply, eval /*alpha*/, eval /*beta*/) const {
    if (b->terminal()) {
      if (!b->result) return eval::STALEMATE_RELATIVE_IN_N(ply);
      if (b->result == white)
        return eval::W_WIN_IN_N(ply);
      else
        return eval::B_WIN_IN_N(ply);
    }
    eval total = b->material_value + b->square_value*use_piece_square_vals + (int)((b->hash()%11)-5);
    return total;
  }
  eval evaluate_relative(const board* b, int ply, eval /*alpha*/, eval /*beta*/) const {
    if (b->terminal()) {
      if (!b->result) return eval::STALEMATE_RELATIVE_IN_N(ply);
      if (b->result == b->to_move)
        return eval::W_WIN_IN_N(ply);
      else
        return eval::B_WIN_IN_N(ply);
    }
    eval total = b->material_value + b->square_value*use_piece_square_vals + (int)((b->hash()%11)-5);
    eval v(b->to_move==white ? total : -total);
    return v;
  }
  int global_search_extensions(
      const board* /*b*/, int /*ply*/, eval /*alpha*/, eval /*beta*/, node_type /*type*/, int /*is_quiescence*/) const
  {
    return 0;
  }
  int local_search_extensions(
      const board* b, int /*ply*/, eval /*alpha*/, eval /*beta*/, node_type /*type*/, int /*is_quiescence*/) const
  {
    if (check_extension)
      return (b->in_check(b->to_move) ? check_extension: 0);
    else
      return 0;
  }
  // e.g. not if in check
  int allow_stand_pat_quiescence(const board* b) const {
    return !b->in_check(b->to_move);
  }

  int showf(FILE* ff) const {
    int n = 0;
    n += fprintf(ff, "<< Check-extension = %i\n", check_extension);

    n += fprintf(ff, "   Piece values = ");
    for (int i=0; i<num_piece_vals; ++i)
      n += fprintf(ff, " %6.2f", (double)(weights+piece_vals)[i]);
    n += fprintf(ff, "\n");

    if (use_piece_square_vals)
    {
      n += fprintf(ff, "   Piece square values =");
      for (int i=0; i<NPIECES; ++i) {
        if (!(i&(diag|ortho))) continue;
        n += fprintf(ff, "\n   %s\n", piece_name[i]);
        for (int r=5; r>=0; --r) {
          n += fprintf(ff, "      ");
          for (int c=0; c<6; ++c) {
            n+=fprintf(ff, " %6.2f", (double)(weights+piece_square_vals)[((r+1)*8+(c+1))*NPIECES+i]);
          }
          n += fprintf(ff, "\n");
        }
      }
    }

    return n;
  }

  void save(FILE* ff) const {
    fprintf(ff, "%i|%i|%i", check_extension, use_piece_square_vals, num_weights());
    for (int i=0; i<num_weights(); ++i)
      fprintf(ff, "|%.20g", get_weight_double(i));
  }
  void load(FILE* ff) {
    int nw = 0;
    fscanf(ff, "%i|%i|%i", &check_extension, &use_piece_square_vals, &nw);
    for (int i=0; i<nw; ++i) {
      double d;
      fscanf(ff, "|%lf", &d);
      set_weight_double(i, d);
    }
  }
};
// TD-lambda tuned weights (~20000 self-play games (depth 200+200)) with _no_ piece-square values
const eval evaluator::init_piece_vals[NPIECES] = {
//  0, 100, 200, 350, 0, 500, 700, 1100
  0, 100, 100, 215, 0, 225, 400, 500
};
const eval evaluator::init_piece_square_vals[8*8][NPIECES];

struct game {
  typedef eval      evalt;
  typedef move      movet;
  typedef board     boardt;
  typedef color     colort;
  typedef evaluator evaluatort;

  static const colort first   = alapo::white;
  static const colort second  = alapo::black;
};


}

int main(int argc, char* argv[])
{
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);
  setlocale(LC_ALL,"");
  srand48(argc>1 ? atoi(argv[1]) : -13);

  if (0)
  {
    alapo::game::boardt b;
    alapo::evaluator e(50, 1);
    {
      FILE* fw = fopen("alapo-weights.txt", "r");
      if (fw) {
        e.load(fw);
        fclose(fw);
      }
      e.showf(stdout);
    }
    Book<alapo::game> book(&b, 0.01, e, 400, 800, 256*1024, 64*1024);
    //Book<alapo::game> book(&b, 0.0, e, 200, 0, 256*1024, 64*1024);
    book.root->showf(stdout);
    for (int i=0; i<1000; ++i) {
      alapo::game::boardt bi;
      book.board = bi;
      //if (!(i%100))
        printf(".");
      //printf("\n");
      if (i%2)
        book.CalcEPO();
      else
        book.CalcEPB();
      //book.root->showf(stdout);
    }
    printf("************************************************************\n");
    book.root->showf(stdout);
    {
      FILE* fo = fopen("dod", "w");
      book.drop_out_diagram(fo);
      fclose(fo);
    }
    if (1){
      FILE* fo = fopen("bk", "w");
      book.save_book(fo);
      fclose(fo);
    }
    if (1){
      FILE* fo = fopen("bk", "r");
      Book<alapo::game> book2(&b, 1.0, e, 200, 400, 64*1024, 64*1024);
      book2.load_book(fo);
      fclose(fo);
      printf("************************************************************\n");
      book2.root->showf(stdout);
    }
  }

  if (0)
  {
    alapo::game::boardt b;
    std::vector<Player<alapo::game>*> v;

    alapo::evaluator e_0(0), e_1(75);

    Player<alapo::game> player_4_2_0; player_4_2_0.SearchPlayer(b, e_0, 400, 200, 64*1024, 64*1024);
    player_4_2_0.engine.settings.set_id(1,200,200);
    v.push_back(&player_4_2_0);

    Player<alapo::game> player_4_2_1; player_4_2_1.SearchPlayer(b, e_1, 400, 200, 64*1024, 64*1024);
    player_4_2_1.engine.settings.set_id(1,200,200);
    v.push_back(&player_4_2_1);

    Player<alapo::game> player_4_3_0; player_4_3_0.SearchPlayer(b, e_0, 400, 300, 64*1024, 64*1024);
    player_4_3_0.engine.settings.set_id(1,200,200);
    v.push_back(&player_4_3_0);

    Player<alapo::game> player_4_3_1; player_4_3_1.SearchPlayer(b, e_1, 400, 300, 64*1024, 64*1024);
    player_4_3_1.engine.settings.set_id(1,200,200);
    v.push_back(&player_4_3_1);

    Tourney<alapo::game> tourney(b, v);
    for (int i=0; i<10; ++i) {
      tourney.Play();
      tourney.showf(stdout);
    }
  }

  if (1)
  {
    static const int NUM_BOTS = 5;
    double  points[NUM_BOTS];
    int     games[NUM_BOTS];
    alapo::evaluator ev[NUM_BOTS];
    for (int i=0; i<NUM_BOTS; ++i) {
      for (int j=0; j<alapo::evaluator::num_piece_vals; ++j)
        ev[i].set_weight_double(j, 100.0 + 900.0*drand48());
      for (int j=0; j<alapo::evaluator::num_piece_square_vals; ++j)
        ev[i].set_weight_double(alapo::evaluator::num_piece_vals+j, -50.0 + 100.0*drand48());
      ev[i].normalize_weights();
    }
    TD_Lambda<alapo::game>*  td[NUM_BOTS];
    for (int i=0; i<NUM_BOTS; ++i)
      td[i] = new TD_Lambda<alapo::game>(ev[i].num_weights(), 0.99, 200.0, 0.25/100.0, 1.0, 1);

    for (int gg=0; gg<5000; ++gg) {
      for (int i=0; i<NUM_BOTS; ++i)
        td[i]->alpha = 200.0 - gg/20.0;
      for (int i=0; i<NUM_BOTS; ++i) {
        for (int j=0; j<NUM_BOTS; ++j) {
          if (i==j) continue;
          alapo::game::boardt b;

          Player<alapo::game> w_player, b_player;
          w_player.SearchPlayer(b, ev[i], 200, 200, 64*1024, 64*1024);
          w_player.engine.settings.set_id(1,200,200);
          w_player.engine.settings.set_asp(1,100);
          b_player.SearchPlayer(b, ev[j], 200, 200, 64*1024, 64*1024);
          b_player.engine.settings.set_id(1,200,200);
          b_player.engine.settings.set_asp(1,100);

          GameRecord<alapo::game> gr(b, &w_player, &b_player);

          for (int k=0; !gr.terminal && k<256; ++k)
            gr.step();
          //gr.current_board.showf(stdout);

          if      (gr.current_board.result==alapo::white) { points[i] += 1.0; }
          else if (gr.current_board.result==alapo::black) { points[j] += 1.0; }
          else                                            { points[i] += 0.5; points[j] += 0.5; }
          ++games[i];
          ++games[j];

          for (int k=0; k<NUM_BOTS; ++k)
            printf("%g %i %.1f   ", points[k], games[k], games[k] ? (points[k]/games[k]*100.0) : 50.0);
          printf("  #results\n");

          if (1) {
            if (1) {
              td[i]->process_game(&gr, &ev[i], 0);
              td[j]->process_game(&gr, &ev[j], 1);
            } else {
              for (int l=0; l<NUM_BOTS; ++l) {
                td[l]->process_game(&gr, &ev[l], 0);
                td[l]->process_game(&gr, &ev[l], 1);
              }
            }
            for (int k=0; k<alapo::NPIECES; ++k) {
              for (int l=0; l<NUM_BOTS; ++l) {
                printf("%.1f ", (double)ev[l].weights[k]);
              }
              printf("   ");
            }
            printf("  #td \n");
          }
        }
      }
    }
    for (int i=0; i<NUM_BOTS; ++i) {
      char  buff[64]; 
      sprintf(buff, "alapo-weights-x%i.txt", i);
      FILE* fw = fopen(buff, "w");
      ev[i].save(fw);
      fclose(fw);
    }
  }

  if (0)
  {
      // Note that check-extension gives _almost_ the strength improvement of 200 more qdepth with
      // significantly less time spent.  (Thus with time-controls may end up stronger).
      // It seems to give a strong improvement over 100 more qdepth with generally a little less time
      // spent (so definite improvement in time-control case.)
      // equality seems to be  QD 400 to ~225+CE (with equal search depths 600)

    int results[alapo::NCOLORS] = {0,0,0};
    alapo::evaluator e_init(50, 1);
    alapo::evaluator e_tuned(50, 1);
    {
      FILE* fw = fopen("alapo-weights.txt", "r");
      if (fw) {
        e_tuned.load(fw);
        fclose(fw);
      }
    }
    e_tuned.showf(stdout);
    if (1) {
      FILE* fw = fopen("alapo-weights.txt", "r");
      if (fw) {
        e_init.load(fw);
        fclose(fw);
      }
    }
    e_init.use_piece_square_vals = 0;
    e_init.showf(stdout);
    TD_Lambda<alapo::game>  td_lambda(e_tuned.num_weights(), 0.75, 100.0, 0.25/100.0, 1.0, 1);
    int verbose = 0;
    for (int gg=0; gg<1000; ++gg)
    {
      alapo::game::boardt b;

      Player<alapo::game> w_player; w_player.SearchPlayer(b, ((gg%2)==0 ? e_tuned : e_init), 400, 600, 64*1024, 64*1024);
      w_player.engine.settings.set_id(1,200,200);
      w_player.engine.settings.set_asp(1,100);

      Player<alapo::game> b_player; b_player.SearchPlayer(b, ((gg%2)==0 ? e_init : e_tuned), 400, 600, 64*1024, 64*1024);
      b_player.engine.settings.set_id(1,200,200);
      b_player.engine.settings.set_asp(1,100);

      GameRecord<alapo::game> gr(b, &w_player, &b_player);

      for (int i=0; !gr.terminal && i<256; ++i) {
        int n;
        alapo::game::movet ml[64];
        if (verbose) {
          w_player.engine.tc.showf(stdout); b_player.engine.tc.showf(stdout); printf("\n");
          gr.current_board.showf(stdout);
          fstatus<alapo::game::movet,alapo::game::evalt>(stdout, "{{%q}}{{%q}}\n",
             e_tuned.compute_evaluation_absolute(&gr.current_board, 1, alapo::game::evalt::B_INF, alapo::game::evalt::W_INF),
             e_init.compute_evaluation_absolute(&gr.current_board, 1, alapo::game::evalt::B_INF, alapo::game::evalt::W_INF));
          fstatus<alapo::game::movet,alapo::game::evalt>(stdout, "Move history: %M []\n",
            gr.current_board.move_stack, gr.current_board.num_moves);
          n = gr.current_board.gen_moves(ml);
          fstatus<alapo::game::movet,alapo::game::evalt>(stdout, "Possible moves: %M []\n", ml, n);
          n = gr.current_board.gen_moves_quiescence(ml);
          fstatus<alapo::game::movet,alapo::game::evalt>(stdout, "Quiescence moves: %M []\n", ml, n);
        }
        gr.step();
        if (verbose) {
          gr.move_list.back().showf(stdout); printf("\n");
        }
      }
      gr.current_board.showf(stdout);

      if      (gr.current_board.result==alapo::white) ++results[(gg%2)==0 ? alapo::white : alapo::black];
      else if (gr.current_board.result==alapo::black) ++results[(gg%2)==0 ? alapo::black : alapo::white];
      else                                            ++results[alapo::none];

      printf("RESULTS  T: %i  U: %i  -: %i   %.2f - %.2f %%\n",
        results[alapo::white], results[alapo::black], results[alapo::none],
        100.0*(results[alapo::white]+results[alapo::none]*0.5)/(gg+1.0),
        100.0*(results[alapo::black]+results[alapo::none]*0.5)/(gg+1.0));

      if (0) {
        printf("TD:before: "); e_tuned.showf(stdout);
        td_lambda.process_game(&gr, &e_tuned, ((gg%2)==0 ? 0 : 1));
        printf("TD:after:  "); e_tuned.showf(stdout);
        //td_lambda.showf(stdout);
        for (int i=0; i<alapo::NPIECES; ++i)
          printf("%.2f %.4f  ", (double)e_tuned.weights[i], (double)td_lambda.alpha_tc[i]);
        printf(" #td \n");
      }
    }
    if (0) {
      FILE* fw = fopen("alapo-weights.txt", "w");
      e_tuned.save(fw);
      fclose(fw);
    }
  }

  return 0;
}

