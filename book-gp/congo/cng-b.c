/* $Id: cng-b.c,v 1.1 2010/12/15 03:23:26 apollo Exp $ */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "cng-b.h"

int esteps[4] = {+1,+11,-1,-11};
int gsteps[8] = {+1,+11,+10,+12,-1,-11,-10,-12};
int zsteps[8] = {+13,+21,+23,+9,-13,-21,-23,-9};

int squares[NR*NC] = {
    blank, blank,  wden,  wden,  wden, blank, blank,
    blank, blank,  wden,  wden,  wden, blank, blank,
    blank, blank,  wden,  wden,  wden, blank, blank,
    river, river, river, river, river, river, river,
    blank, blank,  bden,  bden,  bden, blank, blank,
    blank, blank,  bden,  bden,  bden, blank, blank,
    blank, blank,  bden,  bden,  bden, blank, blank,
};
colort init_colors[NR*NC] = {
    white, white, white, white, white, white, white,
    white, white, white, white, white, white, white,
     none,  none,  none,  none,  none,  none,  none,
     none,  none,  none,  none,  none,  none,  none,
     none,  none,  none,  none,  none,  none,  none,
    black, black, black, black, black, black, black,
    black, black, black, black, black, black, black,
};
piecet init_pieces[NR*NC] = {
    giraffe, monkey, elephant, lion, elephant, crocodile, zebra,
    pawn, pawn, pawn, pawn, pawn, pawn, pawn,
    empty, empty, empty, empty, empty, empty, empty,
    empty, empty, empty, empty, empty, empty, empty,
    empty, empty, empty, empty, empty, empty, empty,
    pawn, pawn, pawn, pawn, pawn, pawn, pawn,
    giraffe, monkey, elephant, lion, elephant, crocodile, zebra,
};
int board_to_box[NR*NC] = {
    24, 25, 26, 27, 28, 29, 30,
    35, 36, 37, 38, 39, 40, 41,
    46, 47, 48, 49, 50, 51, 52,
    57, 58, 59, 60, 61, 62, 63,
    68, 69, 70, 71, 72, 73, 74,
    79, 80, 81, 82, 83, 84, 85,
    90, 91, 92, 93, 94, 95, 96,
};
int box_to_board[(NR+4)*(NC+4)] = {
    -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
    -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
    -1,  -1,   0,   1,   2,   3,   4,   5,   6,  -1,  -1,
    -1,  -1,   7,   8,   9,  10,  11,  12,  13,  -1,  -1,
    -1,  -1,  14,  15,  16,  17,  18,  19,  20,  -1,  -1,
    -1,  -1,  21,  22,  23,  24,  25,  26,  27,  -1,  -1,
    -1,  -1,  28,  29,  30,  31,  32,  33,  34,  -1,  -1,
    -1,  -1,  35,  36,  37,  38,  39,  40,  41,  -1,  -1,
    -1,  -1,  42,  43,  44,  45,  46,  47,  48,  -1,  -1,
    -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
    -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
};

hasht    hash_values[NR*NC][8][2];
hasht    hash_to_move[3];
hasht    hash_monkey[NR*NC];

int tt_hit;
int tt_false;
int tt_deep;
int tt_shallow;
int tt_used;

int    nodes_evaluated;

search_settings    settings;

move    pv[64][64];
int     pv_length[64];

ttent   ttable[NTT];

int hist_heur[NR*NC][NR*NC];

/* ************************************************************ */
/* ************************************************************ */

unsigned int read_clock(void) {
    struct timeval timeval;
    struct timezone timezone;
    gettimeofday(&timeval, &timezone);
    return (timeval.tv_sec * 1000000 + (timeval.tv_usec));
}

#if 1
static const char piece_char[] = "LEGZCMPS.?!-&";
static const char square_char[] = ".~+x*#";
//static const char square_char[] = "      ";
static const char* normal = "\033[0m";
static const char* bg_squares[] = { "\033[47m", "\033[44m", "\033[43m", "\033[43m", "\033[42m", "\033[42m", };
static const char* fg_colors[] = { "\033[31;1m", "\033[30;1m", "\033[35m", };
#else
static const char piece_char[] = "LEGZCMPS.?!-&";
static const char square_char[] = ".=+x*#";
//static const char square_char[] = "      ";
static const char* normal = "";
static const char* bg_squares[] = { "", "", "", "", "", "", };
static const char* fg_colors[] = { "", "", "", };
#endif

