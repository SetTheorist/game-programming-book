#ifndef INCLUDED_OPENING_H_
#define INCLUDED_OPENING_H_

#include "board.h"
#include "player.h"
#include "util.h"

typedef struct BookNode {
  //hasht   hash;
  //char*   fen;
  movet   move;
  int     ply;

  int     terminal;
  int     determined; // theoretically determined...
  evalt   hval;       // relative valuation

  evalt   pval;       // negamax propagated valuation
  double  epb;
  double  epo;

  int     times_played;
  int     times_won;
  int     times_drawn;
  int     times_lost;

  BookNode* parent;
  int       num_children;
  BookNode* children;
} BookNode;

void init_booknode(BookNode* bn) {
  memset(bn, '\x00', sizeof(*bn));
}

void free_booknode(BookNode* bn) {
  if (bn->num_children) {
    for (i=0; i<bn->num_children; ++i)
      free_booknode(&bn->children[i]);
    free(bn->children);
  }
}

void save_booknode(BookNode* bn, FILE* ff) const {
  int i;
  fprintf(ff, "%x|%i|%i|%i|%g|%g|%g|%g|%i|%i|%i|%i|%i\n",
    bn->move.i, bn->terminal, bn->determined, bn->ply, (double)bn->hval, (double)bn->pval, bn->epb, bn->epo,
    bn->times_played, bn->times_won, bn->times_drawn, bn->times_lost, bn->num_children);
  for (i=0; i<num_children; ++i)
    save_booknode(&children[i], ff);
}

void load_booknode(BookNode* bn, FILE* ff) {
  char  buff[256];
  fgets(buff, sizeof(buff), ff);
  double hval_d, pval_d;
  sscanf(buff, "%x|%i|%i|%i|%lf|%lf|%lf|%lf|%i|%i|%i|%i|%i\n",
    &bn->move.i, &bn->terminal, &bn->determined, &bn->ply, &hval_d, &pval_d, &bn->epb, &bn->epo,
    &bn->times_played, &bn->times_won, &bn->times_drawn, &bn->times_lost, &bn->num_children);
  bn->hval = hval_d;
  bn->pval = pval_d;
  if (bn->num_children) {
    bn->children = malloc(sizeof(BookNode)*bn->num_children);
    for (int i=0; i<bn->num_children; ++i) {
      bn->children[i].parent = bn;
      load_booknode(&bn->children[i], ff);
    }
  }
}

inline int booknode_is_leaf(const BookNode* bn) {
  return (bn->num_children == 0);
}
inline int booknode_is_terminal(const BookNode* bn) {
  return bn->terminal;
}
inline int booknode_is_determined(const BookNode* bn) {
  return bn->determined;
}

void evaluate_booknode(BookNode* bn, boardt* b, Player<Game>* searcher) {
  //fprintf(stderr, "===== evaluate() =====\n");
  //b->showf(stderr);
  Ply<Game>  search;
  searcher->Think(*b, search);  // (,ply); ply-handling for terminal positions...
  hval = search.evaluation; //search.side==Game::first ? -search.evaluation : search.evaluation;
  pval = hval;
  if (parent) ply = parent->ply+1;
  if (b->terminal()) {
    determined = 1;
    terminal = 1;
    epo = +1.0/0.0;
    epb = +1.0/0.0;
  }
}

void expand(BookNode* bn, boardt* b, Player<Game>* searcher) {
  movet ml[MAXNMOVES];
  bn->num_children = gen_moves(b, ml);
  bn->children = malloc(sizeof(BookNode)*bn->num_children);
  for (int i=0; i<num_children; ++i) {
    make_move(b, ml[i]);
    bn->children[i].parent = this;
    bn->children[i].move = ml[i];
    evaluate_booknode(&bn->children[i], b, searcher);
    unmake_move(b);
  }
}

void update_booknode(BookNode* bn, double omega) {
  if (!booknode_is_terminal(bn) && !booknode_is_leaf(bn)) {
    int     all_children_determined = 1;
    evalt   bestv = evalt::B_INF;
    for (int i=0; i<num_children; ++i) {
      double cipval = -children[i].pval;
      all_children_determined = all_children_determined && children[i].determined;
      if (cipval > bestv) {
        bestv = cipval;
      }
    }
    pval = bestv;
    determined = all_children_determined;

    double  min_epo = +1.0/0.0;
    double  min_epb = +1.0/0.0;
    if (!determined) {
      for (int i=0; i<num_children; ++i) {
        if (children[i].epo < min_epo)
          min_epo = children[i].epo;
        double cipval = -children[i].pval;
        if (children[i].epb+omega*(bestv - cipval) < min_epb)
          min_epb = children[i].epb+omega*(bestv - cipval);
      }
    }
    epb = 1.0 + min_epo;
    epo = 1.0 + min_epb;
  }
  if (parent) parent->update(omega);
}

