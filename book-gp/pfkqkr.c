
/*
 *	pfkqkr.c (pretty fast kqkr generator)
 *
 *	written by Marcel van Kervinck
 *	<marcelk@stack.urc.tue.nl>
 *
 *	March 1996
 */

/*
 *	platform			cputime (seconds)
 *
 *	68000-7MHz Amiga		7534
 *	486-66MHz FreeBSD
 *	Sun Sparc SLC
 *	Pentium-90MHz FreeBSD
 *	SG Challenge IRIX
 */


/* include the usual stuff */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>


/* some 64 bit definitions for gcc, change these for other compilers */

typedef unsigned long long bitmap;
#define I  0x0000000000000001ULL
#define W  0x7f7f7f7f7f7f7f7fULL
#define E  0xfefefefefefefefeULL
#define V0 0xf0f0f0f0f0f0f0f0ULL
#define V1 0x0f0f0f0f0f0f0f0fULL
#define V2 0xccccccccccccccccULL
#define V3 0x3333333333333333ULL
#define V4 0xaaaaaaaaaaaaaaaaULL
#define V5 0x5555555555555555ULL
#define H2 0x0000ffff0000ffffULL
#define H3 0xffff0000ffff0000ULL
#define H4 0x00ff00ff00ff00ffULL
#define H5 0xff00ff00ff00ff00ULL
#define D0 0xf0f0f0f00f0f0f0fULL
#define D1 0x00000000f0f0f0f0ULL
#define D2 0x0f0f0f0f00000000ULL
#define D3 0xcccc3333cccc3333ULL
#define D4 0x0000cccc0000ccccULL
#define D5 0x3333000033330000ULL
#define D6 0xaa55aa55aa55aa55ULL
#define D7 0x00aa00aa00aa00aaULL
#define D8 0x5500550055005500ULL


/* bit tricks */

#define MIRV(b)	b = (b<<4  & V0) | (b>>4  & V1); \
		b = (b<<2  & V2) | (b>>2  & V3); \
		b = (b<<1  & V4) | (b>>1  & V5);
#define MIRH(b)	b = (b>>32     ) | (b<<32     ); \
		b = (b>>16 & H2) | (b<<16 & H3); \
		b = (b>>8  & H4) | (b<<8  & H5);
#define MIRD(b)	b = (b & D0) | (b>>28 & D1) | (b<<28 & D2); \
		b = (b & D3) | (b>>14 & D4) | (b<<14 & D5); \
		b = (b & D6) | (b>>7  & D7) | (b<<7  & D8);


/* general macros */

#define TIME difftime(time(NULL),starttime)


/* chess macros */

#define STEP_N(b) ((b)<<8)
#define STEP_S(b) ((b)>>8)
#define STEP_W(b) ((b)<<1 & E)
#define STEP_E(b) ((b)>>1 & W)

#define KING(b) \
	( STEP_N(STEP_W(b) | STEP_E(b) | (b)) \
	| STEP_W(b) | STEP_E(b) \
	| STEP_S(STEP_W(b) | STEP_E(b) | (b)) )


/* memory structures */

bitmap wtm[10][64][64][2];	/* wtm database, 2 bits per position */
        /* wk  wq  br */	/* 0: score>=mate    1: score<mate   */
				/* 2: score=mate     3: score=mate-1 */
				/* kqk entries are coded as kqkr with wk==br */

unsigned kqkr, kqk;		/* position counts */
bitmap mask[2][64];		/* bitnumber to bitmap */
bitmap ray[64][8];		/* attack rays */
short slide[64][8][8];		/* sliding moves */
short king[64][9];		/* king moves */

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