/* ************************************************************ */

int show_settings(FILE* f, search_settings* s) {
    fprintf(f, "A-B(%i)", s->evaluator);
    if (s->f_asp) fprintf(f,"-ASP+%i", s->asp_width);
    if (s->f_mws) fprintf(f,"-MWS");
    if (s->f_tt) fprintf(f,"-TT");
    if (s->f_id) fprintf(f,"-ID%i+%i", s->id_base, s->id_step);
    if (s->f_hh) fprintf(f,"-HH");
    if (s->f_iid) fprintf(f,"-IID%i+%i", s->iid_base, s->iid_step);
    fprintf(f, "(%i", s->depth);
    if (s->f_quiesce) fprintf(f, "q%i", s->qdepth);
    if (s->f_nmp) fprintf(f, "[NMP:%i(%i)%i]", s->nmp_R1, s->nmp_cutoff, s->nmp_R2);
    fprintf(f, ")");
    return 0;
}

hasht gen_hash() {
    hasht    h = 0;
    int    i;
    for (i=0; i<32; ++i)
        h = h ^ ((hasht)(lrand48())<<(2*i)) ^ ((hasht)(lrand48())>>(2*i));
    return h;
}
int init_hash() {
    int    i, j, k;
    for (i=0; i<(NR*NC); ++i)
        for (j=0; j<8; ++j)
            for (k=0; k<2; ++k)
                hash_values[i][j][k] = gen_hash();
    for (i=0; i<3; ++i)
        hash_to_move[i] = gen_hash();
    for (i=0; i<(NR*NC); ++i)
        hash_monkey[i] = gen_hash();
    return 0;
}

hasht hash_board(board* b) {
    int    i;
    hasht    hash = hash_to_move[b->side];
    for (i=0; i<(NR*NC); ++i) {
        if (b->pieces[i]!=empty)
            hash = hash ^ hash_values[i][b->pieces[i]][b->colors[i]];
    }
    if (b->monkey_moved>=0)
        hash = hash ^ hash_monkey[b->monkey_moved];
    b->hash = hash;
    return hash;
}

int board_to_fen(board* b, char* chp)
{
    int    r, c;
    for (r=NR-1; r>=0; --r) {
        for (c=0; c<NC; ++c) {
            if (b->colors[r*NC+c]==none) {
                int    n = 0;
                while ((c<NC) && (b->colors[r*NC+c]==none)) {
                    ++n;
                    ++c;
                }
                --c;
                *chp++ = '0'+n;
            } else {
                if (b->colors[r*NC+c]==white)
                    *chp++ = piece_char[b->pieces[r*NC+c]];
                else
                    *chp++ = piece_char[b->pieces[r*NC+c]] + 'a' - 'A';
            }
        }
        if (r) *chp++ = '/';
    }
    *chp++ = ' ';
    *chp++ = (b->side==white ? 'W' : b->side==black ? 'B' : '-');
    *chp++ = ' ';
    if (b->monkey_moved>=0) {
        *chp++ = 'a'+(b->monkey_moved%NC);
        *chp++ = '1'+(b->monkey_moved/NC);
    } else {
        *chp++ = '-';
    }
    *chp = '\000';
    return 0;
}
int fen_to_board(board* b, char* chp) {
    int    r=NR-1, c=0, i;
    init_board(b);
    for(i=0; i<NC*NR; ++i) {
        b->pieces[i] = empty;
        b->colors[i] = none;
    }
    while (*chp) {
        if (isdigit((uint8)*chp)) {
            c+=*chp-'0';
            ++chp;
        } else if (isalpha((uint8)*chp)) {
            if (islower((uint8)*chp))
                b->colors[c+r*NC] = black;
            else
                b->colors[c+r*NC] = white;
            b->pieces[c+r*NC] = strchr(piece_char, toupper((uint8)*chp)) - piece_char;
            ++chp;
            ++c;
        }
        if (c==7) {
            ++chp;
            c=0;
            --r;
            if (r==-1) break;
        }
    }
    if (*chp=='W') {
        b->side=white;
        b->xside=black;
    } else if (*chp=='B') {
        b->side=black;
        b->xside=white;
    } else {
        b->side=none;
        b->xside=none;
    }
    ++chp; ++chp;
    if (*chp=='-') {
        ++chp;
        b->monkey_moved=-1;
    } else {
        c = *chp++ - 'a';
        r = *chp++ - '1';
        b->monkey_moved = c + r*NC;
    }
    /* fix-up pieces in river assumes only 1 per color,
     * as this is the only which can handle during legal play
     */
    for (i=21; i<28; ++i) {
        if ((b->pieces[i]!=empty) && (b->pieces[i]!=crocodile)) {
            if (b->colors[i]==white)
                b->white_river = i;
            else /* (b->colors[i]==black) */
                b->black_river = i;
        }
    }
    hash_board(b);
    return 0;
}

