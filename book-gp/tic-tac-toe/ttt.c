#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*
 * 0 1 2
 * 3 4 5
 * 6 7 8
 */

typedef int board;
#define    XX    1
#define    OO    2
enum { XX_WIN=10, OO_WIN=-10, DRAW=0, INVALID=-9999, UNKNOWN=9999 };
int show(FILE* f, board b, int* results) {
    int i, j;
    for (i=0; i<3; ++i) {
        fprintf(f, "    ");
        for (j=0; j<3; ++j) {
            int n = i*3 + j;
            if (b & (XX << (n*2)))
                fprintf(f, "X");
            else if (b & (OO << (n*2)))
                fprintf(f, "O");
            else
                fprintf(f, ".");
            fprintf(f, " ");
        }
        if (results) {
            fprintf(f, "    ");
            for (j=0; j<3; ++j) {
                int n = i*3 + j;
                switch(results[n]) {
                    case INVALID: fprintf(f, "  .  "); break;
                    case XX_WIN: fprintf(f, "  X  "); break;
                    case OO_WIN: fprintf(f, "  O  "); break;
                    case DRAW: fprintf(f, " 1/2 "); break;
                    case UNKNOWN: fprintf(f, "  ?  "); break;
                }
            }
        }
        fprintf(f, "\n");
    }
    return 0;
}
#define    XX1    ((XX<<(0*2))|(XX<<(1*2))|(XX<<(2*2)))
#define    XX2    ((XX<<(0*2))|(XX<<(4*2))|(XX<<(8*2)))
#define    XX3    ((XX<<(0*2))|(XX<<(3*2))|(XX<<(6*2)))
#define    XX4    ((XX<<(1*2))|(XX<<(4*2))|(XX<<(7*2)))
#define    XX5    ((XX<<(2*2))|(XX<<(4*2))|(XX<<(6*2)))
#define    XX6    ((XX<<(2*2))|(XX<<(5*2))|(XX<<(8*2)))
#define    XX7    ((XX<<(3*2))|(XX<<(4*2))|(XX<<(5*2)))
#define    XX8    ((XX<<(6*2))|(XX<<(7*2))|(XX<<(8*2)))
#define    OO1    ((OO<<(0*2))|(OO<<(1*2))|(OO<<(2*2)))
#define    OO2    ((OO<<(0*2))|(OO<<(4*2))|(OO<<(8*2)))
#define    OO3    ((OO<<(0*2))|(OO<<(3*2))|(OO<<(6*2)))
#define    OO4    ((OO<<(1*2))|(OO<<(4*2))|(OO<<(7*2)))
#define    OO5    ((OO<<(2*2))|(OO<<(4*2))|(OO<<(6*2)))
#define    OO6    ((OO<<(2*2))|(OO<<(5*2))|(OO<<(8*2)))
#define    OO7    ((OO<<(3*2))|(OO<<(4*2))|(OO<<(5*2)))
#define    OO8    ((OO<<(6*2))|(OO<<(7*2))|(OO<<(8*2)))
int result(board b) {
    if ((b&XX1)==XX1) return XX_WIN;
    if ((b&XX2)==XX2) return XX_WIN;
    if ((b&XX3)==XX3) return XX_WIN;
    if ((b&XX4)==XX4) return XX_WIN;
    if ((b&XX5)==XX5) return XX_WIN;
    if ((b&XX6)==XX6) return XX_WIN;
    if ((b&XX7)==XX7) return XX_WIN;
    if ((b&XX8)==XX8) return XX_WIN;
    if ((b&OO1)==OO1) return OO_WIN;
    if ((b&OO2)==OO2) return OO_WIN;
    if ((b&OO3)==OO3) return OO_WIN;
    if ((b&OO4)==OO4) return OO_WIN;
    if ((b&OO5)==OO5) return OO_WIN;
    if ((b&OO6)==OO6) return OO_WIN;
    if ((b&OO7)==OO7) return OO_WIN;
    if ((b&OO8)==OO8) return OO_WIN;
    return DRAW;
}
#define    BOARD_FULL(b_) (!((~(((b_)&0x15555)|(((b_)&0x2AAAA)>>1)))&0x15555))
#define MOVE(b_,m_,s_) ((b_)|((s_)<<(2*(m_))))
#define    CAN_MOVE(b_,m_) (!((b_)&(3<<(2*(m_)))))
int    nodes_searched;
int minimax(int to_move, int b) {
    //if (!(nodes_searched%11017)) printf("%i nodes\n", nodes_searched);
    int eval = result(b);
    ++nodes_searched;
    if (eval != DRAW) return eval;
    if (BOARD_FULL(b)) return DRAW;
    int m;
    int    best_m = -1;
    int    best_v;
    if (to_move == XX) {
        best_v = OO_WIN;
        for (m=0; m<9; ++m) {
            if (CAN_MOVE(b,m)) {
                if (best_m < 0) best_m = m;
                int res = minimax(OO, MOVE(b,m,XX));
                if (res > best_v) {
                    best_m = m;
                    best_v = res;
                }
            } 
        }
    } else {
        best_v = XX_WIN;
        for (m=0; m<9; ++m) {
            if (CAN_MOVE(b,m)) {
                if (best_m < 0) best_m = m;
                int res = minimax(XX, MOVE(b,m,OO));
                if (res < best_v) {
                    best_m = m;
                    best_v = res;
                }
            }
        }
    }
    return best_v;
}
int negamax(int to_move, int b) {
    int eval = (to_move==XX) ? result(b) : -result(b);
    ++nodes_searched;
    if (eval != DRAW) return eval;
    if (BOARD_FULL(b)) return DRAW;
    int m;
    int    best_m = -1;
    int    best_v;
    best_v = OO_WIN;
    for (m=0; m<9; ++m) {
        if (CAN_MOVE(b,m)) {
            if (best_m < 0) best_m = m;
            int res = -negamax(to_move^3, MOVE(b,m,to_move));
            if (res > best_v) {
                best_m = m;
                best_v = res;
            }
        }
    }
    return best_v;
}
int alphabeta(int alpha, int beta, int to_move, int b) {
    int eval = (to_move==XX) ? result(b) : -result(b);
    ++nodes_searched;
    if (eval != DRAW) return eval;
    if (BOARD_FULL(b)) return DRAW;
    int    best_m = -1;
    int m;
    for (m=0; m<9; ++m) {
        if (CAN_MOVE(b,m)) {
            if (best_m < 0) best_m = m;
            int res = -alphabeta(-beta, -alpha, to_move^3, MOVE(b,m,to_move));
            if (res > alpha) {
                best_m = m;
                alpha = res;
            }
            if (alpha >= beta) return alpha;
        }
    }
    return alpha;
}
static const int moves[9] = {4, 0, 2, 6, 8, 1, 3, 5, 7};
int alphabeta_mo(int alpha, int beta, int to_move, int b) {
    int eval = (to_move==XX) ? result(b) : -result(b);
    ++nodes_searched;
    if (eval != DRAW) return eval;
    if (BOARD_FULL(b)) return DRAW;
    int    best_m = -1;
    int m, mm;
    for (mm=0; mm<9; ++mm) {
        m = moves[mm];
        if (CAN_MOVE(b,m)) {
            if (best_m < 0) best_m = m;
            int res = -alphabeta_mo(-beta, -alpha, to_move^3, MOVE(b,m,to_move));
            if (res > alpha) {
                best_m = m;
                alpha = res;
            }
            if (alpha >= beta) return alpha;
        }
    }
    return alpha;
}

