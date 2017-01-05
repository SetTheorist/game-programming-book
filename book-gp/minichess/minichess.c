#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/*************************************************************
 *
 * Simple, naive program to play a mini version
 * of chess:
 *		6x6 board
 *		no castling
 *		no double-pawn move or e.p.
 *		no checkmate (capture king)
 *
 *************************************************************/

typedef	unsigned char	uint8;
typedef	unsigned short	uint16;
typedef	unsigned int	uint32;
typedef	unsigned long long	uint64;

#undef	max
#define	max(a,b)	((a)>(b)?(a):(b))
#undef	min
#define	min(a,b)	((a)<(b)?(a):(b))

#define	white	1
#define	none	0
#define	black	2

#define	playing	0
#define	draw	1
#define	win		2
#define	loss	3

#define	blank	0
#define	pawn	1
#define	knight	2
#define	bishop	3
#define	rook	4
#define	queen	5
#define	king	6

#define	NC	6
#define	NR	6

const signed char piece_code[] = {'.','P','N','B','R','Q','K'};

/* board representation:
 * double array with a1 = 0,0; f6 = 5,5
 */

typedef struct board {
	uint32	piece[NR][NC];
	uint32	color[NR][NC];
	uint32	side;
	uint32	other;
	int		ply;
	int		result;
} board;

/* flags (msb to lsb):
 *   promotion_piece: 3
 *   captured_piece:  3
 *   is_promotion:    1
 *   is_capture:      1
 */
#define	f_cap	1
#define	f_pro	2

typedef struct move {
	uint8	from;
	uint8	to;
	uint8	piece;
	uint8	flags;
} move;

const uint8 initial_lineup[] = {rook,bishop,queen,king,knight,rook};

#define	hash_exact 1
#define	hash_upper 2

typedef	uint32	hasht;

typedef struct hash_entry {
	hasht	hash1;
	hasht	hash2;
	int		eval;
	move	best_move;
	uint16	depth;
	uint16	flag;
} hash_entry;

/* hash# ; side-1 ; piece-1 ; row ; col */
hasht	piece_hash[2][2][6][6][6];
/* (white-to-move) */
hasht	move_hash[2];

int
init_hash() {
	int	h,i,j,k,l;
	for (h=0; h<2; ++h)
		for (i=0; i<2; ++i)
			for (j=0; j<6; ++j)
				for (k=0; k<6; ++k)
					for (l=0; l<6; ++l)
						piece_hash[h][i][j][k][l] = (hasht)lrand48();
	for (h=0; h<2; ++h)
		move_hash[h] = (hasht)lrand48();
	return 0;
}
hasht
hash_board(board* b, int h)
{
	hasht	hash = 0;
	int	r, c;
	if (b->side==white) hash=move_hash[h];
	for (r=0; r<6; ++r)
		for (c=0; c<6; ++c)
			if (b->color[r][c]!=none)
				hash ^= piece_hash[h][b->color[r][c]-1][b->piece[r][c]-1][r][c];
	return hash;
}

int
display_move(FILE* f, board* b, move* m)
{
	fprintf(f, "%c%c%i%c%c%i",
		piece_code[m->piece],
		'a' + (m->from)%NC,
		1 + (m->from)/NC,
		m->flags&f_cap ? 'x' : '-',
		'a' + (m->to)%NC,
		1 + (m->to)/NC
		);
	if (m->flags&f_pro)
		fprintf(f, "=%c", piece_code[m->flags>>5]);
	return 0;
}

int
init_board(board* b)
{
	int c;
	memset(b, 0, sizeof(*b));
	for (c=0; c<NC; ++c)
	{
		b->piece[0][c] = initial_lineup[c];
		b->piece[1][c] = pawn;
		b->piece[NR-2][c] = pawn;
		b->piece[NR-1][c] = initial_lineup[(NC-1)-c];

		b->color[0][c] = b->color[1][c] = white;
		b->color[NR-1][c] = b->color[NR-2][c] = black;
	}
	b->side = white;
	b->other = black;
	b->ply = 1;
	b->result = playing;
	return 0;
}

