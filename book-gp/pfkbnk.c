
/*
 *	pfkbnk.c (pretty fast kbnk generator)
 *
 *	This demonstration program solves the kbnk
 *	endgame by means of the bitmapped database
 *	method as proposed by Urban Koistinen.
 *
 *	written by Marcel van Kervinck
 *	<marcelk@stack.urc.tue.nl>
 *
 *	18 March 1996
 */

/*
 *	platform			cputime (seconds)
 *
 *	68000-7MHz Amiga		1602
 *	486-66MHz FreeBSD		45
 *	Sun Sparc SLC			36
 *	Pentium-90MHz FreeBSD		16
 *	SG Challenge IRIX		12
 */


/* include the usual stuff */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>


/* some 64 bit definitions for gcc, change these for other compilers */

typedef unsigned long long bitmap;
#define I  0x0000000000000001ULL
#define B5 0xffffffff00000000ULL
#define B4 0xffff0000ffff0000ULL
#define B3 0xff00ff00ff00ff00ULL
#define B2 0xf0f0f0f0f0f0f0f0ULL
#define B1 0xccccccccccccccccULL
#define B0 0xaaaaaaaaaaaaaaaaULL
#define W  0x7f7f7f7f7f7f7f7fULL
#define E  0xfefefefefefefefeULL


/* bit tricks */

#define FIRST(b) ((b)&(~(b)+1))	/* return first bit */
	/* compiler should be smart enough to generate b&-b */

#define DROP(b) ((b)&=((b)-1))	/* drop first bit */


/* general macros */

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

#define TIME difftime(time(NULL),starttime)


/* chess macros */

#define STEP_N(b) ((b) << 8)
#define STEP_S(b) ((b) >> 8)
#define STEP_W(b) (((b) & W) << 1)
#define STEP_E(b) (((b) & E) >> 1)

#define STEP_NE(b) STEP_N(STEP_E(b))
#define STEP_NW(b) STEP_N(STEP_W(b))
#define STEP_SE(b) STEP_S(STEP_E(b))
#define STEP_SW(b) STEP_S(STEP_W(b))

#define KING(b) \
	( STEP_N(STEP_W(b) | STEP_E(b) | (b)) \
	| STEP_W(b) | STEP_E(b) \
	| STEP_S(STEP_W(b) | STEP_E(b) | (b)) )

#define KNIGHT(b) \
	( STEP_N( STEP_N(STEP_W(b)|STEP_E(b)) \
		| STEP_W(STEP_W(b)) \
		| STEP_E(STEP_E(b)) ) \
	| STEP_S( STEP_S(STEP_W(b)|STEP_E(b)) \
		| STEP_W(STEP_W(b)) \
		| STEP_E(STEP_E(b)) ) )

/* memory structures */

bitmap wtm[10][64][64][2];	/* wtm database, 2 bits per position */
				/* 0: score>=mate    1: score<mate   */
				/* 2: score=mate     3: score=mate-1 */

bitmap mask[2][64];		/* bitnumber to bitmap */
short slide[64][4][8];		/* sliding moves */
short king[64][9];		/* king moves */
short knight[64][9];		/* knight moves */

short map[64] = {		/* map king to database index */
	0,1,2,3,0,0,0,0,
	0,4,5,6,0,0,0,0,
	0,0,7,8,0,0,0,0,
	0,0,0,9,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
};

short mir[64] = {		/* reflection bits */
	0,0,0,0,1,1,1,1,
	4,0,0,0,1,1,1,5,
	4,4,0,0,1,1,5,5,
	4,4,4,0,1,5,5,5,
	6,6,6,2,3,7,7,7,
	6,6,2,2,3,3,7,7,
	6,2,2,2,3,3,3,7,
	2,2,2,2,3,3,3,3,
};

short dia[64] = {		/* reflection over diagonal */
	0, 8,16,24,32,40,48,56,
	1, 9,17,25,33,41,49,57,
	2,10,18,26,34,42,50,58,
	3,11,19,27,35,43,51,59,
	4,12,20,28,36,44,52,60,
	5,13,21,29,37,45,53,61,
	6,14,22,30,38,46,54,62,
	7,15,23,31,39,47,55,63,
};

short factor[64] = {		/* multiplication for position counts */
	4,8,8,8,8,8,8,4,
	8,4,8,8,8,8,4,8,
	8,8,4,8,8,4,8,8,
	8,8,8,4,4,8,8,8,
	8,8,8,4,4,8,8,8,
	8,8,4,8,8,4,8,8,
	8,4,8,8,8,8,4,8,
	4,8,8,8,8,8,8,4,
};