void init(void)			/* setup move tables and others */
{
	bitmap b;
	short sq, x, y, i, j, *m;
	short dx[8] = { 0, 0, 1,-1, 1, 1,-1,-1};
	short dy[8] = { 1,-1, 0, 0, 1,-1, 1,-1};

	for(sq=0; sq<64; sq++) {		/* useful masks */
		mask[0][sq] = I<<sq;
		mask[1][sq] = ~(I<<sq);
	}

	for(sq=0; sq<64; sq++) {

		/* precompute sliding moves and attack rays */

		for(i=0; i<8; i++) {
			x = sq % 8;
			y = sq / 8;
			m = slide[sq][i];
			b = 0;
			for(j=0; j<7; j++) {
				x += dx[i];
				y += dy[i];
				if(0<=x && x<8 && 0<=y && y<8) {
					*m++ = y*8+x;
					b |= mask[0][y*8+x];
				}
			}
			*m = -1;
			ray[sq][i] = b;
		}

		/* precompute king moves */

		for(i=0, m=king[sq]; i<64; i++)
		if(i!=sq && abs((i%8)-(sq%8))<=1 && abs((i/8)-(sq/8))<=1)
			*m++ = i;
		*m = -1;
	}
}

void illegal(void)		/* mark relevant illegal positions as lost */
{
	short wk, wq, br, i;
	bitmap b, c;
	unsigned count;

	kqkr = kqk = 0;
	for(wk=0; wk<64; wk++) if(!mir[wk])
	for(wq=0; wq<64; wq++)
	for(br=0; br<64; br++) {

		b = KING(mask[0][wk]);
		for(i=0; i<8; i++) {
			c = ray[wq][i];
			if(c & mask[0][wk])
				c &= ~ray[wk][i];
			if(c & mask[0][br])
				c &= ~ray[br][i];
			b |= c;
		}
		b |= mask[0][br];
		b &= mask[1][wk]; /* allows easy mate detection */

		wtm[map[wk]][wq][br][0] = b;
		wtm[map[wk]][wq][br][1] = 0;

		count = factor[wk]
			* (wk==wq || wq==br ? 64
				: bitcount(b | mask[0][wk] | mask[0][wq]));
		if(wk!=br)
			kqkr += count;
		else
			kqk += count;
	}
}


void new2old(void)	/* mark new found positions as old */
{
	short wk, wq, br;
	unsigned count;
	bitmap b;

	kqkr = kqk = 0;
	for(wk=0; wk<64; wk++) if(!mir[wk])
	for(wq=0; wq<64; wq++)
	for(br=0; br<64; br++) {
		b = ~wtm[map[wk]][wq][br][0] & wtm[map[wk]][wq][br][1];
		wtm[map[wk]][wq][br][0] |= b;
		wtm[map[wk]][wq][br][1] &= b;
		count = factor[wk] * bitcount(b);
		if(wk!=br)
			kqkr += count;
		else
			kqk += count;
	}
}


void enter(short wk, short wq, short br, bitmap b)	/* add wtm positions */
{
	short m;

	if(b) {
		m = mir[wk];
		if(m & 1)	 {	/* mirror vertical */
			wk ^= 007;
			wq ^= 007;
			br ^= 007;
			MIRV(b);
		}
		if(m & 2) {		/* mirror horizontal */
			wk ^= 070;
			wq ^= 070;
			br ^= 070;
			MIRH(b);
		}
		if(m & 4) {		/* mirror diagonal */
			wk = dia[wk];
			wq = dia[wq];
			br = dia[br];
			MIRD(b);
		}

		if( (b &= ~wtm[map[wk]][wq][br][0]
			& ~wtm[map[wk]][wq][br][1]) ) {	/* which are new? */

			wtm[map[wk]][wq][br][1] |= b;	/* enter those */

			if(dia[wk]==wk) {		/* and the doubles */
				wq = dia[wq];
				br = dia[br];
				MIRD(b);
				wtm[map[wk]][wq][br][1] |= b;
			}
		}
	}
}