int
display_board(FILE* f, board* b)
{
	int r,c;
	fprintf(f, "    a  b  c  d  e  f\n");
	fprintf(f, "  +------------------+    %08X  %08X\n", hash_board(b, 0), hash_board(b, 1));
	for (r=NR-1; r>=0; --r)
	{
		fprintf(f, "  |");
		for (c=0; c<6; ++c)
		if (0)
		{
			fprintf(f, " %i/%i ", b->piece[r][c], b->color[r][c]);
		}
		else
		{
			if (b->color[r][c]==white)
				fprintf(f, " %c ", piece_code[b->piece[r][c]]);
			else if (b->color[r][c]==black)
				fprintf(f, " %c ", piece_code[b->piece[r][c]]-'A'+'a');
			else
				fprintf(f, " %c ", piece_code[blank]);
		}
		fprintf(f, "| %i\n", r+1);
	}
	fprintf(f, "  +------------------+\n");
	if (b->result==win)
		fprintf(f, "*** White wins ***\n");
	else if (b->result==loss)
		fprintf(f, "*** Black wins ***\n");
	else if (b->result==draw)
		fprintf(f, "*** Draw ***\n");
	else {
		if (b->side==white)
			fprintf(f, "White to move\n");
		else if (b->side==black)
			fprintf(f, "Black to move\n");
		else
			fprintf(f, "<ERROR> to move\n");
	}
	return 0;
}

#define MOVE(m,fr,fc,tr,tc,p,fl,n,b) {\
	(m)->from=(fr)*(NC)+(fc);(m)->to=(tr)*(NC)+(tc);(m)->piece=(p);\
	(m)->flags=(fl)|(((fl)&f_cap)?((b)->piece[tr][tc]<<2):0);\
	++n;}

#define	BISHOP_MOVES(pc) \
	for (tr=r+1,tc=c+1;tr<NR && tc<NC && b->color[tr][tc]==none;++tr,++tc) \
		MOVE(ml+num,r,c,tr,tc,pc,0,num,b); \
	if (tr<NR && tc<NC && b->color[tr][tc]==b->other) \
		MOVE(ml+num,r,c,tr,tc,pc,f_cap,num,b); \
	for (tr=r+1,tc=c-1;tr<NR && tc>=0 && b->color[tr][tc]==none;++tr,--tc) \
		MOVE(ml+num,r,c,tr,tc,pc,0,num,b); \
	if (tr<NR && tc>=0 && b->color[tr][tc]==b->other) \
		MOVE(ml+num,r,c,tr,tc,pc,f_cap,num,b); \
	for (tr=r-1,tc=c+1;tr>=0 && tc<NC && b->color[tr][tc]==none;--tr,++tc) \
		MOVE(ml+num,r,c,tr,tc,pc,0,num,b); \
	if (tr>=0 && tc<NC && b->color[tr][tc]==b->other) \
		MOVE(ml+num,r,c,tr,tc,pc,f_cap,num,b); \
	for (tr=r-1,tc=c-1;tr>=0 && tc>=0 && b->color[tr][tc]==none;--tr,--tc) \
		MOVE(ml+num,r,c,tr,tc,pc,0,num,b); \
	if (tr>=0 && tc>=0 && b->color[tr][tc]==b->other) \
		MOVE(ml+num,r,c,tr,tc,pc,f_cap,num,b);

#define	ROOK_MOVES(pc) \
	for (tr=r+1;tr<NR && b->color[tr][c]==none;++tr) \
		MOVE(ml+num,r,c,tr,c,pc,0,num,b); \
	if (tr<NR && b->color[tr][c]==b->other) \
		MOVE(ml+num,r,c,tr,c,pc,f_cap,num,b); \
	for (tr=r-1;tr>=0 && b->color[tr][c]==none;--tr) \
		MOVE(ml+num,r,c,tr,c,pc,0,num,b); \
	if (tr>=0 && b->color[tr][c]==b->other) \
		MOVE(ml+num,r,c,tr,c,pc,f_cap,num,b); \
	for (tc=c+1;tc<NC && b->color[r][tc]==none;++tc) \
		MOVE(ml+num,r,c,r,tc,pc,0,num,b); \
	if (tc<NC && b->color[r][tc]==b->other) \
		MOVE(ml+num,r,c,r,tc,pc,f_cap,num,b); \
	for (tc=c-1;tc>=0 && b->color[r][tc]==none;--tc) \
		MOVE(ml+num,r,c,r,tc,pc,0,num,b); \
	if (tc>=0 && b->color[r][tc]==b->other) \
		MOVE(ml+num,r,c,r,tc,pc,f_cap,num,b);