int init_ttable() {
    memset(ttable, 0, sizeof(ttable));
    return 0;
}

int put_ttable(hasht hash, evalt score, move m, int flags, int depth) {
    int    loc = (unsigned int)hash%NTT;
    if (loc<0) printf("!Error in add_ttable()!\n");
    /* most-recent: replaces without checking
     * deepest: replaces only if as deep or deeper
     * --- inital testing seems to say most-recent gives fewer nodes
     */
//    if (!(ttable[loc].flags&tt_valid) || (ttable[loc].depth>=depth))
    {
        ttable[loc].hash = hash;
        ttable[loc].score = score;
        ttable[loc].best_move = m;
        ttable[loc].flags = flags|tt_valid;
        ttable[loc].depth = depth;
    }
    return 0;
}
ttent* get_ttable(hasht hash) {
    return &ttable[(unsigned int)hash%NTT];
}

int init_board(board* b) {
    memset(b, 0, sizeof(*b));
    memcpy(b->pieces, init_pieces, sizeof(init_pieces));
    memcpy(b->colors, init_colors, sizeof(init_colors));
    b->side = white;
    b->xside = black;
    b->ply = 0;
    b->monkey_moved = -1;
    b->white_river = -1;
    b->black_river = -1;
    b->hash_hist[b->ply] = hash_board(b);
    return 0;
}

int show_board(FILE* f, board* b) {
    int    r, c;
    hash_board(b);
    fprintf(f, " abcdefg \n");
    fprintf(f, "+-------+   [%016llX] (%i/%i)\n", b->hash, (int)((b->hash)%(hasht)NTT), NTT);
    for (r=NR-1; r>=0; --r) {
        fprintf(f, "|");
        for (c=0; c<NC; ++c) {
            fprintf(f, "%s", bg_squares[squares[r*NC+c]]);
            fprintf(f, "%s", fg_colors[b->colors[r*NC+c]]);
            if (b->pieces[r*NC+c]!=empty) {
                fprintf(f, "%c", piece_char[b->pieces[r*NC+c]]);
            } else {
                fprintf(f, "%c", square_char[squares[r*NC+c]]);
            }
            fprintf(f, "%s", normal);
        }
        if (r==NR-2) {
            fprintf(f, "| %i [%i|%i,%i]\n", r+1, b->monkey_moved, b->white_river, b->black_river);
        } else if (r==NR-4) {
            fprintf(f, "| %i <%i|%i>\n", r+1, (int)evaluate1(b,0), (int)evaluate2(b,0));
        } else
            fprintf(f, "| %i\n", r+1);
    }
    fprintf(f, "+-------+  ");
    if (b->side==white)
        fprintf(f, " White to move\n");
    else if (b->side==black)
        fprintf(f, " Black to move\n");
    else if ((b->side==none) && (b->xside==white))
        fprintf(f, " ** White wins **\n");
    else if ((b->side==none) && (b->xside==black))
        fprintf(f, " ** Black wins **\n");
    else if ((b->side==none) && (b->xside==none))
        fprintf(f, " ** Draw **\n");
    return 0;
}