int showf_booknode(BookNode* bn, FILE* ff, int ind=0, int star=0) const {
  if (ind<1) ind=1;
  evalt   bestv = evalt::B_INF;
  for (int i=0; i<num_children; ++i) {
    if (-children[i].pval > bestv) {
      bestv = -children[i].pval;
    }
  }

  int n = 0;
  n += fprintf(ff, "%*s", star?ind-1:ind, "");
  n += fstatus<movet,evalt>(ff, "%s%m {p:%q h:%q} epb:%g epo:%g%s (+%i=%i-%i/%i)\n", star?"*":"",
      move, pval, hval, epb, epo, (is_terminal()?" <terminal>":is_determined()?" <determined>":is_leaf()?" ~":""),
      times_won, times_drawn, times_lost, times_played);
  for (int i=0; i<num_children; ++i)
    n += children[i].showf(ff, ind+4, (-children[i].pval==bestv));
  return n;
}
};


template<typename Game>
struct Book {
  typedef typename Game::evalt evalt;
  typedef typename Game::movet movet;
  typedef typename Game::boardt boardt;
  typedef typename Game::evaluatort evaluatort;

  BookNode<Game>* root;
  boardt          board;
  double          omega;
  Player<Game>    searcher;

  Book(boardt* board_, double omega_, const evaluatort& e, int depth, int qdepth, int ttnum=64*1024, int qttnum=64*1024) {
    board = *board_;
    omega = omega_;
    root = new BookNode<Game>;
    searcher.SearchPlayer(board, e, depth, qdepth, ttnum, qttnum);
    searcher.engine.settings.set_id(1);
    searcher.engine.settings.set_asp(1);
    root->evaluate(&board, &searcher);
    root->update(omega);
  }
  ~Book() {
    delete root;
  }
private:
  Book(const Book<Game>&);
  const Book<Game>& operator = (const Book<Game>&);
public:

  void CalcEPB() {
    calc_epb(root);
  }
  void CalcEPO() {
    calc_epo(root);
  }

  void calc_epb(BookNode<Game>* n) {
    if (n->is_leaf()) {
      select(n);
    } else {
      evalt   best_value = n->pval; // best successor value (?)
      double  epb_max = +1.0/0.0;
      int     best_move = 0;
      int     num_best = 0;
      for (int i=0; i<n->num_children; ++i) {
        if (-n->children[i].pval == best_value) {
          if (1.0 + n->children[i].epo < epb_max) {
            num_best = 1;
            epb_max = 1 + n->children[i].epo;
            best_move = i;
          } else if (1.0 + n->children[i].epo == epb_max) {
            // choose randomly among those with identical priority
            ++num_best;
            if (!(lrand48()%num_best)) {
              epb_max = 1 + n->children[i].epo;
              best_move = i;
            }
          }
        }
      }

      board.make_move(n->children[best_move].move);
      calc_epo(&n->children[best_move]);
      board.unmake_move();
    }
  }

  void calc_epo(BookNode<Game>* n) {
    if (n->is_leaf()) {
      select(n);
    } else {
      evalt   best_value = n->pval; // best successor value (?)
      double  epo_max = +1.0/0.0;
      int     best_move = 0;
      int     num_best = 0;
      for (int i=0; i<n->num_children; ++i) {
        double cipval = -n->children[i].pval;
        if (1.0 + n->children[i].epb + omega*(best_value - cipval) < epo_max) {
          num_best = 1;
          epo_max = 1.0 + n->children[i].epb + omega*(best_value - cipval);
          best_move = i;
        } else if (1.0 + n->children[i].epb + omega*(best_value - cipval) == epo_max) {
          // choose randomly among those with identical priority
          ++num_best;
          if (!(lrand48()%num_best)) {
            epo_max = 1.0 + n->children[i].epb + omega*(best_value - cipval);
            best_move = i;
          }
        }
      }

      board.make_move(n->children[best_move].move);
      calc_epb(&n->children[best_move]);
      board.unmake_move();
    }
  }

  void select(BookNode<Game>* n) {
    n->expand(&board, &searcher);
    n->update(omega);
  }

  static int drop_out_diagram_rec(FILE* ff, const BookNode<Game>* node) {
    int n = 0;
    if (node->is_terminal() || node->is_leaf()) {
      n += fprintf(ff, "%i %g\n", node->ply, node->move.w?-(double)node->pval:(double)node->pval);
    } else {
      for (int i=0; i<node->num_children; ++i) {
        if (node->ply%2) {
          n += drop_out_diagram_rec(ff, &(node->children[i]));
        } else {
          if (-node->children[i].pval == node->pval)
            n += drop_out_diagram_rec(ff, &(node->children[i]));
        }
      }
    }
    return n;
  }
  int drop_out_diagram(FILE* ff) const {
    return drop_out_diagram_rec(ff, root);
  }

  int save_book(FILE* ff) const {
    root->save(ff);
    return 0;
  }
  int load_book(FILE* ff) {
    BookNode<Game>* new_root = new BookNode<Game>;
    new_root->load(ff);
    delete root;
    root = new_root;
    return 0;
  }
};

#endif/*INCLUDED_OPENING_H_*/