int
gen_moves(board* b, move* ml)
{
	int	r, c, dir;
	int	num=0;
	dir = (b->side==white) ? +1 : -1;
	for (r=0; r<NR; ++r)
	{
		for (c=0; c<NC; ++c)
		{
			if (b->color[r][c] == b->side) {
				int tc,tr;
				switch (b->piece[r][c])
				{
				case pawn:
					tr=r+dir;
					if (tr>=0 && tr<NR) {
						if (tr==0 || tr==NR-1)
						{
							if (b->piece[tr][c]==blank)
							{
								MOVE(ml+num,r,c,tr,c,pawn,f_pro|(queen<<5),num,b)
								MOVE(ml+num,r,c,tr,c,pawn,f_pro|(rook<<5),num,b)
								MOVE(ml+num,r,c,tr,c,pawn,f_pro|(knight<<5),num,b)
								MOVE(ml+num,r,c,tr,c,pawn,f_pro|(bishop<<5),num,b)
							}
							if ( (c<NC-1) && (b->color[tr][c+1]==b->other) )
							{
								MOVE(ml+num,r,c,tr,c+1,pawn,f_cap|f_pro|(queen<<5),num,b)
								MOVE(ml+num,r,c,tr,c+1,pawn,f_cap|f_pro|(rook<<5),num,b)
								MOVE(ml+num,r,c,tr,c+1,pawn,f_cap|f_pro|(knight<<5),num,b)
								MOVE(ml+num,r,c,tr,c+1,pawn,f_cap|f_pro|(bishop<<5),num,b)
							}
							if ( (c>0) && (b->color[tr][c-1]==b->other) )
							{
								MOVE(ml+num,r,c,tr,c-1,pawn,f_cap|f_pro|(queen<<5),num,b)
								MOVE(ml+num,r,c,tr,c-1,pawn,f_cap|f_pro|(rook<<5),num,b)
								MOVE(ml+num,r,c,tr,c-1,pawn,f_cap|f_pro|(knight<<5),num,b)
								MOVE(ml+num,r,c,tr,c-1,pawn,f_cap|f_pro|(bishop<<5),num,b)
							}
						}
						else
						{
							if (b->piece[tr][c]==blank)
								MOVE(ml+num,r,c,tr,c,pawn,0,num,b)
							if ( (c<NC-1) && (b->color[tr][c+1]==b->other) )
								MOVE(ml+num,r,c,tr,c+1,pawn,f_cap,num,b)
							if ( (c>0) && (b->color[tr][c-1]==b->other) )
								MOVE(ml+num,r,c,tr,c-1,pawn,f_cap,num,b)
						}
					}
					break;
				case knight:
					tr=r-2; tc=c-1;
					if (tr>=0 && tc>=0 && b->color[tr][tc]!=b->side)
						MOVE(ml+num,r,c,tr,tc,knight,b->color[tr][tc]==b->other?f_cap:0,num,b)
					tr=r-2; tc=c+1;
					if (tr>=0 && tc<NC && b->color[tr][tc]!=b->side)
						MOVE(ml+num,r,c,tr,tc,knight,b->color[tr][tc]==b->other?f_cap:0,num,b)
					tr=r+2; tc=c-1;
					if (tr<NR && tc>=0 && b->color[tr][tc]!=b->side)
						MOVE(ml+num,r,c,tr,tc,knight,b->color[tr][tc]==b->other?f_cap:0,num,b)
					tr=r+2; tc=c+1;
					if (tr<NR && tc<NC && b->color[tr][tc]!=b->side)
						MOVE(ml+num,r,c,tr,tc,knight,b->color[tr][tc]==b->other?f_cap:0,num,b)
					tr=r-1; tc=c-2;
					if (tr>=0 && tc>=0 && b->color[tr][tc]!=b->side)
						MOVE(ml+num,r,c,tr,tc,knight,b->color[tr][tc]==b->other?f_cap:0,num,b)
					tr=r-1; tc=c+2;
					if (tr>=0 && tc<NC && b->color[tr][tc]!=b->side)
						MOVE(ml+num,r,c,tr,tc,knight,b->color[tr][tc]==b->other?f_cap:0,num,b)
					tr=r+1; tc=c-2;
					if (tr<NR && tc>=0 && b->color[tr][tc]!=b->side)
						MOVE(ml+num,r,c,tr,tc,knight,b->color[tr][tc]==b->other?f_cap:0,num,b)
					tr=r+1; tc=c+2;
					if (tr<NR && tc<NC && b->color[tr][tc]!=b->side)
						MOVE(ml+num,r,c,tr,tc,knight,b->color[tr][tc]==b->other?f_cap:0,num,b)
					break;
				case bishop:
					BISHOP_MOVES(bishop)
					break;
				case rook:
					ROOK_MOVES(rook)
					break;
				case queen:
					BISHOP_MOVES(queen)
					ROOK_MOVES(queen)
					break;
				case king:
					for (tr=r-1; tr<=r+1; ++tr)
					{
						for (tc=c-1; tc<=c+1; ++tc)
						{
							if ((tr||tc) && tr>=0 && tc>=0 && tr<NR && tc<NC)
							{
								if (b->color[tr][tc] == b->other)
									MOVE(ml+num,r,c,tr,tc,king,f_cap,num,b)
								else if (b->color[tr][tc] == none)
									MOVE(ml+num,r,c,tr,tc,king,0,num,b)
							}
						}
					}
					break;
				}
			}
		}
	}
	return num;
}

