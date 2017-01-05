#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ************************************************************ */

static int min(int a, int b) { return a<b ? a : b; }


/* ************************************************************ */

/* nxn TIC-TAC-TOE */

#define N  3

typedef enum Side {
  None=0, White=1, Black=2
} Side;

typedef struct Move {
  unsigned int w : 1;
  unsigned int q : 6;
} Move;

typedef struct Board {
  Side  to_move;
  int   is_terminal;
  int   result;

  Side  cells[N*N];

  int   num_moves;
  Move  moves[64];
} Board;

int init_board(Board* b) {
  memset(b, 0, sizeof(Board));
  b->to_move = White;
  return 0;
}
int show_board(Board* b) {
  int i, j;
  for (i=0; i<N; ++i)
  {
    for (j=0; j<N; ++j)
    {
      printf("%c", ".XO"[b->cells[i*N+j]]);
    }
    printf("\n");
  }
  printf("Moves: %i\n", b->num_moves);
  printf("%c to move\n", "?XO"[b->to_move]);
  if (b->is_terminal)
    printf("Result: %c\n", "-XO"[b->result]);
  printf("\n");
  return 0;
}
int gen_moves(Board* b, Move* ml) {
  int i, n=0;
  for (i=0; i<N*N; ++i)
  {
    if (b->cells[i]==None)
    {
      Move m = {b->to_move==White, i};
      ml[n++] = m;
    }
  }
  return n;
}
int winset(Board* b, int start, int delta) {
  int i;
  if (b->cells[start]==None)
    return None;
  for (i=1; i<N; ++i)
    if (b->cells[start+i*delta]!=b->cells[start])
      return None;
  return b->cells[start];
}
int check_terminal(Board* b) {
  int i;
  Side  winner = None;
  b->is_terminal = 0;

  if (winner==None)
    winner = winset(b, 0, N+1);
  if (winner==None)
    winner = winset(b, N-1, N-1);
  for (i=0; i<N; ++i)
    if (winner==None)
      winner = winset(b, i, N);
  for (i=0; i<N; ++i)
    if (winner==None)
      winner = winset(b, i*N, 1);

  if (b->num_moves==N*N || winner!=None)
  {
    b->is_terminal = 1;
    b->result = winner;
  }
  return b->is_terminal;
}
int make_move(Board* b, Move m) {
  b->cells[m.q] = b->to_move;
  b->to_move ^= 3;
  b->moves[b->num_moves++] = m;
  check_terminal(b);
  return 0;
}
int unmake_move(Board* b) {
  b->is_terminal = 0;
  b->to_move ^= 3;
  b->cells[b->moves[--b->num_moves].q] = None;
  return 0;
}


/* ************************************************************ */

enum {
  f_internal = 1,
  f_maxnode = 2,
  f_terminal = 4,
  f_win = 8,
};

typedef struct Node {
  Move  premove;
  unsigned char flags;
  int pn;
  int dp;
  int num_children;
  struct Node* parent;
  struct Node** children;
} Node;

#define INF 1000000

static void show_rec(const Node* n, int id)
{
  if (!(n->flags & f_internal)) return;
  int i;
  printf("%*s", id, "");
  printf((n->pn==INF ? " oo" : "%3i"), n->pn);
  printf("|");
  printf((n->dp==INF ? "oo " : "%3i"), n->dp);
  printf("  %c%c%c%c  <%p>\n",
    n->flags & f_internal ? 'I' : '-',
    n->flags & f_maxnode  ? 'M' : '-',
    n->flags & f_terminal ? 'T' : '-',
    n->flags & f_win ? 'W' : '-',
    n
    );
  for (i=0; i<n->num_children; ++i)
    show_rec(n->children[i], id+8);
}
static void show(const Node* n) { show_rec(n, 0); }

static Node* update(Board* b, Node* v)
{
  do {
    if (v->flags & f_maxnode) {
      int i;
      int pn = INF;
      int dp = 0;
      for (i=0; i<v->num_children; ++i)
      {
        pn = min(pn, v->children[i]->pn);
        dp += v->children[i]->dp;
      }
      dp = min(dp,INF);
      v->pn = pn;
      v->dp = dp;
    } else {
      int i;
      int pn = 0;
      int dp = INF;
      for (i=0; i<v->num_children; ++i)
      {
        pn += v->children[i]->pn;
        dp = min(dp, v->children[i]->dp);
      }
      pn = min(pn,INF);
      v->pn = pn;
      v->dp = dp;
    }
    if (v->parent)
      unmake_move(b);
    v = v->parent;
  } while (v);
  return v;
}

static Node* determine_most_proving_node(Board* b, Node* v) {
  while (v->flags & f_internal) {
    //printf("(%p)", v);
    if (v->flags & f_maxnode) {
      int i;
      for (i=0; i<v->num_children; ++i)
        if (v->children[i]->pn == v->pn)
        {
          v = v->children[i];
          make_move(b, v->premove);
          break;
        }
    } else {
      int i;
      for (i=0; i<v->num_children; ++i)
        if (v->children[i]->dp == v->dp)
        {
          v = v->children[i];
          make_move(b, v->premove);
          break;
        }
    }
  }
  return v;
}

Node* new_node(Node* parent);
Node* expand(Board* b, Node* v)
{
  int i;
  Move  ml[16];
  int num_moves = gen_moves(b, ml);
  v->num_children = num_moves;
  v->children = malloc(sizeof(Node*)*num_moves);
  for (i=0; i<num_moves; ++i)
  {
    v->children[i] = new_node(v);
    v->children[i]->premove = ml[i];
    make_move(b, ml[i]);
    v->children[i]->flags |= (b->to_move==White) ? f_maxnode : 0;
    if (b->is_terminal)
    {
      v->children[i]->flags |= f_terminal;
      //if (b->result == White)
      if (b->result != Black)
        v->children[i]->flags |= f_win;
    }
    unmake_move(b);
  }
  v->flags |= f_internal;
  
  for (i=0; i<v->num_children; ++i)
  {
    if (v->children[i]->flags & f_terminal) {
      if (v->children[i]->flags & f_win) {
        v->children[i]->pn = 0;
        v->children[i]->dp = INF;
      } else {
        v->children[i]->pn = INF;
        v->children[i]->dp = 0;
      }
    }
  }
  return v;
}

Node* new_node(Node* parent)
{
  Node* n = malloc(sizeof(Node));
  n->pn = 1;
  n->dp = 1;
  n->flags = 0;
  n->parent = parent;
  n->num_children = 0;
  n->children = 0;
  return n;
}

void free_node(Node* n)
{
  int i;
  for (i=0; i<n->num_children; ++i)
    free_node(n->children[i]);
  free(n->children);
  free(n);
}

int main(int argc, char* argv[])
{
  srand(argc>1 ? atoi(argv[1]) : 13);

  Board b;
  init_board(&b);
  show_board(&b);

  Node* n = new_node(0);
  n->flags |= f_maxnode;
  int count = 0;

  show(n);
  while (n->pn!=0 && n->dp!=0 && count<10000000)
  {
    ++count;
    Node* mp = determine_most_proving_node(&b, n);
    //printf("\nmost proving = %p\n", mp);
    //show_board(&b);
    expand(&b, mp);
    update(&b, mp);
    //show(n);
    if (count%1000==1)
      printf("%i | %i\n", n->pn, n->dp);
    //show_board(&b);
  }
  printf("Count = %i\n", count);
  if (n->pn==0) printf("Won\n");
  else if (n->dp==0) printf("Lost\n");
  else printf("**aborted**\n");
  free_node(n);
  printf("\n");

  return 0;
}