int test(int b, int m) {
    int r, i, results[9];
    printf("----------------------------------------\n");
    nodes_searched = 0;
    for (i=0; i<9; ++i) {
        if (CAN_MOVE(b,i))
            r = minimax(m^3, MOVE(b,i,m));
        else
            r = INVALID;
        results[i] = r;
    }
    show(stdout, b, results);
    printf("--------  nodes=%i\n", nodes_searched);
    nodes_searched = 0;
    r = minimax(m, b);
    printf("*** Result=%i  nodes=%i\n\n", r, nodes_searched);

    nodes_searched = 0;
    for (i=0; i<9; ++i) {
        if (CAN_MOVE(b,i))
            r = negamax(m^3, MOVE(b,i,m));
        else
            r = INVALID;
        results[i] = r;
    }
    show(stdout, b, results);
    printf("--------  nodes=%i\n", nodes_searched);
    nodes_searched = 0;
    r = negamax(m, b);
    printf("*** Result=%i  nodes=%i\n\n", r, nodes_searched);

    nodes_searched = 0;
    for (i=0; i<9; ++i) {
        if (CAN_MOVE(b,i))
            r = alphabeta(OO_WIN-1, XX_WIN+1, m^3, MOVE(b,i,m));
        else
            r = INVALID;
        results[i] = r;
    }
    show(stdout, b, results);
    printf("--------  nodes=%i\n", nodes_searched);
    nodes_searched = 0;
    r = alphabeta(OO_WIN-1, XX_WIN+1, m, b);
    printf("*** Result=%i  nodes=%i\n\n", r, nodes_searched);

    nodes_searched = 0;
    for (i=0; i<9; ++i) {
        if (CAN_MOVE(b,i))
            r = alphabeta_mo(OO_WIN-1, XX_WIN+1, m^3, MOVE(b,i,m));
        else
            r = INVALID;
        results[i] = r;
    }
    show(stdout, b, results);
    printf("Result=%i  nodes=%i\n", r, nodes_searched);
    nodes_searched = 0;
    r = alphabeta_mo(OO_WIN-1, XX_WIN+1, m, b);
    printf("*** Result=%i  nodes=%i\n\n", r, nodes_searched);

    return r;
}
int main(int argc, char* argv[]) {
    int b=0, i, j, r;

    nodes_searched = 0;
        for (i=0; i<100; ++i)
      r = negamax(XX, b);
    printf("*** Result=%i  nodes=%i\n\n", r, nodes_searched);

        return 0;

    b = 0;
    test(b, XX);

    for (i=0; i<9; ++i) {
        if ((i%3)<(i/3)) continue;
        test(XX<<(2*i), OO);
    }

    for (i=0; i<9; ++i) {
        if ((i%3)<(i/3)) continue;
        for (j=0; j<9; ++j) {
            if (i!=j) {
                test((XX<<(2*i))|(OO<<(2*j)), XX);
            }
        }
    }

    return 0;
}
