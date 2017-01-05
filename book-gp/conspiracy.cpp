#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TH  10

namespace ttt {
typedef unsigned char movet;
typedef unsigned int hasht;
enum colort {
  none, white, black
};
struct boardt {
  colort  cells[9];
  colort  to_move;
  int     is_terminal;
  colort  result;

  int     num_moves;
  char    move_stack[9];

  hasht hash() const {
    hasht h = 0;
    for (int i=0; i<9; ++i)
      h = 3*h + cells[i];
    return h;
  }

  boardt() {
    memset(this, '\x00', sizeof(*this));
    to_move = white;
  }

  int check_terminal() {
    colort res = none;

    if (cells[0]==cells[1] && cells[1]==cells[2]) res = cells[0];
    if (cells[3]==cells[4] && cells[4]==cells[5]) res = cells[3];
    if (cells[6]==cells[7] && cells[7]==cells[8]) res = cells[6];
    if (cells[0]==cells[3] && cells[3]==cells[6]) res = cells[0];
    if (cells[1]==cells[4] && cells[4]==cells[7]) res = cells[1];
    if (cells[2]==cells[5] && cells[5]==cells[8]) res = cells[2];
    if (cells[0]==cells[4] && cells[4]==cells[8]) res = cells[0];
    if (cells[2]==cells[4] && cells[4]==cells[6]) res = cells[2];

    if (res || num_moves==9) {
      is_terminal = 1;
      result = res;
    }
    return is_terminal;
  }

  int make_move(movet m) {
    cells[m] = to_move;
    to_move = (colort)(to_move ^ 3);
    move_stack[num_moves++] = m;
    check_terminal();
    return 0;
  }

  int unmake_move() {
    movet m = move_stack[--num_moves];
    to_move = (colort)(to_move ^ 3);
    cells[m] = none;
    is_terminal = 0;
    return 0;
  }

  int gen_moves(movet ml[]) const {
    int n = 0;
    for (int i=0; i<9; ++i)
      if (!cells[i])
        ml[n++] = i;
    return n;
  }

  int showf(FILE* ff) const {
    int n = 0;
    for (int i=0; i<9; ++i)
      n += fprintf(ff, "%c%s", cells[i]==white?'X':cells[i]==black?'0':'_', (i==2||i==5)?"|":"");
    if (is_terminal)
      n += fprintf(ff, "*%c", result==white?'x':result==black?'o':'-');
    else
      n += fprintf(ff, ":%c", to_move==white?'x':to_move==black?'o':'-');
   
    return n;
  }

  int evaluate() const {
    int v = 0;
    if (is_terminal) {
      if (result==white)
        v = 10;
      else if (result==black)
        v = -10;
      else
        v = 0;
    } else {
      if (cells[0] && cells[0]==cells[1]) v+=(cells[0]==white?+1:-1);
      if (cells[1] && cells[1]==cells[2]) v+=(cells[1]==white?+1:-1);
      if (cells[0] && cells[0]==cells[2]) v+=(cells[0]==white?+1:-1);

      if (cells[3] && cells[3]==cells[4]) v+=(cells[3]==white?+1:-1);
      if (cells[4] && cells[4]==cells[5]) v+=(cells[4]==white?+1:-1);
      if (cells[3] && cells[3]==cells[5]) v+=(cells[3]==white?+1:-1);

      if (cells[6] && cells[6]==cells[7]) v+=(cells[6]==white?+1:-1);
      if (cells[7] && cells[7]==cells[8]) v+=(cells[7]==white?+1:-1);
      if (cells[6] && cells[6]==cells[8]) v+=(cells[6]==white?+1:-1);

      if (cells[0] && cells[0]==cells[3]) v+=(cells[0]==white?+1:-1);
      if (cells[3] && cells[3]==cells[6]) v+=(cells[3]==white?+1:-1);
      if (cells[0] && cells[0]==cells[6]) v+=(cells[0]==white?+1:-1);

      if (cells[1] && cells[1]==cells[4]) v+=(cells[1]==white?+1:-1);
      if (cells[4] && cells[4]==cells[7]) v+=(cells[4]==white?+1:-1);
      if (cells[1] && cells[1]==cells[7]) v+=(cells[1]==white?+1:-1);

      if (cells[2] && cells[2]==cells[6]) v+=(cells[2]==white?+1:-1);
      if (cells[6] && cells[6]==cells[8]) v+=(cells[6]==white?+1:-1);
      if (cells[2] && cells[2]==cells[8]) v+=(cells[2]==white?+1:-1);

      if (cells[0] && cells[0]==cells[4]) v+=(cells[0]==white?+1:-1);
      if (cells[0] && cells[0]==cells[8]) v+=(cells[0]==white?+1:-1);
      if (cells[4] && cells[4]==cells[8]) v+=(cells[4]==white?+1:-1);

      if (cells[2] && cells[2]==cells[4]) v+=(cells[2]==white?+1:-1);
      if (cells[2] && cells[2]==cells[6]) v+=(cells[2]==white?+1:-1);
      if (cells[4] && cells[4]==cells[6]) v+=(cells[4]==white?+1:-1);

      v /= 3;

      if (cells[4]==white) v+=2;
      if (cells[4]==black) v-=2;
    }
    return v + 10;
  }
};

} // namespace ttt