int show_move(FILE* f, move m) {
    if (m.flags&nullmove) {
        fprintf(f, " <null>");
    } else {
        fprintf(f, " %c%c%i-%c%i",
            piece_char[m.moved],
            'a'+m.from%NC, 1+m.from/NC,
            'a'+m.to%NC, 1+m.to/NC);
        if (m.flags & cap) fprintf(f, "(x%c)", piece_char[m.capped]);
        if (m.drowned) fprintf(f, "(*%c)", piece_char[m.drowned]);
    }
    if      (m.flags & w_win) fprintf(f, "{1-0}");
    else if (m.flags & b_win) fprintf(f, "{0-1}");
    else if (m.flags & w_draw) fprintf(f, "{W:1/2}");
    else if (m.flags & b_draw) fprintf(f, "{B:1/2}");
    return 0;
}

#define    PUSH_MOVE(f_,t_,fl_) do{ \
    ml[n].from=(f_); \
    ml[n].to=(t_); \
    ml[n].flags=(fl_)|(b->colors[(t_)]!=none?cap:0); \
    ml[n].moved=b->pieces[(f_)]; \
    ml[n].capped=b->pieces[(t_)]; \
    ml[n].drowned=0; \
    ++n;}while(0)
#define    PUSH_JUMP(f_,t_,m_,fl_) do{ \
    ml[n].from=(f_); \
    ml[n].to=(t_); \
    ml[n].flags=(fl_)|(b->colors[(m_)]!=none?cap:0); \
    ml[n].moved=b->pieces[(f_)]; \
    ml[n].capped=b->pieces[(m_)]; \
    ml[n].drowned=0; \
    ++n;}while(0)
int gen_moves_monkey(board* b, move* ml) {
    int    /*f, t, dir,*/ n=0;
    /* todo: it */
    return n;
}

int gen_moves(board* b, move* ml) {
    int    f, t, dir, n=0;

    /* todo: drowning */
    for (f=0; f<(NR*NC); ++f) {
        if ((b->colors[f]==b->side)&&(b->pieces[f]!=empty)) {
            switch (b->pieces[f]) {
            case lion:
                /* todo: lion jump */
                for (dir=0; dir<8; ++dir) {
                    if ((t=box_to_board[board_to_box[f]+gsteps[dir]])!=-1) {
                        if ((b->colors[t]!=b->side)
                            && (((b->side==white)&&(squares[t]==wden))||((b->side==black)&&(squares[t]==bden))))
                            PUSH_MOVE(f,t,0);
                    }
                }
                break;
            case elephant:
                for (dir=0; dir<4; ++dir) {
                    if ((t=box_to_board[board_to_box[f]+esteps[dir]])!=-1) {
                        int tt;
                        if (b->colors[t]!=b->side)
                            PUSH_MOVE(f,t,0);
                        if ((tt=box_to_board[board_to_box[t]+esteps[dir]])!=-1) {
                            if (b->colors[tt]!=b->side)
                                PUSH_MOVE(f,tt,0);
                        }
                    }
                }
                break;
            case giraffe:
                for (dir=0; dir<8; ++dir) {
                    if ((t=box_to_board[board_to_box[f]+gsteps[dir]])!=-1) {
                        int tt;
                        if (b->colors[t]==none)
                            PUSH_MOVE(f,t,0);
                        if ((tt=box_to_board[board_to_box[t]+gsteps[dir]])!=-1) {
                            if (b->colors[tt]!=b->side)
                                PUSH_MOVE(f,tt,0);
                        }
                    }
                }
                break;
            case zebra:
                for (dir=0; dir<8; ++dir) {
                    if ((t=box_to_board[board_to_box[f]+zsteps[dir]])!=-1) {
                        if (b->colors[f]!=b->side)
                            PUSH_MOVE(f,t,0);
                    }
                }
                break;
            case crocodile:
                /* todo: river swim/slide */
                if (squares[f]==river) {
                    for (dir=0; dir<8; ++dir) {
                        if ((t=box_to_board[board_to_box[f]+gsteps[dir]])!=-1) {
                            if (b->colors[t]!=b->side)
                                PUSH_MOVE(f,t,0);
                        }
                    }
                    /* swim */
                } else {
                    for (dir=0; dir<8; ++dir) {
                        if ((t=box_to_board[board_to_box[f]+gsteps[dir]])!=-1) {
                            if (b->colors[t]!=b->side)
                                PUSH_MOVE(f,t,0);
                        }
                    }
                    /* slide */
                }
                break;
            case monkey:
                for (dir=0; dir<8; ++dir) {
                    if ((t=box_to_board[board_to_box[f]+gsteps[dir]])!=-1) {
                        if (b->colors[t]==none) {
                            PUSH_MOVE(f,t,0);
                        } else if (b->colors[t]==b->xside) {
                            int tt;
                            if ((tt=box_to_board[board_to_box[t]+gsteps[dir]])!=-1) {
                                if (b->colors[tt]==none) {
                                    PUSH_JUMP(f,tt,t,monkeyjump);
                                }
                            }
                        }
                    }
                }
                break;
            case pawn:
                if (b->side==white) {
                    for (dir=10; dir<=12; ++dir) {
                        if ((t=box_to_board[board_to_box[f]+dir])!=-1) {
                            if (b->colors[t]!=b->side)
                                PUSH_MOVE(f,t,(t>=42?promote:0));
                        }
                    }
                    if (f>=28) {
                        t = f-7;
                        if (b->colors[t]==none) {
                            PUSH_MOVE(f,t,0);
                            int tt = t-7;
                            if (b->colors[tt]==none)
                                PUSH_MOVE(f,tt,0);
                        }
                    }
                } else {
                    for (dir=-12; dir<=-10; ++dir) {
                        if ((t=box_to_board[board_to_box[f]+dir])!=-1) {
                            if (b->colors[t]!=b->side)
                                PUSH_MOVE(f,t,t<7?promote:00);
                        }
                    }
                    if (f<21) {
                        t = f+7;
                        if (b->colors[t]==none) {
                            PUSH_MOVE(f,t,0);
                            int tt = t+7;
                            if (b->colors[tt]==none)
                                PUSH_MOVE(f,tt,0);
                        }
                    }
                }
                break;
            case superpawn:
                /* todo: it */
                break;
            default:
                break;
            }
        }
    }

    return n;
}