short bitcount(bitmap b)	/* count number of bits */
{
	short i;

	for(i=0; b; b&=b-1)
		i++;

	return i;
}


unsigned init(void)		/* init global data */
{
	bitmap wkb, wbb, wnb, b, c, ray[64][8];
	short wk, wb, wn, i, *m;
	unsigned count;


	/* move tables and others */

	for(wb=0, wbb=I; wb<64; wbb<<=1, wb++) {

		for(b=wbb, i=0; ~b&STEP_NE(b); b|=STEP_NE(b), i++)
			slide[wb][0][i] = wb+7+7*i;
		ray[wb][0] = b & ~wbb;
		slide[wb][0][i] = -1;

		for(b=wbb, i=0; ~b&STEP_NW(b); b|=STEP_NW(b), i++)
			slide[wb][1][i] = wb+9+9*i;
		ray[wb][1] = b & ~wbb;
		slide[wb][1][i] = -1;

		for(b=wbb, i=0; ~b&STEP_SW(b); b|=STEP_SW(b), i++)
			slide[wb][2][i] = wb-7-7*i;
		ray[wb][2] = b & ~wbb;
		slide[wb][2][i] = -1;

		for(b=wbb, i=0; ~b&STEP_SE(b); b|=STEP_SE(b), i++)
			slide[wb][3][i] = wb-9-9*i;
		ray[wb][3] = b & ~wbb;
		slide[wb][3][i] = -1;

		for(i=0, m=king[wb]; i<64; i++)
		if(i!=wb && abs((i%8)-(wb%8))<=1 && abs((i/8)-(wb/8))<=1)
			*m++ = i;
		*m = -1;

		for(i=0, m=knight[wb]; i<64; i++)
		if(abs( ((i%8)-(wb%8)) * ((i/8)-(wb/8)) )==2)
			*m++ = i;
		*m = -1;

		mask[0][wb] = I<<wb;
		mask[1][wb] = ~(I<<wb);

	}


	/* mark relevant illegal positions */

	count = 0;
	for(wk=0, wkb=I; wk<64; wkb<<=1, wk++) if(!mir[wk])
	for(wb=0, wbb=I; wb<64; wbb<<=1, wb++)
	for(wn=0, wnb=I; wn<64; wnb<<=1, wn++) {

		b = KING(wkb) | KNIGHT(wnb);
		for(i=0; i<4; i++) {
			c = ray[wb][i];
			if(c & wkb)
				c &= ~ray[wk][i];
			if(c & wnb)
				c &= ~ray[wn][i];
			b |= c;
		}
		b &= ~wkb;

		wtm[map[wk]][wb][wn][0] = b;
		wtm[map[wk]][wb][wn][1] = 0;
		count += factor[wk]
			* (wk==wb || wk==wn || wb==wn ? 64 : bitcount(b | wkb | wbb | wnb) );
	}

	return count;
}


unsigned new2old(void)	/* mark new found positions as old */
{
	short wk, wb, wn;
	unsigned count;
	bitmap b;

	count = 0;
	for(wk=0; wk<64; wk++) if(!mir[wk])
	for(wb=0; wb<64; wb++)
	for(wn=0; wn<64; wn++) {
		b = ~wtm[map[wk]][wb][wn][0] & wtm[map[wk]][wb][wn][1];
		wtm[map[wk]][wb][wn][0] |= b;
		wtm[map[wk]][wb][wn][1] &= b;
		count += factor[wk] * bitcount(b);
	}
	return count;
}


short index(bitmap b)	/* return bitnumber (pre: bitcount(b)==1) */
{
	return	(b & B5 ? 32 : 0)
	+	(b & B4 ? 16 : 0)
	+	(b & B3 ? 8 : 0)
	+	(b & B2 ? 4 : 0)
	+	(b & B1 ? 2 : 0)
	+	(b & B0 ? 1 : 0);
}


