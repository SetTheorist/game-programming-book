/* $Id: cng-s.c,v 1.1 2010/12/15 03:23:26 apollo Exp $ */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "cng-b.h"
#include "cng-s.h"

#define MAXNMOVES 128

/* ************************************************************ */

/* sort moves by score value (in stupid fashion) */
void sort_moves(int n, move* ml, evalt* scorel)
{
    int    i, j;
    for (i=0; i<n; ++i) {
        for (j=i+1; j<n; ++j) {
            if (scorel[i] < scorel[j]) {
                evalt ts = scorel[i];
                move tm = ml[i];

                scorel[i] = scorel[j];
                ml[i] = ml[j];

                scorel[j] = ts;
                ml[j] = tm;
            }
        }
    }
}
/* just bring the best score from ml[i]..ml[n-1] to position i (swap it) */
inline void sort_it(int i, int n, move* ml, evalt* scorel)
{
    evalt    best_score = scorel[i];
    int    besti = i;
    int    j;
    for (j=i+1; j<n; ++j) {
        if (scorel[j]>best_score) {
            best_score = scorel[j];
            besti = j;
        }
    }
    if (besti!=i) {
        evalt ts = scorel[i];
        move tm = ml[i];

        scorel[i] = scorel[besti];
        ml[i] = ml[besti];

        scorel[besti] = ts;
        ml[besti] = tm;
    }
}