template<typename T>
inline T min(T a, T b) { return a<b ? a : b; }
template<typename T>
inline T max(T a, T b) { return a>b ? a : b; }

#define MINVAL        0
#define MAXVAL        20
#define MAXNCHILDREN  10

static int max_depth = 0;

struct node {
  ttt::boardt  b;
  int depth;
  int is_terminal;
  int is_maxnode;
  int is_leaf;
  int value;
  float cn_up[MAXVAL+1];
  float cn_dn[MAXVAL+1];
  node* parent;
  unsigned int   num_children;
  node* children[MAXNCHILDREN];

  node(ttt::boardt b_, int is_max=1) {
    memset(this, '\x00', sizeof(*this));
    b = b_;
    is_maxnode = is_max;
    is_leaf = 1;
    value = max(MINVAL,min(MAXVAL,b.evaluate()));
    is_terminal = (value<=MINVAL || value>=MAXVAL);
    update_cn();
  }

  int threshold_min(float th) const {
    for (int k=MINVAL; k<MAXVAL; ++k)
      if (cn_dn[k]<th)
        return k;
    return MAXVAL;
  }
  int threshold_max(float th) const {
    for (int k=MAXVAL; k>=MINVAL; --k)
      if (cn_up[k]<th)
        return k;
    return MINVAL;
  }

  void expand() {
    is_leaf = 0;
    ttt::movet ml[10];
    num_children = b.gen_moves(ml);
    for (unsigned int i=0; i<num_children; ++i) {
      b.make_move(ml[i]);
      children[i] = new node(b, !is_maxnode);
      b.unmake_move();
      children[i]->parent = this;
      children[i]->depth = depth+1;
      if (depth+1 > max_depth) max_depth = depth+1;
    }
    update_cn();
  }

  void update_cn() {
    if (is_terminal) {
      for (int i=MINVAL; i<=MAXVAL; ++i) {
        cn_up[i] = i>value ? 1.0/0.0 : 0;
        cn_dn[i] = i<value ? 1.0/0.0 : 0;
        // These from the paper give the wrong results:
        //cn_up[i] = i!=value ? 1.0/0.0 : 0;
        //cn_dn[i] = i!=value ? 1.0/0.0 : 0;
      }
    } else if (is_leaf) {
      for (int i=MINVAL; i<=MAXVAL; ++i) {
        cn_up[i] = i>value ? 1 : 0;
        cn_dn[i] = i<value ? 1 : 0;
      }
    } else {
      if (is_maxnode) {
        value = MINVAL;
        for (unsigned int k=0; k<num_children; ++k)
          value = max(value, children[k]->value);
        for (int i=MINVAL; i<=MAXVAL; ++i) {
          if (i<=value) {
            cn_up[i] = 0;
          } else {
            float m = 1.0/0.0;
            for (unsigned int k=0; k<num_children; ++k)
              m = min(m, children[k]->cn_up[i]);
            cn_up[i] = m;
          }
          if (i>=value) {
            cn_dn[i] = 0;
          } else {
            float s = 0.0;
            for (unsigned int k=0; k<num_children; ++k)
              s = s + children[k]->cn_dn[i];
            cn_dn[i] = s;
          }
        }
      } else {
        value = MAXVAL;
        for (unsigned int k=0; k<num_children; ++k)
          value = min(value, children[k]->value);
        for (int i=MINVAL; i<=MAXVAL; ++i) {
          if (i>=value) {
            cn_dn[i] = 0;
          } else {
            float m = 1.0/0.0;
            for (unsigned int k=0; k<num_children; ++k)
              m = min(m, children[k]->cn_dn[i]);
            cn_dn[i] = m;
          }
          if (i<=value) {
            cn_up[i] = 0;
          } else {
            float s = 0.0;
            for (unsigned int k=0; k<num_children; ++k)
              s = s + children[k]->cn_up[i];
            cn_up[i] = s;
          }
        }
      }
    }
    if (parent)
      parent->update_cn();
  }

