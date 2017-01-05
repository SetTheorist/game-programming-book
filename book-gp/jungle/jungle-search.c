/* $Id: jungle-search.c,v 1.7 2010/01/16 17:03:36 apollo Exp apollo $ */

/*
 * TODO:
 *  - stalemate handling
 *  - all pieces eaten handling (special case of above)
 *  - incremental hash computation
 *  - incremental evaluation
 *  - refactor for different players, evaluations, etc.
 *  - better interactive (undo, set depth, "En" or "Me" style moves: E^><v Enews Eurld Eklhj ?)
 *  * TDLeaf(lambda) - get piece values (piece-square values)
 *    (use new players, simple-to-complex players...)
 *
 * NEW PLAYERS:
 *  - random player
 *  - simple MC move ranker
 *  - UCB1 player
 *  - UCT player
 *  - epsilon-greedy
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "jungle-board.h"
#include "jungle-search.h"

/* The Jungle Game (Dou Shou Qi)
 * 
 * board is:
 *
 *  ..*#*..
 *  ...*...
 *  .......
 *  .==.==.
 *  .==.==.
 *  .==.==.
 *  .......
 *  ...*...
 *  ..*#*..
 *
 * Pieces (in descending order of strength):
 *  Elephant, Tiger, Lion, Panther, Wolf, Dog, Cat, Rat
 *
 * with starting positions:
 *  l.*#*.t
 *  .d.*.c.
 *  r.p.w.e
 *  .==.==.
 *  .==.==.
 *  .==.==.
 *  E.W.P.R
 *  .C.*.D.
 *  T.*#*.L
 * 
 * Basic rules:
 *  - pieces move one step orthogonally
 *  - pieces capture any animal of equal or lesser
 *    strength and rat can capture elephant
 *  - only rat can enter water, cannot capture crossing in or out of water
 *  - lions and tigers jump over water (not over any rat) and can capture so
 *  - any piece on enemy's trap can be captured by any animal
 *
 * variant rules:
 *
 */

/* ************************************************************ */

int show_game(FILE* f, board* b)
{
  int  i;
  for (i=0; i<b->ply; ++i) {
    if (i%2==0)
      fprintf(f, " %i.", 1+i/2);
    show_move(f, b->hist[i]);
  }
  if (b->side==white)
    fprintf(f, " Red to move.\n");
  else if (b->side==black)
    fprintf(f, " Blue to move.\n");
  else if ((b->side==none) && (b->xside==white))
    fprintf(f, " ** Red wins. **\n");
  else if ((b->side==none) && (b->xside==black))
    fprintf(f, " ** Blue wins. **\n");
  else if ((b->side==none) && (b->xside==none))
    fprintf(f, " ** Draw. **\n");
  return 0;
}