int gen_moves_cap(board* b, move* ml) {
    int    f, /*t, dir,*/ n=0;

    for (f=0; f<(NR*NC); ++f) {
        if ((b->colors[f]==b->side)&&(b->pieces[f]!=empty)) {
            /* todo: it */
        }
    }

    return n;
}

/* NB: only checks if last board position is 3rd-time repeat */
int check3rep(board* b) {
    int    i, c=1;
    hasht    h = b->hash_hist[b->ply-1];
    for (i=0; i<b->ply-1; ++i)
        if (b->hash_hist[i]==h)
            ++c;
    return c>=3;
}

move null_move() {
    move m = {0,0,0,0,0,nullmove};
    return m;
}

move make_move(board* b, move m) {
    int    check_3_rep = 1;
    colort mover = b->side;
    if (b->side==none) { printf("<NOBODY TO MOVE IN MAKE_MOVE?!>\n"); return m; }
    if (1)
    {
      if (m.from>=NR*NC || m.to>=NR*NC
        || m.moved>=empty
        || m.capped>empty
      )
      {
        printf("<INVALID MOVE in make_move() : 0m%08X=[f=%i t=%i c=%i m=%i d=%i f=%i]>\n",
          *((int*)&m), m.from, m.to, m.capped, m.moved, m.drowned, m.flags);
      }
    }

    if (m.flags & nullmove) {
        if (b->side==white) {
            b->side = black;
            b->xside = white;
        } else if (b->side==black) {
            b->side = white;
            b->xside = black;
        } else {
            /* TODO: */
        }
        b->hash_hist[b->ply] = hash_board(b);
        b->hist[b->ply] = m;
        ++b->ply;
        return m;
    }

    b->pieces[m.to] = b->pieces[m.from];
    b->colors[m.to] = b->side;
    b->pieces[m.from] = empty;
    b->colors[m.from] = none;

    if (m.flags&w_win) {
        b->side = none;
        b->xside = white;
    } else if (m.flags&b_win) {
        b->side = none;
        b->xside = black;
    } else if (m.flags&draw) {
        b->side = none;
        b->xside = none;
    } else if (b->side==white) {
        b->side = black;
        b->xside = white;
    } else if (b->side==black) {
        b->side = white;
        b->xside = black;
    } else {
        /* TODO: */
    }

    b->hash_hist[b->ply] = hash_board(b);
    b->hist[b->ply] = m;
    ++b->ply;

    if (1 && check_3_rep && check3rep(b)) {
        /* TODO: this is broken!!!! */
        check_3_rep = 0;
        printf("===3===\n");
        if (mover==white)
            m.flags |= w_draw;
        else
            m.flags |= b_draw;

        b->side = none;
        b->xside = none;

        b->hash_hist[b->ply-1] = hash_board(b);
        b->hist[b->ply-1] = m;
    }

    return m;
}