evalt search_quiesce(board* b, int qdepth, evalt alpha, evalt beta, int ply)
{
        pv_length[ply] = 0;
    /* NB that all (non-white & non-black) sides are draws==0 */
    if (b->side==none) {
        pv_length[ply] = 0;
        return b->xside==black ? evaluate(b, ply) : -evaluate(b, ply);
    }
    if (qdepth<=0) {
        pv_length[ply] = 0;
        return b->side==white ? evaluate(b, ply) : -evaluate(b, ply);
    }

    move    ml[MAXNMOVES];
    evalt    scorel[MAXNMOVES];
    int        n, i;
    evalt    val;

    n = gen_moves_cap(b, ml);
    /* if no caps, then just return evaluation --- this will happen automatically below */

    if (n>MAXNMOVES) {printf("<TOO MANY MOVES IN QUIESCE!  INTERNAL INCONSISTENCY!>\n");fflush(stdout);return -1000001;}
    /* initialize scores by capture value */
    for (i=0; i<n; ++i) scorel[i] = ml[i].capped*100 + i;

    /* first do "stand-pat"
     *
     */
    val = b->side==white ? evaluate(b, ply) : -evaluate(b,ply);
    if (val>alpha) {
        alpha=val;
        /* cutoff */
        if (alpha>=beta) return alpha;
    }

    for (i=0; i<n; ++i) {
        sort_it(i, n, ml, scorel);
        ml[i] = make_move(b, ml[i]);
        evalt val = -search_quiesce(b, qdepth-100, -beta, -alpha, ply+1);
        unmake(b);
        if (val>alpha) {
            alpha = val;
            /* cutoff */
            if (alpha>=beta) break;
            memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*pv_length[ply+1]);
            pv[ply][ply] = ml[i];
            pv_length[ply] = pv_length[ply+1]+1;
        }
    }
    return alpha;
}
evalt search_general(board* b, int depth, evalt alpha, evalt beta, int ply)
{
    pv_length[ply] = 0;

    /* NB that all (non-white & non-black) sides are draws==0 */
    if (b->side==none) {
        pv_length[ply] = 0;
        return b->xside==black ? evaluate(b, ply) : -evaluate(b, ply);
    }
    if (depth<=0) {
        pv_length[ply] = 0;
        if (settings.f_quiesce)
            return search_quiesce(b, settings.qdepth, alpha, beta, ply);
        else
            return b->side==white ? evaluate(b, ply) : -evaluate(b, ply);
    }

    move    ml[MAXNMOVES];
    evalt    scorel[MAXNMOVES];
    int        n, i;
    move    tt_goodm = {0,0,0,0};
    int        have_tt_goodm = 0;

    if (settings.f_tt) {
        //hasht   preh = b->hash;
        //hash_board(b);
        //hasht   posth = b->hash;
        //if (preh!=posth) fprintf(stderr,"$");
        ttent*    tt = get_ttable(b->hash);
        if (tt->flags & tt_valid) {
            ++tt_hit;
            if (tt->hash!=b->hash) {
                ++tt_false;
                goto skip_tt;
            }
            if (tt->depth<depth) {
                ++tt_shallow;
                have_tt_goodm = 1;
                tt_goodm = tt->best_move;
                goto skip_tt;
            }
            if (tt->depth>depth) {
                ++tt_deep;
                //goto skip_tt;
            }
            ++tt_used;
            pv[ply][ply] = tt->best_move;
            pv_length[ply] = 1;
            return tt->score;
        }
    }
skip_tt:

    n = gen_moves(b, ml);
    if (n==0) {printf("<ZERO MOVES IN ABTTX_ANMP_Q!>\n");fflush(stdout);return -1000001;}
    if (n>MAXNMOVES) {printf("<TOO MANY MOVES IN ABTTX_ANMP_Q!  INTERNAL INCONSISTENCY!>\n");fflush(stdout);return -1000001;}

    if (settings.f_hh) {
        /* use history_heuristic table for score initialization */
        for (i=0; i<n; ++i) scorel[i] = hist_heur[ml[i].from][ml[i].to];
        /* bonus capture value */
        for (i=0; i<n; ++i) if (ml[i].flags&cap) scorel[i] += 20;
    } else {
        /* initialize scores by capture value */
        for (i=0; i<n; ++i) scorel[i] = (ml[i].flags&cap) ? (10000+i) : 0;
    }

    if (have_tt_goodm) {
        /* bonus score to transition-table iteration good move so it's used first */
        for (i=0; i<n; ++i)
        {
            if ((tt_goodm.from==ml[i].from) && (tt_goodm.to==ml[i].to)) {
                scorel[i] += 1000000;
                break;
            }
        }
    }


    /* first do null-move-pruning:
     *  - swap sides
     *  - search with reduced depth
     *  - if result is <alpha, then prune
     */
    if (settings.f_nmp) {
        const move    nm = null_move();
        if (!(nullmove&(b->hist[b->ply-1].flags))) if (depth>(settings.nmp_R2+100))
        {
            int        nmr = depth>settings.nmp_cutoff ? settings.nmp_R1 : settings.nmp_R2;
            make_move(b,nm);
            evalt nmv = -search_general(b, depth-100-nmr, -beta, -alpha, ply+1);
            unmake(b);
            if (nmv>=beta) {
                /* cutoff on fail-high*/
                return beta;
            }
        }
    }


    if (settings.f_mws) {
        /* minimal-window search:
         *  after first move with full alpha-beta window, try minimal windows for following searches */
        for (i=0; i<n; ++i) {
            evalt   val;
            sort_it(i, n, ml, scorel);
            ml[i] = make_move(b, ml[i]);
            if (i==0) {
                val = -search_general(b, depth-100, -beta, -alpha, ply+1);
            } else {
                val = -search_general(b, depth-100, -(alpha+1), -alpha, ply+1);
                /* fail-high means we have to try again with full window */
                if (val>=alpha+1) {
                    val = -search_general(b, depth-100, -beta, -(alpha+1), ply+1);
                }
            }
            unmake(b);
            if (val>alpha) {
                alpha = val;
                /* cutoff */
                if (alpha>=beta) {
                    /* increment cutoff move */
                    if (settings.f_hh)
                        hist_heur[ml[i].from][ml[i].to] += 1<<((depth-100)/100);
                    break;
                }
                memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*pv_length[ply+1]);
                pv[ply][ply] = ml[i];
                pv_length[ply] = pv_length[ply+1]+1;
            }
        }
    } else {
        int dpth;
        /* straight-forward alpha-beta */
        for (dpth =  (settings.f_iid&&(depth>=500)) ? (depth-400) : depth;
             dpth <= depth;
             dpth += (settings.f_iid&&(depth>=500)) ? 200 : 100) {
        //fprintf(stderr, "<%i/%i>", dpth, depth);
        for (i=0; i<n; ++i) {
            sort_it(i, n, ml, scorel);
            ml[i] = make_move(b, ml[i]);
            evalt val = -search_general(b, dpth-100, -beta, -alpha, ply+1);
            unmake(b);
            if (settings.f_iid) scorel[i] = val;
            if (val>alpha) {
                alpha = val;
                /* cutoff */
                if (alpha>=beta) {
                    /* increment cutoff move */
                    if (settings.f_hh)
                        hist_heur[ml[i].from][ml[i].to] += 1<<((dpth-100)/100);
                    break;
                }
                memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*pv_length[ply+1]);
                pv[ply][ply] = ml[i];
                pv_length[ply] = pv_length[ply+1]+1;
            }
        }
        }
    }


    if (settings.f_tt) {
        if (alpha<beta) {
            put_ttable(b->hash, alpha, pv[ply][ply], tt_exact, depth);
            /* increment best move */
            if (settings.f_hh)
                hist_heur[pv[ply][ply].from][pv[ply][ply].to] += 1<<((depth-100)/100);
        }
    }

    return alpha;
}