void enter(short wk, short wb, short wn, bitmap b)	/* add wtm positions */
{
	bitmap c;

	if(b) {
		if(mir[wk] & 1) {	/* mirror vertical */
			wk ^= 007;
			wb ^= 007;
			wn ^= 007;
			for(c=b, b=0; c; DROP(c))
				b |= mask[0][index(FIRST(c))^007];
		}
		if(mir[wk] & 2) {	/* mirror horizontal */
			wk ^= 070;
			wb ^= 070;
			wn ^= 070;
			for(c=b, b=0; c; DROP(c))
				b |= mask[0][index(FIRST(c))^070];
		}
		if(mir[wk] & 4) {	/* mirror diagonal */
			wk = dia[wk];
			wb = dia[wb];
			wn = dia[wn];
			for(c=b, b=0; c; DROP(c))
				b |= mask[0][dia[index(FIRST(c))]];
		}

		if( (b &= ~wtm[map[wk]][wb][wn][0]
			& ~wtm[map[wk]][wb][wn][1]) ) {	/* which are new? */

			wtm[map[wk]][wb][wn][1] |= b;	/* enter those */

			if(dia[wk]==wk) {		/* and the doubles */
				wb = dia[wb];
				wn = dia[wn];
				for(c=0; b; DROP(b))
					c |= mask[0][dia[index(FIRST(b))]];
				wtm[map[wk]][wb][wn][1] |= c;
			}
		}
	}
}


void retro_btm(short wk, short wb, short wn, bitmap b)	/* process btm positions */
{
	short i, *m;

	/* find all white moves that lead to the btm positions */


		/* white bishop */

	for(i=0; i<4; i++)
	for(m=slide[wb][i]; *m>=0 && *m!=wk && *m!=wn; m++)
		enter(wk,*m,wn,b & mask[1][*m]);


		/* white knight */

	for(m=knight[wn]; *m>=0; m++)
	if(*m!=wk && *m!=wb)
		enter(wk,wb,*m,b & mask[1][*m]);


		/* white king */

	for(m=king[wk]; *m>=0; m++)
	if(*m!=wb && *m!=wn)
		enter(*m,wb,wn,b & mask[1][*m]);
}


void retro_wtm(short wk, short wb, short wn, bitmap b)	/* process wtm positions */
{
	/* find forced black moves that lead to the wtm positions */


		/* black king */

	if( (b = KING(b) & mask[1][wb] & mask[1][wn]
		& ~KING(~wtm[map[wk]][wb][wn][0])) )
		retro_btm(wk,wb,wn,b);
}


int main(void)					/* compute database */
{
	bitmap b;
	short wk, wb, wn;
	unsigned count, total;
	int mate;
	time_t starttime;


	/* init */

	starttime = time(NULL);

	printf("kbnk wtm\n");
	printf(" status    count time\n");

	total = init();

	printf("illegal %8u (%1.0f)\n", total, TIME);


	/* enter mates */

	mate = 1;

	for(wk=0; wk<64; wk++) if(!mir[wk])
	for(wb=0; wb<64; wb++) if(wb!=wk) if(dia[wk]!=wk || dia[wb]<=wb)
	for(wn=0; wn<64; wn++) if(wn!=wk && wn!=wb) if(dia[wk]!=wk || dia[wb]!=wb || dia[wn]<=wn)
		if( (b = wtm[map[wk]][wb][wn][0] & ~KING(~wtm[map[wk]][wb][wn][0])) )
			retro_btm(wk,wb,wn,b);


	/* iterate */

	while( (count = new2old()) ) {

		total += count;

		printf("mate%03d %8u (%1.0f)\n", mate, count, TIME);
		count = 0;

		mate++;

		for(wk=0; wk<64; wk++) if(!mir[wk])
		for(wb=0; wb<64; wb++) if(dia[wk]!=wk || dia[wb]<=wb)
		for(wn=0; wn<64; wn++) if(dia[wk]!=wk || dia[wb]!=wb || dia[wn]<=wn)
			if( (b = wtm[map[wk]][wb][wn][0] & wtm[map[wk]][wb][wn][1]) )
				retro_wtm(wk,wb,wn,b);
	}


	/* count left overs */

	for(wk=0; wk<64; wk++) if(!mir[wk])
	for(wb=0; wb<64; wb++) if(wk!=wb)
	for(wn=0; wn<64; wn++) if(wk!=wn && wb!=wn)
		count += factor[wk]
			* bitcount(~wtm[map[wk]][wb][wn][0]
					& mask[1][wk] & mask[1][wb] & mask[1][wn]);

	printf("no_mate %8u (%1.0f)\n", count, TIME);
	total += count;


	/* print total */

	printf("  total %8u (%1.0f)\n", total, TIME);


	/* be happy */

	return 0;
}

