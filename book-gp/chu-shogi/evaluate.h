#ifndef INCLUDED_EVALUATE_H_
#define INCLUDED_EVALUATE_H_
/* $Id: $ */
#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

#include <stdio.h>
#include "board.h"

// TODO: check timing difference for float vs. int here 
//typedef signed int evalt;
typedef float evalt;

#define W_WIN (+100000000)
#define B_WIN (-100000000)
#define W_WIN_IN_N(ply_) (W_WIN-1*(ply_))
#define B_WIN_IN_N(ply_) (B_WIN+1*(ply_))
#define W_INF (2*W_WIN)
#define B_INF (2*B_WIN)
#define DRAW  (0)
#define PLIES_TO_WIN(v) (((v)>0) ? ((W_WIN-(v))/1) : (((v)-B_WIN)/1))  // only works if actually win-in-n...

typedef enum game_phase {
  opening, midgame, endgame, num_phases
} game_phase;
#define NUM_WEIGHTS ((num_piece*2) + (num_piece*2) + num_phases*1 + num_phases*3 + (4+4) + (1+4) + (num_phases*num_piece*2*12*12))
typedef struct evaluator {
  union {
    evalt weights[NUM_WEIGHTS];
    struct {
      evalt piece_vals[num_piece][2];
      evalt king_tropism_vals[num_piece][2];

      evalt protected_piece[num_phases];

      evalt mobility_square_count[num_phases];
      evalt mobility_square_control[num_phases];
      evalt mobility_promotion_control[num_phases];

      evalt lion_protected[4];
      evalt lion_free[4];

      evalt king_attacked;
      evalt king_safe[4];

      evalt piece_square_bonus[num_phases][num_piece][2][12*12];

      //evalt attacked_piece_malus;
      //evalt piece_mobility_factor[num_piece][2];
    };
  };
} evaluator;

int simple_evaluator(evaluator* e);

int compute_board_control(const board* b, color side, int* counts);

evalt evaluate(const evaluator* e, const board* b, int ply, evalt alpha, evalt beta);

evalt evaluate_relative(const evaluator* e, const board* b, int ply, evalt alpha, evalt beta);

int evaluate_moves_for_search(const evaluator* e, const board* b,
                              const movet* ml, evalt* vl, int nm);

int evaluate_moves_for_quiescence(const evaluator* e, const board* b,
                                  const movet* ml, evalt* vl, int nm);

int renormalize_evaluator(evaluator* e);

int show_evaluator(FILE* f, const evaluator* e);
int show_evaluator_pieces_only(FILE* f, const evaluator* e);

int save_evaluator(FILE* f, const evaluator* e);

int load_evaluator(FILE* f, evaluator* e);

/* **************************************** */

#ifdef  __cplusplus
};
#endif/*__cplusplus*/
#endif /* INCLUDED_EVALUATE_H_ */