int unmake_move(board* b, move m) {
    colort    moved = ((b->side==black) || (m.flags&(w_win|w_draw))) ? white : black;

    if (m.flags & nullmove) {
        if (moved==white) {
            b->side = white;
            b->xside = black;
        } else if (moved==black) {
            b->side = black;
            b->xside = white;
        } else {
            /* TODO */
            printf("<WHOOPSIE in unmake_move(nullmove)>\n");
        }
        --b->ply;
        hash_board(b);
        return 0;
    }

    b->pieces[m.from] = m.moved;
    b->colors[m.from] = moved;
    if (m.flags&cap) {
        b->pieces[m.to] = m.capped;
        b->colors[m.to] = moved==white ? black : white;
    } else {
        b->pieces[m.to] = empty;
        b->colors[m.to] = none;
    }

    if (moved==white) {
        b->side = white;
        b->xside = black;
    } else if (moved==black) {
        b->side = black;
        b->xside = white;
    } else {
        /* TODO */
    }
    --b->ply;
    hash_board(b);
    return 0;
}

int unmake(board* b) {
    return unmake_move(b, b->hist[b->ply-1]);
}

/* ************************************************************ */

static const evalt piece_value[8] = {1000, 300, 300, 400, 400, 800, 200, 400};
eval_params    eparams;

void setup_default_eval_params(eval_params* ep) {
    memcpy(ep->piece_value, piece_value, sizeof(evalt)*8);
}

evalt evaluate(board* b, int ply) {
    switch (settings.evaluator) {
    case 2:  return evaluate_gen(b,ply,&eparams);
    case 1:  return evaluate2(b,ply);
    case 0:  return evaluate1(b,ply);
    default: return evaluate1(b,ply);
    }
}

evalt evaluate1(board* b, int ply) {
    ++nodes_evaluated;
    if (b->side==none) {
        if (b->xside==white) return 1000000 - 10*ply;
        else if (b->xside==black) return -1000000 + 10*ply;
        else if (b->xside==none) return 0;
    }

    int    i;
    evalt    sum = 0;

    for (i=0; i<NR*NC; ++i) {
        if (b->pieces[i]!=empty) {
            evalt val;
            if (b->colors[i]==white) {
                val =   piece_value[b->pieces[i]];
            } else {
                val = - piece_value[b->pieces[i]];
            }
            sum += val;
        }
    }

    if (RANDOM_EVAL) sum += abs(b->hash % 11)-5;

    return sum;
}
evalt evaluate2(board* b, int ply) {
    ++nodes_evaluated;
    if (b->side==none) {
        if (b->xside==white) return 1000000 - 10*ply;
        else if (b->xside==black) return -1000000 + 10*ply;
        else if (b->xside==none) return 0;
    }

    int    i;
    evalt    sum = 0;

    for (i=0; i<NR*NC; ++i) {
        if (b->pieces[i]!=empty) {
            evalt val;
            if (b->colors[i]==white) {
                val =  piece_value[b->pieces[i]];
            } else {
                val = -piece_value[b->pieces[i]];
            }
            sum += val;
        }
    }

    if (RANDOM_EVAL) sum += abs(b->hash % 11)-5;

    return sum;
}

/* ************************************************************ */
/* ************************************************************ */

evalt evaluate_gen(board* b, int ply, eval_params* ep) {
    ++nodes_evaluated;
    if (b->side==none) {
        if (b->xside==white) return 1000000 - 10*ply;
        else if (b->xside==black) return -1000000 + 10*ply;
        else if (b->xside==none) return 0;
    }

    int    i;
    evalt    sum = 0;

    for (i=0; i<NR*NC; ++i) {
        if (b->pieces[i]!=empty) {
            evalt val;
            if (b->colors[i]==white) {
                val =   ep->piece_value[b->pieces[i]] + ep->piece_square_bonus[b->pieces[i]][i];
            } else {
                val = - ep->piece_value[b->pieces[i]] - ep->piece_square_bonus[b->pieces[i]][(NR-1-i/NC) + (NC-1-i%NC)];
            }
            sum += val;
        }
    }

    if (RANDOM_EVAL) sum += (abs(b->hash % 201)-100)/100.0;

    return sum;
}