move search5(board* b, evalt* eval)
{
    move    ml[36];
    move    old_ml[36];
    evalt   scorel[36];
    evalt   old_scorel[36];
    move    bestm={0,0,0,0};
    evalt   lower_bound=-1000001, upper_bound;
    int     i, n, d;
    int     pre, post;

    n = gen_moves(b, ml);
    if (n==0) {
        /* stalemate is loss */
        printf("<NO MOVES IN SEARCH5!>\n");
        b->side=none;
        *eval = -1000001;
        return bestm;
    }

    /* initialize scores by capture value
     * (captured piece value doesn't seem to matter, but ...)
     */
    for (i=0; i<n; ++i)
        scorel[i] = (ml[i].flags&cap) ? (10000+ml[i].capped*100+i) : 0;

    if (settings.f_hh)
        memset(hist_heur, 0, sizeof(hist_heur));

    for (d=settings.f_id?settings.id_base:settings.depth; d<=settings.depth; d+=settings.id_step) {
        int     doing_asp;
        evalt   asp_midv, asp_low, asp_high;
        if (settings.f_asp && settings.f_id && (d>settings.id_base)) {
            /* aspiration search: after first iteration we try a small window around previous iteration's result */
            doing_asp = 1;
            asp_midv = lower_bound;
            lower_bound = asp_low = asp_midv - settings.asp_width;
            upper_bound = asp_high = asp_midv + settings.asp_width;
            memcpy(old_scorel, scorel, sizeof(scorel));
            memcpy(old_ml, ml, sizeof(ml));
            if (!SILENT) printf("<Aspiration window: (%i,%i)>\n", (int)asp_low, (int)asp_high);
        } else {
            doing_asp = 0;
            lower_bound = -1000001;
            upper_bound = +1000001;
        }
        pre = nodes_evaluated;
        for (i=0; i<n; ++i) {
            sort_it(i, n, ml, scorel);
            if (SHOW_MOVES) {
                printf("["); show_move(stdout, ml[i]); printf("]");
            }
            ml[i] = make_move(b, ml[i]);
            evalt v = -search_general(b, d, -upper_bound, -lower_bound, 1);
            unmake(b);
            if (SHOW_MOVES) {
                printf("{"); show_move(stdout, ml[i]); printf(" :%i}", (int)v); if (i%6==5) printf("\n");
            }

            if (v>lower_bound) {
                bestm = ml[i];
                lower_bound = v;
                memcpy( &pv[0][1], &pv[1][1], sizeof(move)*pv_length[1]);
                pv[0][0] = ml[i];
                pv_length[0] = pv_length[1]+1;
            }
            /* save value for sorting next iteration */
            scorel[i] = v;

            if (doing_asp && ((lower_bound>=asp_high) || (lower_bound<=asp_low))) { /* TODO: ? */
                /* darn, fell outside aspiration window,
                 * restart with full window
                 *
                 * actually: we can use the results up to this failing move...
                 */
                lower_bound = -1000001;
                upper_bound = +1000001;
                memcpy(scorel, old_scorel, sizeof(scorel));
                memcpy(ml, old_ml, sizeof(ml));
                i = -1;
                doing_asp = 0;
                if (!SILENT) { if(i%6!=5)printf("\n"); printf("<FAILED ASPIRATION - RESTART ITERATION>\n"); }
            }
        }
        if (SHOW_MOVES) printf("\n");
        post = nodes_evaluated;
        if (!SILENT) printf("{{Nodes for iter %i(qd=%i) = %i}}\n", d, settings.qdepth, post-pre);
        if (!SILENT) fflush(stdout);
    }

    *eval = lower_bound;
    return bestm;
}

int show_game(FILE* f, board* b)
{
    int    i;
    for (i=0; i<b->ply; ++i) {
        if (i%2==0)
            fprintf(f, " %i.", 1+i/2);
        show_move(f, b->hist[i]);
    }
    if (b->side==white)
        fprintf(f, " White to move.\n");
    else if (b->side==black)
        fprintf(f, " Black to move.\n");
    else if ((b->side==none) && (b->xside==white))
        fprintf(f, " ** White wins. **\n");
    else if ((b->side==none) && (b->xside==black))
        fprintf(f, " ** Black wins. **\n");
    else if ((b->side==none) && (b->xside==none))
        fprintf(f, " ** Draw. **\n");
    return 0;
}