void retro_btm(short wk, short wq, short br, bitmap b)	/* process btm positions */
{
	short i, *m;

	/* find all white moves that lead to the btm positions */


		/* white queen */

	for(i=0; i<8; i++)
	for(m=slide[wq][i]; *m>=0 && *m!=wk && *m!=br; m++) {
		enter(wk,*m,br,b & mask[1][*m]);
		if(wk==br)
			enter(wk,*m,wq,b & mask[1][*m]);
	}

		/* white king */

	for(m=king[wk]; *m>=0; m++)
	if(*m!=wq && *m!=br) {
		enter(*m,wq,br,b & mask[1][*m]);
		if(wk==br)
			enter(*m,wq,*m,b & mask[1][*m]);
	}
}


void filter(short wk, short wq, short br, bitmap b) /* remove non-forced btm positions */
{
	bitmap c;
	short i, *m;

	b &= ~KING(~wtm[map[wk]][wq][br][0]);

	if(wk!=br)
	for(i=0; i<4; i++) {
		m = slide[br][i];
		c = 0;
		while(*m>=0 && *m!=wk && *m!=wq) {
			c |= mask[0][*m];
			b &= wtm[map[wk]][wq][*m][0] | c;
			if(!b) return;
			m++;
		}
		if(*m>=0) /* can capture king or queen */
			b &= c;
	}
	if(b)
		retro_btm(wk,wq,br,b);
}


void retro_wtm(short wk, short wq, short br, bitmap b)	/* process wtm positions */
{
	short i, *m;
	bitmap c;

	/* find black moves that lead to the wtm positions */


		/* black rook */

	if(wk!=br)
	for(i=0; i<4; i++) {
		m = slide[br][i];
		c = b;
		while(*m>=0 && *m!=wk && *m!=wq && c) {
			filter(wk,wq,*m,c);
			c &= mask[1][*m];
			m++;
		}
	}


		/* black king */

	if( (c = KING(b) & mask[1][wq] & mask[1][br]) )
		filter(wk,wq,br,c);

}


int main(void)					/* compute database */
{
	short wk, wq, br, mate;
	bitmap b;
	unsigned count, total;
	time_t starttime;


	/* init */

	starttime = time(NULL);

	printf("status      kqkr    kqk (time)\n");

	init();
	illegal();

	printf("illegal %8u %6u (%1.0f)\n", kqkr, kqk, TIME);
	total = kqkr + kqk;

	/* enter mates */

	mate = 1;

	for(wk=0; wk<64; wk++) if(!mir[wk])
	for(wq=0; wq<64; wq++) if(wq!=wk)
	for(br=0; br<64; br++) if(br!=wq)
		if( (b = wtm[map[wk]][wq][br][0]) )
			filter(wk,wq,br,b);


	/* iterate */

	new2old();

	while(kqkr+kqk) {

		printf("mate%03d %8u %6u (%1.0f)\n", mate, kqkr, kqk, TIME);
		total += kqkr + kqk;

		mate++;

		for(wk=0; wk<64; wk++) if(!mir[wk])
		for(wq=0; wq<64; wq++)
		for(br=0; br<64; br++)
			if( (b = wtm[map[wk]][wq][br][0] & wtm[map[wk]][wq][br][1]) )
				retro_wtm(wk,wq,br,b);

		new2old();
	}


	/* count left overs */

	count = kqkr = kqk = 0;
	for(wk=0; wk<64; wk++) if(!mir[wk])
	for(wq=0; wq<64; wq++) if(wq!=wk)
	for(br=0; br<64; br++) if(br!=wq) {
		count = factor[wk]
			* bitcount(~wtm[map[wk]][wq][br][0] & mask[1][wk] & mask[1][wq]);
		if(wk!=br)
			kqkr += count;
		else
			kqk += count;
	}

	printf("no_mate %8u %6u (%1.0f)\n", kqkr, kqk, TIME);
	total += kqkr+kqk;

	printf("\ntotal %8u\n", total);


	/* be happy */

	return 0;
}