const int	piece_val[] = {0, 100, 300, 325, 500, 950, 100000};
#define	max_val	1000000000
#define	draw_val 0
#define	mate(n)	(10000000-1000*(n))
#define	win_val	 10000000
#define	loss_val (-10000000)
int	nodes_evaluated;
int
evaluate(board* b)
{
	int	r,c;
	int	sum = 0;
	++nodes_evaluated;
	/* simple material evaluation */
	for (r=0; r<NR; ++r)
		for (c=0; c<NC; ++c)
			if (b->color[r][c]==white)
				sum += piece_val[b->piece[r][c]];
			else
				sum -= piece_val[b->piece[r][c]];
	/* knight center bonus */
	for (r=2; r<4; ++r)
		for (c=2; c<4; ++c)
			if (b->piece[r][c]==knight)
			{
				if (b->color[r][c]==white)
					sum += 5;
				else
					sum -= 5;
			}
	return sum;
}

int
make_move(board* b, move* m)
{
	int fr=m->from/NC;
	int fc=m->from%NC;
	int tr=m->to/NC;
	int tc=m->to%NC;
	//fprintf(stdout, "** Making move: "); display_move(stdout, b, m); fprintf(stdout, "\n");
	b->color[fr][fc] = none;
	b->piece[fr][fc] = blank;
	b->color[tr][tc] = b->side;
	b->piece[tr][tc] = (m->flags&f_pro) ? (m->flags>>5) : m->piece;
	if (b->side==white)
	{
		b->side = black;
		b->other = white;
	}
	else
	{
		b->side = white;
		b->other = black;
	}
	if ((m->flags&f_cap) && (m->flags>>2==king))
	{
		b->result = (b->side==black) ? win : loss;
	}
	++b->ply;
	//display_board(stdout, b );
	return 0;
}
int
unmake_move(board* b, move* m)
{
	int fr=m->from/NC;
	int fc=m->from%NC;
	int tr=m->to/NC;
	int tc=m->to%NC;
	//fprintf(stdout, "** Unmaking move: "); display_move(stdout, b, m); fprintf(stdout, "\n");
	b->color[fr][fc] = b->other;
	b->piece[fr][fc] = m->piece;
	if (m->flags & f_cap)
	{
		b->color[tr][tc] = b->side;
		b->piece[tr][tc] = (m->flags>>2) & 7;
	}
	else
	{
		b->color[tr][tc] = none;
		b->piece[tr][tc] = blank;
	}
	if (b->side==white)
	{
		b->side = black;
		b->other = white;
	}
	else
	{
		b->side = white;
		b->other = black;
	}
	b->result = playing;
	--b->ply;
	//display_board(stdout, b );
	return 0;
}