/* sort moves by score value (in stupid fashion) */
void sort_moves(int n, move* ml, evalt* scorel)
{
  int  i, j;
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
  evalt  best_score = scorel[i];
  int  besti = i;
  int  j;
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

evalt search_minmax(board* b, int depth, int ply)
{
  if (depth<=0||b->side==none)
    return evaluate(b, ply);

  move  ml[36];
  int    n, i;
  evalt    best_eval = b->side==white ? -1000001 : 1000001;
  n = gen_moves(b, ml);
  if (n==0) {printf("<ZERO MOVES IN MINMAX!>\n");fflush(stdout);return b->side==white?-1000001:1000001;}

  for (i=0; i<n; ++i) {
    //board  bx = *b;
    hasht preh, posth;
    if (VALIDATE_HASH) preh = hash_board(b);
    ml[i] = make_move(b, ml[i]);
    evalt val = search_minmax(b, depth-100, ply+1);
    unmake(b);
    if (VALIDATE_HASH) posth = hash_board(b);
    if (VALIDATE_HASH) if (preh!=posth) { 
      //printf("=====----------------==----------------=====\n");
      printf("!!!!!%016llX!=%016llX!!!!! <<", preh, posth); show_move(stdout, ml[i]); printf(">>\n");
      //printf(" ---------> PRE\n");
      //show_board(stdout, &bx);
      //printf(" ---------> POSt\n");
      //show_board(stdout, b);
      //printf("********************************************\n");
    }
    if (b->side==white) {
      if (val>best_eval)
        best_eval = val;
    } else {
      if (val<best_eval)
        best_eval = val;
    }
  }

  return best_eval;
}
evalt search_negamax(board* b, int depth, int ply)
{
  /* NB that all non-white & non-black sides are draws==0 */
  if (b->side==none)
    return b->xside==black ? evaluate(b, ply) : -evaluate(b, ply);
  if (depth<=0)
    return b->side==white ? evaluate(b, ply) : -evaluate(b, ply);

  move  ml[36];
  int    n, i;
  evalt    best_eval = -1000001;
  n = gen_moves(b, ml);
  if (n==0) {printf("<ZERO MOVES IN NEGAMAX!>\n");fflush(stdout);return -1000001;}

  for (i=0; i<n; ++i) {
    hasht preh, posth;
    if (VALIDATE_HASH) preh = hash_board(b);
    ml[i] = make_move(b, ml[i]);
    evalt val = -search_negamax(b, depth-100, ply+1);
    unmake(b);
    if (VALIDATE_HASH) posth = hash_board(b);
    if (VALIDATE_HASH) if (preh!=posth) {printf("!!!!!%016llX!=%016llX!!!!! <<", preh, posth); show_move(stdout, ml[i]); printf(">>\n");}
    if (val>best_eval)
      best_eval = val;
  }

  return best_eval;
}
evalt search_alphabeta(board* b, int depth, evalt alpha, evalt beta, int ply)
{
  /* NB that all (non-white & non-black) sides are draws==0 */
  if (b->side==none)
    return b->xside==black ? evaluate(b, ply) : -evaluate(b, ply);
  if (depth<=0)
    return b->side==white ? evaluate(b, ply) : -evaluate(b, ply);

  move  ml[36];
  int    n, i;
  n = gen_moves(b, ml);
  if (n==0) {printf("<ZERO MOVES IN ALPHABETA!>\n");fflush(stdout);return -1000001;}

  for (i=0; i<n; ++i) {
    hasht preh, posth;
    if (VALIDATE_HASH) preh = hash_board(b);
    ml[i] = make_move(b, ml[i]);
    evalt val = -search_alphabeta(b, depth-100, -beta, -alpha, ply+1);
    unmake(b);
    if (VALIDATE_HASH) posth = hash_board(b);
    if (VALIDATE_HASH) if (preh!=posth) {printf("!!!!!%016llX!=%016llX!!!!! <<", preh, posth); show_move(stdout, ml[i]); printf(">>\n");}
    if (val>alpha) {
      alpha = val;
      /* cutoff */
      if (alpha>=beta) break;
      memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*(depth-100)/100);
      pv[ply][ply] = ml[i];
    }
  }

  return alpha;
}
evalt search_quiesce(board* b, int qdepth, evalt alpha, evalt beta, int ply)
{
  /* NB that all (non-white & non-black) sides are draws==0 */
  if (b->side==none) {
    pv_length[ply] = 0;
    return b->xside==black ? evaluate(b, ply) : -evaluate(b, ply);
  }
  if (qdepth<=0) {
    pv_length[ply] = 0;
    return b->side==white ? evaluate(b, ply) : -evaluate(b, ply);
  }

  move  ml[36];
  evalt  scorel[36];
  int    n, i;
  evalt  val;

  n = gen_cap_moves(b, ml);
  /* if no caps, then just return evaluation --- this will happen automatically below */

  if (n>36) {printf("<TOO MANY MOVES IN QUIESCE!  INTERNAL INCONSISTENCY!>\n");fflush(stdout);return -1000001;}
  /* initialize scores by capture value */
  for (i=0; i<n; ++i) scorel[i] = ((ml[i].pieces>>4)&0x0F)*100 + i;

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

  move  ml[36];
  evalt  scorel[36];
  int    n, i;
  move  tt_goodm = {0,0,0,0};
  int    have_tt_goodm = 0;

  if (settings.f_tt) {
    hash_board(b);
    ttent*  tt = get_ttable(b->hash);
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
  if (n>36) {printf("<TOO MANY MOVES IN ABTTX_ANMP_Q!  INTERNAL INCONSISTENCY!>\n");fflush(stdout);return -1000001;}

  if (settings.f_hh) {
    /* initialize by history heuristic */
    for (i=0; i<n; ++i)
      scorel[i] = hist_heur[ml[i].from][ml[i].to]
        + ((ml[i].flags&cap) ? (10+((ml[i].pieces>>4)&0x0F)*10+i) : 0);
  } else {
    /* initialize scores by capture value */
    for (i=0; i<n; ++i)
      scorel[i] = (ml[i].flags&cap) ? (10000+((ml[i].pieces>>4)&0x0F)*100+i) : 0;
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
    const move  nm = null_move();
    if (memcmp(&(b->hist[b->ply-1]), &nm, sizeof(nm))) if (depth>(settings.nmp_R2+100))
    {
      int    nmr = depth>settings.nmp_cutoff ? settings.nmp_R1 : settings.nmp_R2;
      make_move(b,nm);
      evalt nmv = -search_general(b, depth-100-nmr, -beta, -alpha, ply+1);
      //evalt nmv = -search_general(b, depth-100-nmr, -beta, -beta+1, ply+1);
      unmake(b);
      if (nmv>=beta) {
        /* cutoff on fail-high*/
        return beta;
      }

      /* bump up alpha */
      if (nmv>alpha)
        alpha = nmv;
    }
  }

  for (i=0; i<n; ++i) {
    sort_it(i, n, ml, scorel);
    ml[i] = make_move(b, ml[i]);
    evalt val = -search_general(b, depth-100, -beta, -alpha, ply+1);
    unmake(b);
    if (val>alpha) {
      alpha = val;
      /* cutoff */
      if (alpha>=beta) {
        if (settings.f_hh)
          hist_heur[ml[i].from][ml[i].to] += 1<<((depth/100)-1);
        break;
      }
      memcpy( &pv[ply][ply+1], &pv[ply+1][ply+1], sizeof(move)*pv_length[ply+1]);
      pv[ply][ply] = ml[i];
      pv_length[ply] = pv_length[ply+1]+1;
    }
  }
  if (settings.f_tt) {
    if (alpha<beta) {
      put_ttable(b->hash, alpha, pv[ply][ply], tt_exact, depth);
      if (settings.f_hh)
        hist_heur[pv[ply][ply].from][pv[ply][ply].to] += 1<<((depth/100)-1);
    }
  }

  return alpha;
}

move search1(board* b, int depth, evalt* eval)
{
  move  ml[36];
  move  bestm={0,0,0,0};
  evalt  bestv = b->side==white ? -1000001 : 1000001;
  int    i, n;

  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH1!>\n");

  for (i=0; i<n; ++i) {
    ml[i] = make_move(b, ml[i]);
    evalt v = search_minmax(b, depth, 1);
    unmake(b);
    if (SHOW_MOVES) { printf("{"); show_move(stdout, ml[i]); printf(" :%i}", (int)v); if (i%6==5) printf("\n"); }

    if (b->side==white) {
      if (v>bestv) {
        bestm = ml[i];
        bestv = v;
      }
    } else {
      if (v<bestv) {
        bestm = ml[i];
        bestv = v;
      }
    }
  }
  if (SHOW_MOVES) printf("\n");
  
  *eval = bestv;
  return bestm;
}
move search2(board* b, int depth, evalt* eval)
{
  move  ml[36];
  move  bestm={0,0,0,0};
  evalt  bestv = -1000001;
  int    i, n;

  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH2!>\n");

  for (i=0; i<n; ++i) {
    ml[i] = make_move(b, ml[i]);
    evalt v = -search_negamax(b, depth, 1);
    unmake(b);
    if (SHOW_MOVES) { printf("{"); show_move(stdout, ml[i]); printf(" :%i}", (int)v); if (i%6==5) printf("\n"); }

    if (v>bestv) {
      bestm = ml[i];
      bestv = v;
    }
  }
  if (SHOW_MOVES) printf("\n");
  
  *eval = bestv;
  return bestm;
}
move search3(board* b, int depth, evalt* eval)
{
  move  ml[36];
  move  bestm={0,0,0,0};
  evalt  bestv = -1000001;
  int    i, n;

  n = gen_moves(b, ml);
  if (n==0) printf("<NO MOVES IN SEARCH3!>\n");

  for (i=0; i<n; ++i) {
    ml[i] = make_move(b, ml[i]);
    evalt v = -search_alphabeta(b, depth, -1000001, -bestv, 1);
    unmake(b);
    if (SHOW_MOVES) { printf("{"); show_move(stdout, ml[i]); printf(" :%i}", (int)v); if (i%6==5) printf("\n"); }

    if (v>bestv) {
      bestm = ml[i];
      bestv = v;
      memcpy( &pv[0][0+1], &pv[0+1][0+1], sizeof(move)*(depth-100)/100);
      pv[0][0] = ml[i];
    }
  }
  if (SHOW_MOVES) printf("\n");

  *eval = bestv;
  return bestm;
}
move search5(board* b, evalt* eval) {
  move  ml[36];
  evalt  scorel[36];
  move  bestm={0,0,0,0};
  evalt  lower_bd = -1000001;
  evalt  upper_bd = +1000001;
  evalt  asp_upper;
  evalt  asp_lower;
  int    doing_asp;
  int    i, n, d;
  int    pre, post;

  n = gen_moves(b, ml);
  if (n==0) {
    /* stalemate is loss */
    printf("<NO MOVES IN SEARCH5!>\n");
    b->side=none;
    *eval = -1000001;
    return bestm;
  }

  if (settings.f_hh)
    memset(hist_heur, '\x00', sizeof(hist_heur));

  /* initialize scores by capture value
   * (captured piece value doesn't seem to matter, but ...)
   */
  for (i=0; i<n; ++i)
    scorel[i] = (ml[i].flags&cap) ? (10000+((ml[i].pieces>>4)&0x0F)*100+i) : 0;

  for (d=settings.f_id?settings.id_base:settings.depth; d<=settings.depth; d+=settings.f_id?settings.id_step:0) {
    if (settings.f_asp &&(d>(settings.f_id?settings.id_base:settings.depth))) {
      evalt  mid = lower_bd;
      lower_bd = asp_lower = mid - settings.asp_width;
      upper_bd = asp_upper = mid + settings.asp_width;
      doing_asp = 1;
    } else {
      lower_bd = -1000001;
      upper_bd = +1000001;
      doing_asp = 0;
    }
    pre = nodes_evaluated;
    for (i=0; i<n; ++i) {
      sort_it(i, n, ml, scorel);
asp_restart:
      ml[i] = make_move(b, ml[i]);
      evalt v = -search_general(b, d, -upper_bd, -lower_bd, 1);
      unmake(b);

      if (doing_asp) {
        if (v >= asp_upper) {
          /* oops, upper window too low, restart */
          doing_asp = 0;
          upper_bd = +1000001;
          goto asp_restart;
        } else if (v <= asp_lower) {
          /* oops, lower window too high, restart */
          doing_asp = 0;
          lower_bd = -1000001;
          goto asp_restart;
        }
      }

      if (v>lower_bd) {
        bestm = ml[i];
        lower_bd = v;
        memcpy( &pv[0][1], &pv[1][1], sizeof(move)*pv_length[1]);
        pv[0][0] = ml[i];
        pv_length[0] = pv_length[1]+1;
      }

      /* save value for sorting next iteration */
      scorel[i] = v;

      if (SHOW_MOVES) {
        printf("{"); show_move(stdout, ml[i]); printf(" :%i}", (int)v);
        if (SHOW_MOVES_PV) {
          int  j;
          for (j=0; j<pv_length[0]; ++j) show_move(stdout, pv[0][j]);
          printf("\n");
        } else if (i%6==5) printf("\n");
      }
    }
    if (SHOW_MOVES) { if (SHOW_MOVES_PV||(n%6)) printf("\n"); }
    post = nodes_evaluated;
    if (!SILENT) {
      printf("{{Nodes for iter %iq%i = %i}} [", d, settings.qdepth, post-pre);
            for (i=0; i<pv_length[0]; ++i) show_move(stdout, pv[0][i]);
      printf(" ]\n");
      fflush(stdout);
      printf("\n");
    }
  }
  *eval = lower_bd;
  return bestm;
}
