/* $Id: $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* Phutball ("philosopher's football")
 *
 */

typedef unsigned char		uint8;
typedef unsigned short		uint16;
typedef unsigned int		uint32;
typedef unsigned long long	uint64;

unsigned int read_clock(void) {
	struct timeval timeval;
	struct timezone timezone;
	gettimeofday(&timeval, &timezone);
	return (timeval.tv_sec * 1000000 + (timeval.tv_usec));
}

#define	NR	19
#define	NC	15
#define	NR2	(NR+2)
#define	NC2	(NC+2)

typedef enum colort {
	white, black, white_jump, black_jump, none
} colort;
typedef enum piecet {
	empty, stone, ball, w_goal, b_goal, invalid
} piecet;

typedef	struct board {
	piecet	pieces[NR2*NC2];
	colort	side;
	int		ball_loc;
} board;
#define	RC2(r,c)	((r+1)*NC2 + (c+1))

const int dirs[8] = {+1, +1-NC2, -NC2, -1-NC2, -1, -1+NC2, +NC2, +NC2+1};

typedef enum movet {
	 null, drop, jump
} movet;
typedef struct move {
	uint8	type;
	uint8	flags;
	uint16	dest;
} move;

void init_board(board* b)
{
	int	i;
	memset(b->pieces, 0, sizeof(b->pieces));
	for (i=-1; i<=NC; ++i) b->pieces[RC2(-1,i)] = w_goal;
	for (i=-1; i<=NC; ++i) b->pieces[RC2(NR,i)] = b_goal;
	for (i=-1; i<=NR; ++i) b->pieces[RC2(i,-1)] = invalid;
	for (i=-1; i<=NR; ++i) b->pieces[RC2(i,NC)] = invalid;
	b->ball_loc = RC2(9,7);
	b->pieces[RC2(9,7)] = ball;
	b->side = white;
}
void
show_move(FILE* f, move m)
{
	if (m.type==null) {
		fprintf(f, "|");
	} else if (m.type==drop) {
		fprintf(f, "@%c%i", 'A'+(m.dest%NC2)-1, 1+(m.dest/NC2)-1);
	} else if (m.type==jump) {
		fprintf(f, "x%c%i", 'A'+(m.dest%NC2), 1+(m.dest/NC2)-1);
	} else {
	}
}
void
show_board(FILE* f, board* b)
{
	int r, c;
	fprintf(f, " "); for (c=0; c<NC; ++c) fprintf(f, "%c", 'A'+c); fprintf(f, "\n");
	for (r=NR; r>=-1; --r) {
		for (c=-1; c<=NC; ++c) {
			if ((r+c)%2) {
				fprintf(f, "%c", " XOwb|"[b->pieces[RC2(r,c)]]);
			} else {
				fprintf(f, "%c", ".XOwb|"[b->pieces[RC2(r,c)]]);
			}
		}
		if ((r==NR)||(r==-1))
			fprintf(f, "\n");
		else
			fprintf(f, "%2i\n", r+1);
	}
	fprintf(f, " "); for (c=0; c<NC; ++c) fprintf(f, "%c", 'A'+c); fprintf(f, "\n");
}
int
gen_moves(board* b, move* ml)
{
	int	i, d, n=0;
	/* drops */
	if ((b->side==white) || (b->side==black)) {
		for (i=0; i<(NR+2)*(NC+2); ++i) {
			if (b->pieces[i]==empty) {
				ml[n].type=drop;
				ml[n].dest=i;
				ml[n].flags=0;
				++n;
			}
		}
	}
	/* end of jump series */
	if ((b->side==white_jump)|(b->side==black_jump)) {
		ml[n].type=null;
		ml[n].flags=0;
		++n;
	}
	/* jumps */
	for (i=0; i<8; ++i) {
		d = b->ball_loc + dirs[i];
		if (b->pieces[d]==stone) {
			while (b->pieces[d]==stone)
				d+=dirs[i];
			if (b->pieces[d]==empty) {
				ml[n].type=jump;
				ml[n].dest=d;
				ml[n].flags=0;
				++n;
			}
		}
	}

	return n;
}

int
main(int argc, char* argv[])
{
	int		i, n;
	board	b;
	move	ml[1024];

	srand48(-13);
	init_board(&b);
	for (i=0; i<30; ++i)
		b.pieces[RC2(((uint32)lrand48())%NR,((uint32)lrand48())%NC)] = stone;
	show_board(stdout, &b);

	n = gen_moves(&b, ml);
	for (i=0; i<n; ++i) {
		show_move(stdout, ml[i]);
		if ((i%10)==9)
			printf("\n");
		else
			printf(" ");
	}
	printf("\n");

	return 0;
}