evalt    eval_r(board* b, eval_params* ep, evalt beta) {
    return tanh(beta * evaluate_gen(b,0,ep));
}
void    d_eval_r_dw(board* b, eval_params* ep, eval_params* res, evalt beta) {
    const int n=sizeof(eval_params)/sizeof(evalt);
    int    i;
    eval_params    bumped = *ep;

    /* ugly hackery --- maybe better to just have arrays? */
    for (i=0; i<n; ++i) {
        evalt    ev_up, ev_dn;
        const double    bump = 1.0;

        ((evalt*)(&bumped))[i] = ((evalt*)ep)[i] + bump;
        ev_up = eval_r(b, &bumped, beta);

        ((evalt*)(&bumped))[i] = ((evalt*)ep)[i] - bump;
        ev_dn = eval_r(b, &bumped, beta);

        ((evalt*)res)[i] = (ev_up - ev_dn) / (2*bump);

        ((evalt*)(&bumped))[i] = ((evalt*)ep)[i];
    }
}


#define ALPHA   55.000
#define BETA    0.00425
#define LAMBDA  0.990

int do_td_lambda_leaf(int n, game_move* moves, colort result, eval_params* epin, eval_params* epout)
{
    int nnn = n/2+1;
    evalt        d_arr[nnn];
    //evalt        evals[nnn];
    evalt        r_evals[nnn];
    eval_params    partials[nnn];
    board        b;
    int    n_ev = sizeof(eval_params) / sizeof(evalt);
    int    i, j;

    init_board(&b);
    for (i=0; i<n; i+=2) {
        for (j=0; j<moves[i].n_pv; ++j) make_move(&b, moves[i].pv[j]);

        //evals[i/2] = evaluate_gen(&b, 0, epin);
        r_evals[i/2] = eval_r(&b, epin, BETA);
        d_eval_r_dw(&b, epin, &(partials[i/2]), BETA);
        //printf("%i %f %f %f\n", i, moves[i/2].evaluation, evals[i/2], r_evals[i/2]);

        for (j=0; j<moves[i].n_pv; ++j) unmake(&b);

        make_move(&b, moves[i].move_made);
        if (i+1<n)
            make_move(&b, moves[i+1].move_made);
    }
    if (b.side!=none) {
        /* make it a draw, probably game went too long */
        r_evals[nnn-1] = 0.0;
    } else {
        r_evals[nnn-1] = b.xside==white ? 1.0 : -1.0;
    }
    for (i=0; i<nnn-1; ++i) {
        d_arr[i] = r_evals[i+1] - r_evals[i];
        //printf(">> %6.2f - %6.2f = %6.2f  ", r_evals[i+1], r_evals[i], d_arr[i]);
        //for (j=0; j<8; ++j) printf(" %5.1f", 1000*((evalt*)&(partials[i]))[j]);
        //printf("\n");
    }

    for (j=0; j<n_ev; ++j) {
        evalt     sum = 0.0;
        for (i=0; i<nnn-1; ++i) {
            evalt    subsum = 0.0;
            int    m;
            for (m=i; m<nnn-1; ++m)
                subsum += d_arr[m] * pow(LAMBDA, m-i);
            sum += ((evalt*)&(partials[i]))[j]*subsum;
        }
        ((evalt*)epout)[j] = ((evalt*)epin)[j] + ALPHA * sum;
    }

    return 0;
}

colort self_play_game(int *n_moves, game_move *moves)
{
    board    b;
    int        n;

    init_board(&b);
    for (n=0; n<512; ++n) {
        evalt    val;
        move    m;
        init_ttable();
        m = search5(&b, &val);
        make_move(&b, m);

        moves[n].move_made = m;
        moves[n].evaluation = val;
        moves[n].n_pv = pv_length[0];
        memcpy(moves[n].pv, pv[0], sizeof(move)*pv_length[0]);

        if (b.side==none) break;
    }
    *n_moves = n+1;

    return b.side==none ? b.xside : none;
}

/* ************************************************************ */
/* ************************************************************ */
