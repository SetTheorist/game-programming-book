#ifndef    CONGO_BOARD_H
#define    CONGO_BOARD_H

/* $Id: cng-b.h,v 1.1 2010/12/15 21:48:32 ahogan Exp $ */

#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

#include <stdio.h>


#define    SILENT            0
#define    SHOW_MOVES        1
#define    VALIDATE_HASH    1
#define    RANDOM_EVAL        1

typedef unsigned char        uint8;
typedef unsigned short        uint16;
typedef unsigned int        uint32;
typedef unsigned long long    uint64;

unsigned int read_clock(void);

#define    NR    7
#define    NC    7

typedef enum colort {
    white, black, none
} colort;
typedef enum piecet {
    lion, elephant, giraffe, zebra, crocodile, monkey, pawn, superpawn,
    empty
} piecet;
typedef enum squaret {
    blank, river, wden, bden,
} squaret;

typedef    int    evalt;

extern int squares[NR*NC];
typedef    uint64    hasht;

extern int    bsteps[4], steps[4];
extern int box_to_board[], board_to_box[];
extern colort init_colors[NR*NC];
extern piecet init_pieces[NR*NC];

typedef enum flagst {
    w_win=1, b_win=2, w_draw=4, b_draw=8, cap=16, nullmove=32, monkeyjump=64, promote=128
} flagst;
#define    draw    (w_draw|b_draw)

typedef struct move {
    unsigned int    from    : 6;
    unsigned int    to      : 6;
    unsigned int    capped  : 4;
    unsigned int    moved   : 4;
    unsigned int    drowned : 4;
    unsigned int    flags   : 8;
} move;

typedef struct board {
    piecet  pieces[NR*NC];
    colort  colors[NR*NC];
    colort  side;
    colort  xside;
    int     monkey_moved;
    int     white_river;
    int     black_river;
    hasht   hash;
    int     ply;
    move    hist[1024];
    hasht   hash_hist[1024];
} board;

typedef enum ttent_flags {
    tt_valid=1, tt_exact=2, tt_upper=4, tt_lower=8
} ttent_flags;

typedef struct ttent {
    hasht   hash;
    evalt   score;
    int     flags;
    move    best_move;
    int     depth;
} ttent;

extern hasht    hash_values[NR*NC][8][2];
extern hasht    hash_to_move[3];
extern hasht    hash_monkey[NR*NC];

extern int tt_hit;
extern int tt_false;
extern int tt_deep;
extern int tt_shallow;
extern int tt_used;
extern int nodes_evaluated;

typedef struct search_settings {
    int    depth;    /* in %ge of ply */

    int    f_hh;
    int    f_tt;

    int    f_id;
    int    id_base;
    int    id_step;

    int f_iid;
    int    iid_base;
    int    iid_step;

    int f_mws;

    int f_asp;
    int asp_width;

    int    f_quiesce;
    int    qdepth;    /* in %ge of ply */

    /* use R1 reduction if depth>cutoff else R2 */
    int    f_nmp;
    int    nmp_R1;
    int    nmp_R2;
    int    nmp_cutoff;

    int evaluator;
} search_settings;
extern search_settings    settings;

extern move    pv[64][64];
extern int    pv_length[64];

#define    NTT    (1024*512)
extern ttent    ttable[NTT];

extern int hist_heur[NR*NC][NR*NC];


/* **************************************** */

typedef struct eval_params {
    evalt   piece_value[8];
    evalt   piece_square_bonus[8][NR*NC];
} eval_params;

typedef struct game_move {
    move    move_made;
    evalt   evaluation;
    int     n_pv;
    move    pv[50];
} game_move;

/* **************************************** */


evalt evaluate(board* b, int ply);
int show_settings(FILE* f, search_settings* s);
hasht gen_hash();
int init_hash();
int board_to_fen(board* b, char* chp);
int fen_to_board(board* b, char* chp);
hasht hash_board(board* b);
int init_ttable();
int put_ttable(hasht hash, evalt score, move m, int flags, int depth);
ttent* get_ttable(hasht hash);
int init_board(board* b);
int show_board(FILE* f, board* b);
int show_move(FILE* f, move m);
int gen_moves(board* b, move* ml);
int gen_moves_cap(board* b, move* ml);
int gen_moves_monkey(board* b, move* ml);
int check3rep(board* b);
move null_move();
move make_move(board* b, move m);
int unmake_move(board* b, move m);
int unmake(board* b);
move search5(board* b, evalt* eval);

int show_game(FILE* f, board* b);

extern eval_params eparams;
void setup_default_eval_params(eval_params* ep);

evalt evaluate1(board* b, int ply);
evalt evaluate2(board* b, int ply);
evalt evaluate_gen(board* b, int ply, eval_params* ep);
colort self_play_game(int *n_moves, game_move *moves);
int do_td_lambda_leaf(int n, game_move* moves, colort result, eval_params* epin, eval_params* epout);
evalt   eval_r(board* b, eval_params* ep, evalt beta);
void    d_eval_r_dw(board* b, eval_params* ep, eval_params* res, evalt beta);


#ifdef  __cplusplus
};
#endif/*__cplusplus*/

#endif/*CONGO_BOARD_H*/