/* assume that very initial call will have depth>0 (otherwise no move returned) */
int
search_minmax(board* b, move* m, int depth)
{
	if (b->result != playing)
	{
		if (b->result==win) return win_val;
		else if (b->result==loss) return loss_val;
		else if (b->result==draw) return draw;
	}

	if (depth<=0)
		return evaluate(b);

	move	ml[128];
	int		n, i;
	n = gen_moves( b, ml );
	int		best_m = 0;
	int		best_val = (b->side==white) ? -max_val : max_val;
	for (i=0; i<n; ++i)
	{
		int	val;
		move m;
		make_move(b, ml+i);
		val = search_minmax(b, &m, depth-100);
		unmake_move(b, ml+i);
		if (b->side==white)
		{
			if (val>best_val)
			{
				best_val = val;
				best_m = i;
			}
		}
		else
		{
			if (val<best_val)
			{
				best_val = val;
				best_m = i;
			}
		}
	}

	*m = ml[best_m];
	if (best_val>mate(100))
		best_val-=1000;
	else if (best_val<-mate(100))
		best_val+=1000;
	return best_val;
}

/* assume that very initial call will have depth>0 (otherwise no move returned) */
int
search_negamax(board* b, move* m, int depth)
{
	int	sign = (b->side==white) ? +1 : -1;
	if (b->result != playing)
	{
		if (b->result==win) return sign*win_val;
		else if (b->result==loss) return sign*loss_val;
		else if (b->result==draw) return sign*draw;
	}

	if (depth<=0)
		return sign*evaluate(b);

	move	ml[128];
	int		n, i;
	n = gen_moves( b, ml );
	int		best_m = 0;
	int		best_val = -max_val;
	for (i=0; i<n; ++i)
	{
		int	val;
		move m;
		make_move(b, ml+i);
		val = -search_negamax(b, &m, depth-100);
		unmake_move(b, ml+i);
		if (val>best_val)
		{
			best_val = val;
			best_m = i;
		}
	}

	*m = ml[best_m];
	if (best_val>mate(100))
		best_val-=1000;
	else if (best_val<-mate(100))
		best_val+=1000;
	return best_val;
}

/* assume that very initial call will have depth>0 (otherwise no move returned) */
int
search_alphabeta(board* b, move* m, int depth, int alpha, int beta)
{
	int	sign = (b->side==white) ? +1 : -1;
	if (b->result != playing)
	{
		if (b->result==win) return sign*win_val;
		else if (b->result==loss) return sign*loss_val;
		else if (b->result==draw) return sign*draw;
	}

	if (depth<=0)
		return sign*evaluate(b);

	move	ml[128];
	int		n, i;
	n = gen_moves( b, ml );
	int		best_m = 0;
	for (i=0; i<n; ++i)
	{
		int	val;
		move m;
		make_move(b, ml+i);
		val = -search_alphabeta(b, &m, depth-100, -beta, -alpha);
		unmake_move(b, ml+i);
		if (val > alpha)
		{
			alpha = val;
			best_m = i;
		}
		/* cutoff */
		if (alpha>=beta)
			break;
	}

	*m = ml[best_m];
	if (alpha>mate(100))
		alpha-=1000;
	else if (alpha<-mate(100))
		alpha+=1000;
	return alpha;
}