  int showf(FILE* ff, int ind=0) const {
    int n = 0;
    n += fprintf(ff, "%*s<%c%c%i:%i>", ind, "", is_terminal?'*':' ', is_maxnode?'x':'n', num_children, value);
    n += fprintf(ff, " %i:{%i(%i)%i} ", TH, threshold_min(TH), value, threshold_max(TH));
    n += b.showf(ff);
    n += fprintf(ff, " [ ");
    for (int i=MINVAL; i<=MAXVAL; ++i) {
      if (i<value)
        n += fprintf(ff, "%g ", cn_dn[i]);
      else if (i>value)
        n += fprintf(ff, "%g ", cn_up[i]);
      else
        n += fprintf(ff, "|%g| ", cn_up[i]);
    }
    n += fprintf(ff, "]");
    n += fprintf(ff, "\n");
    for (unsigned int i=0; i<num_children; ++i)
      children[i]->showf(ff, ind+4);
    return n;
  }

  void expand_widest(int thresh) {
    if (is_terminal) {
      // TODO: ?!
    } else if (is_leaf) {
      expand();
    } else {
      int maxw = 0;
      int maxch = 0;
      for (unsigned int k=0; k<num_children; ++k) {
        int w = children[k]->threshold_max(thresh) - children[k]->threshold_min(thresh);
        if (w > maxw) {
          maxw = w;
          maxch = k;
        }
      }
      children[maxch]->expand_widest(thresh);
    }
  }

  void decrease_root(int t_min) {
    if (is_terminal) {
      // TODO: ?!
    } else if (is_leaf) {
      expand();
    } else if (is_maxnode) {
      for (unsigned int k=0; k<num_children; ++k) {
        if (children[k]->value > t_min) {
          children[k]->decrease_root(t_min);
          break;
        }
      }
    } else {
      float min_cn = 1.0/0.0;
      int   min_child = 0;
      for (unsigned int k=0; k<num_children; ++k) {
        if (children[k]->cn_dn[t_min] < min_cn) {
          min_cn = children[k]->cn_dn[t_min];
          min_child = k;
        }
      }
      children[min_child]->decrease_root(t_min);
    }
  }

  void increase_root(int t_max) {
    if (is_terminal) {
      // TODO: ?!
    } else if (is_leaf) {
      expand();
    } else if (!is_maxnode) {
      for (unsigned int k=0; k<num_children; ++k) {
        if (children[k]->value < t_max) {
          children[k]->increase_root(t_max);
          break;
        }
      }
    } else {
      float min_cn = 1.0/0.0;
      int   min_child = 0;
      for (unsigned int k=0; k<num_children; ++k) {
        if (children[k]->cn_up[t_max] < min_cn) {
          min_cn = children[k]->cn_up[t_max];
          min_child = k;
        }
      }
      children[min_child]->increase_root(t_max);
    }
  }
};


int main(int argc, const char* argv[]) {
  srand48(argc>1 ? atoi(argv[1]) : -13);
  printf("%i\n", (int)sizeof(ttt::boardt));

  ttt::boardt b;
  node* root = new node(b);
  root->showf(stdout); printf("\n");

  if (0)
  {
    int tmin = MINVAL, tmax = MAXVAL;
    int n = 0;
    while (tmin < tmax) {
      root->expand_widest(TH);
      tmin = root->threshold_min(TH);
      tmax = root->threshold_max(TH);
      printf("<%5i | %3i>  ", ++n, max_depth);
      root->showf(stdout);
      printf("\n");
    }
  }

  if (1)
  {
    int tmin = MINVAL, tmax = MAXVAL;
    int n = 0;
    while (tmin < tmax) {
      if ((root->value - tmin) < (tmax - root->value)) {
        printf("<<Increase Root (%i | %i)>>\n", ++n, max_depth);
        root->increase_root(tmax);
      } else {
        printf("<<Decrease Root (%i | %i)>>\n", ++n, max_depth);
        root->decrease_root(tmin);
      }
      tmin = root->threshold_min(TH);
      tmax = root->threshold_max(TH);
      root->showf(stdout); printf("\n");
    }
  }

  return 0;
}