#define	TABLESIZE	(1024*1024)
hash_entry	table[TABLESIZE];
int	hash_hit;
int	hash_used;
int	hash_extradepth;
/* assume that very initial call will have depth>0 (otherwise no move returned) */
int
search_abtt(board* b, move* m, int depth, int alpha, int beta)
{
	int	sign = (b->side==white) ? +1 : -1;
	int	hashed_eval;
	move	hashed_move;
	int	hashed_flag = 0;
	if (b->result != playing)
	{
		if (b->result==win) return sign*win_val;
		else if (b->result==loss) return sign*loss_val;
		else if (b->result==draw) return sign*draw;
	}

	hasht	h1 = hash_board(b, 0);
	hasht	h2 = hash_board(b, 1);

	/* in the transposition table? */
	if ((table[h1%TABLESIZE].hash1 == h1) && (table[h1%TABLESIZE].hash2 == h2))
	{
		hash_entry he = table[h1%TABLESIZE];
		++hash_hit;
		if ((table[h1%TABLESIZE].flag&hash_exact) && (table[h1%TABLESIZE].depth>=depth))
		{
			if (table[h1%TABLESIZE].depth>depth)
			{
				++hash_extradepth;
			}
			else
			{
				++hash_used;
				//*m = table[h1%TABLESIZE].best_move;
				//return table[h1%TABLESIZE].eval;
				hashed_move = table[h1%TABLESIZE].best_move;
				hashed_eval = table[h1%TABLESIZE].eval;
				hashed_flag = 1;
			}
		}
	}

	if (depth<=0)
		return sign*evaluate(b);

	move	ml[128];
	int		n, i;
	n = gen_moves( b, ml );
	int		best_m = 0;
	for (i=0; i<n; ++i)
	{
		int	val;
		move m;
		make_move(b, ml+i);
		val = -search_abtt(b, &m, depth-100, -beta, -alpha);
		unmake_move(b, ml+i);
		if (val > alpha)
		{
			alpha = val;
			best_m = i;
		}
		/* cutoff */
		if (alpha>=beta)
			break;
	}

	*m = ml[best_m];
	if (alpha>mate(100))
		alpha-=1000;
	else if (alpha<-mate(100))
		alpha+=1000;

	if (hashed_flag)
	{
		printf("(%08X %08X) h_eval=%-8i alpha=%-8i || h_move=%08x  move=%08x (beta=%-i)\n",
			h1, h2, hashed_eval, alpha, *(int*)&hashed_move, *(int*)m, alpha, beta
			);
	}
	/* store in table */
	if (alpha < beta)
	{
		table[h1%TABLESIZE].flag = hash_exact;
		table[h1%TABLESIZE].hash1 = h1;
		table[h1%TABLESIZE].hash2 = h2;
		table[h1%TABLESIZE].depth = depth;
		table[h1%TABLESIZE].eval = alpha;
		table[h1%TABLESIZE].best_move = *m;

		printf(">> Storing (%08X %08X)   hashed_eval=%-8i ||  hashed_move=%08x \n",
			h1, h2, alpha, *(int*)m
			);
	}

	return alpha;
}

unsigned int read_clock(void) {
	struct timeval timeval;
	struct timezone timezone;
	gettimeofday(&timeval, &timezone);
	return (timeval.tv_sec * 1000000 + (timeval.tv_usec));
}

int
main(int argc, char* argv[])
{
	board	b;
	int		n=0, i, j, val;
	move	game[10*1024];
	int		white_depth = 500;
	int		black_depth = 100;

	srand(-1317);
	srand48(-1317L);

	init_hash();
	memset(table, 0, sizeof(table));
	printf("%i\n", sizeof(hash_entry));

	init_board( &b );

	for (j=0; b.result==playing; ++j)
	{
		unsigned int	bt, et;
		//display_board(stdout, &b );
		nodes_evaluated = 0;
		hash_hit = 0;
		hash_used = 0;
		hash_extradepth = 0;
		/* why do we need this? */
		memset(table, 0, sizeof(table));
		bt = read_clock();
		switch ((b.side==white) ? (argc>1 ? atoi(argv[1]) : 0) : (argc>2 ? atoi(argv[2]) : 0))
		{
		case 0: val = search_minmax( &b, &game[j], b.side==white ? white_depth : black_depth); break;
		case 1: val = search_negamax( &b, &game[j], b.side==white ? white_depth : black_depth); break;
		case 2: val = search_alphabeta( &b, &game[j], b.side==white ? white_depth : black_depth, -max_val, max_val); break;
		default:
		case 3: val = search_abtt( &b, &game[j], b.side==white ? white_depth : black_depth, -max_val, max_val); break;
		//case 4: val = search_abttid( &b, &game[j], b.side==white ? white_depth : black_depth, -max_val, max_val); break;
		}
		et = read_clock();
		printf("%i%s", (1+b.ply)/2, b.side==white?".":"...");
		display_move( stdout, &b, &game[j] );
		double nps = nodes_evaluated / ((et-bt)*1e-6);
		printf(" [%i]   {%in %.3fs @ %.2f%s}\n", val,
			nodes_evaluated, (et-bt)*1e-6,
			nps>1e6 ? nps*1e-6 : nps>1e3 ? nps*1e-3 : nps,
			nps>1e6 ? "Mnps" : nps>1e3 ? "Knps" : "nps"
			);
		printf("   ht=%i hu=%i he=%i\n",
			hash_hit, hash_used, hash_extradepth);
		make_move(&b, &game[j]);
	}
	display_board(stdout, &b );

	return 0;
}

