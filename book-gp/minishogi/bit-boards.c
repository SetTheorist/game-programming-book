#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 5x5 "mini-shogi" with drops and promotion
 * using bit-board representations
 *
 */

typedef unsigned char	uint8;
typedef unsigned short	uint16;
typedef unsigned int	uint32;
typedef unsigned long long	uint64;
typedef uint32	hasht;

typedef uint32 bb;

typedef enum piece_type {
	pawn, gold, silver, bishop, rook, king,
	tokin, promoted_silver, horse, dragon
} piece_type;
typedef enum color_type {
	black, white
} color_type;
char piece_name[] = {'P','G','S','B','R','K','T','N','H','D'};

piece_type	promotes[10] = {};
piece_type	unpromotes[10] = {};

typedef struct board {
	bb piece[10];
	bb color[2];
	color_type	side;
	color_type	xside;
	piece_type	hand[2][10];
	int	ply;
	uint64	hash1;
	uint64	hash2;
} board;

/* derived bit-boards */
#define w_pawn(b) ((b)->piece[pawn] & (b)->color[white])
#define w_gold(b) ((b)->piece[gold] & (b)->color[white])
#define w_silver(b) ((b)->piece[silver] & (b)->color[white])
#define w_bishop(b) ((b)->piece[bishop] & (b)->color[white])
#define w_rook(b) ((b)->piece[rook] & (b)->color[white])
#define w_king(b) ((b)->piece[king] & (b)->color[white])
#define w_tokin(b) ((b)->piece[tokin] & (b)->color[white])
#define w_promoted_silver(b) ((b)->piece[promoted_silver] & (b)->color[white])
#define w_horse(b) ((b)->piece[horse] & (b)->color[white])
#define w_dragon(b) ((b)->piece[dragon] & (b)->color[white])
#define b_pawn(b) ((b)->piece[pawn] & (b)->color[black])
#define b_gold(b) ((b)->piece[gold] & (b)->color[black])
#define b_silver(b) ((b)->piece[silver] & (b)->color[black])
#define b_bishop(b) ((b)->piece[bishop] & (b)->color[black])
#define b_rook(b) ((b)->piece[rook] & (b)->color[black])
#define b_king(b) ((b)->piece[king] & (b)->color[black])
#define b_tokin(b) ((b)->piece[tokin] & (b)->color[black])
#define b_promoted_silver(b) ((b)->piece[promoted_silver] & (b)->color[black])
#define b_horse(b) ((b)->piece[horse] & (b)->color[black])
#define b_dragon(b) ((b)->piece[dragon] & (b)->color[black])


/* move arrays */
bb	board_mask = 0x01FFFFFF;
bb	row_mask[5] = {0x01F00000, 0x000F8000, 0x00007C00, 0x000003E0, 0x0000001F};
bb	col_mask[5] = {0x00108421, 0x00210842, 0x00421084, 0x00842108, 0x01084210};
bb	promotion[2] = {0x01F00000, 0x0000001F};
bb	pawn_drop[2] = {0x000FFFFF, 0x01FFFFE0};
bb	move_step[2][10][25];
bb	move_slide[2][10][25];


static const bb init_piece[10] = {
	0x00080020, /*pawn*/
	0x00800002, /*gold*/
	0x00400004, /*silver*/
	0x00200008, /*bishop*/
	0x00100010, /*rook*/
	0x01000001, /*king*/
	0x00000000, /*tokin*/
	0x00000000, /*promoted_silver*/
	0x00000000, /*horse*/
	0x00000000  /*dragon*/
};
static const bb init_color[2] = {
	0x0000003F, /*black*/
	0x01F80000  /*white*/
};

int
init_moves()
{
	int	fr, fc;

	/* simplistic and slow */
	for (fr=0; fr<5; ++fr) {
		for (fc=0; fc<5; ++fc) {
			int frc = fr*5 + fc;
			bb	bt;

			/* pawn */
			if (fr+1<5) move_step[black][pawn][frc] = (bb)1<<((fr+1)*5 + fc);
			if (fr-1>=0) move_step[white][pawn][frc] = (bb)1<<((fr-1)*5 + fc);
			move_slide[black][pawn][frc] = (bb)0x00000000;
			move_slide[white][pawn][frc] = (bb)0x00000000;

			/* king */
			bt = (bb)0x00000000;
			if  (fr+1<5 )             bt |= (bb)1<<((fr+1)*5+(fc  ));
			if ((fr+1<5 )&&(fc+1<5 )) bt |= (bb)1<<((fr+1)*5+(fc+1));
			if             (fc+1<5 )  bt |= (bb)1<<((fr  )*5+(fc+1));
			if ((fr-1>=0)&&(fc+1<5 )) bt |= (bb)1<<((fr-1)*5+(fc+1));
			if  (fr-1>=0)             bt |= (bb)1<<((fr-1)*5+(fc  ));
			if ((fr-1>=0)&&(fc-1>=0)) bt |= (bb)1<<((fr-1)*5+(fc-1));
			if             (fc-1>=0)  bt |= (bb)1<<((fr  )*5+(fc-1));
			if ((fr+1<5 )&&(fc-1>=0)) bt |= (bb)1<<((fr+1)*5+(fc-1));
			move_step[black][king][frc] = bt;
			move_step[white][king][frc] = bt;
			move_slide[black][king][frc] = (bb)0x00000000;
			move_slide[white][king][frc] = (bb)0x00000000;

			/* gold */
			/* promoted silver */
			/* tokin */
			bt = (bb)0x00000000;
			if  (fr+1<5 )             bt |= (bb)1<<((fr+1)*5+(fc  ));
			if ((fr+1<5 )&&(fc+1<5 )) bt |= (bb)1<<((fr+1)*5+(fc+1));
			if             (fc+1<5 )  bt |= (bb)1<<((fr  )*5+(fc+1));
			if  (fr-1>=0)             bt |= (bb)1<<((fr-1)*5+(fc  ));
			if             (fc-1>=0)  bt |= (bb)1<<((fr  )*5+(fc-1));
			if ((fr+1<5 )&&(fc-1>=0)) bt |= (bb)1<<((fr+1)*5+(fc-1));
			move_step[black][gold][frc] = bt;
			move_step[black][promoted_silver][frc] = bt;
			move_step[black][tokin][frc] = bt;
			bt = (bb)0x00000000;
			if  (fr+1<5 )             bt |= (bb)1<<((fr+1)*5+(fc  ));
			if             (fc+1<5 )  bt |= (bb)1<<((fr  )*5+(fc+1));
			if ((fr-1>=0)&&(fc+1<5 )) bt |= (bb)1<<((fr-1)*5+(fc+1));
			if  (fr-1>=0)             bt |= (bb)1<<((fr-1)*5+(fc  ));
			if ((fr-1>=0)&&(fc-1>=0)) bt |= (bb)1<<((fr-1)*5+(fc-1));
			if             (fc-1>=0)  bt |= (bb)1<<((fr  )*5+(fc-1));
			move_step[white][gold][frc] = bt;
			move_step[white][promoted_silver][frc] = bt;
			move_step[white][tokin][frc] = bt;

			move_slide[black][gold][frc] = (bb)0x00000000;
			move_slide[white][gold][frc] = (bb)0x00000000;
			move_slide[black][promoted_silver][frc] = (bb)0x00000000;
			move_slide[white][promoted_silver][frc] = (bb)0x00000000;
			move_slide[black][tokin][frc] = (bb)0x00000000;
			move_slide[white][tokin][frc] = (bb)0x00000000;

			/* silver */
			bt = (bb)0x00000000;
			if  (fr+1<5 )             bt |= (bb)1<<((fr+1)*5+(fc  ));
			if ((fr+1<5 )&&(fc+1<5 )) bt |= (bb)1<<((fr+1)*5+(fc+1));
			if ((fr-1>=0)&&(fc+1<5 )) bt |= (bb)1<<((fr-1)*5+(fc+1));
			if ((fr-1>=0)&&(fc-1>=0)) bt |= (bb)1<<((fr-1)*5+(fc-1));
			if ((fr+1<5 )&&(fc-1>=0)) bt |= (bb)1<<((fr+1)*5+(fc-1));
			move_step[black][silver][frc] = bt;
			bt = (bb)0x00000000;
			if ((fr+1<5 )&&(fc+1<5 )) bt |= (bb)1<<((fr+1)*5+(fc+1));
			if ((fr-1>=0)&&(fc+1<5 )) bt |= (bb)1<<((fr-1)*5+(fc+1));
			if  (fr-1>=0)             bt |= (bb)1<<((fr-1)*5+(fc  ));
			if ((fr-1>=0)&&(fc-1>=0)) bt |= (bb)1<<((fr-1)*5+(fc-1));
			if ((fr+1<5 )&&(fc-1>=0)) bt |= (bb)1<<((fr+1)*5+(fc-1));
			move_step[white][king][frc] = bt;
			move_slide[black][silver][frc] = (bb)0x00000000;
			move_slide[white][silver][frc] = (bb)0x00000000;

			/* rook */
			move_step[black][rook][frc] = (bb)0x00000000;
			move_step[white][rook][frc] = (bb)0x00000000;

			/* bishop */
			move_step[black][bishop][frc] = (bb)0x00000000;
			move_step[white][bishop][frc] = (bb)0x00000000;

			/* dragon */
			bt = (bb)0x00000000;
			if ((fr+1<5 )&&(fc+1<5 )) bt |= (bb)1<<((fr+1)*5+(fc+1));
			if ((fr-1>=0)&&(fc+1<5 )) bt |= (bb)1<<((fr-1)*5+(fc+1));
			if ((fr-1>=0)&&(fc-1>=0)) bt |= (bb)1<<((fr-1)*5+(fc-1));
			if ((fr+1<5 )&&(fc-1>=0)) bt |= (bb)1<<((fr+1)*5+(fc-1));
			move_step[black][dragon][frc] = bt;
			move_step[white][dragon][frc] = bt;
			move_slide[black][dragon][frc] = move_slide[black][rook][frc];
			move_slide[white][dragon][frc] = move_slide[white][rook][frc];

			/* horse */
			bt = (bb)0x00000000;
			if  (fr+1<5 )             bt |= (bb)1<<((fr+1)*5+(fc  ));
			if             (fc+1<5 )  bt |= (bb)1<<((fr  )*5+(fc+1));
			if  (fr-1>=0)             bt |= (bb)1<<((fr-1)*5+(fc  ));
			if             (fc-1>=0)  bt |= (bb)1<<((fr  )*5+(fc-1));
			move_step[black][horse][frc] = bt;
			move_step[white][horse][frc] = bt;
			move_slide[black][horse][frc] = move_slide[black][bishop][frc];
			move_slide[white][horse][frc] = move_slide[white][bishop][frc];
		}
	}

	return 0;
}

hasht 	hash_piece[2][10][25];
hasht	hash_white;

int
init_hash()
{
	return 0;
}

int
hash_board(board* b)
{
	return 0;
}

int
init_board(board* b)
{
	memcpy(b->piece, init_piece, sizeof(init_piece));
	memcpy(b->color, init_color, sizeof(init_color));
	b->side = black;
	b->xside = white;
	b->ply = 1;
	hash_board(b);
	return 0;
}

int
show_bitboard(FILE* f, bb b)
{
	int	r, c, bit;
	for (r=4; r>=0; --r) {
		for (c=0; c<5; ++c) {
			bit = 1<<(r*5+c);
			fprintf(f, "%c", b&bit ? '1' : '.');
		}
		fprintf(f, "\n");
	}
	return 0;
}

int
show_board(FILE* f, board* b)
{
	int	r, c;
	bb	bit;

	fprintf(f, "   5  4  3  2  1  \n");
	fprintf(f, " +---------------+    [%08X %08X]\n", b->hash1, b->hash2);
	for (r=4; r>=0; --r) {
		fprintf(f, " |");
		for (c=0; c<5; ++c) {
			int	piece;
			bit = 1<<(r*5+c);
			for (piece=0; piece<10; ++piece)
				if (b->piece[piece] & bit)
					break;
			if (piece<10) {
				if (b->color[black] & bit)
					fprintf(f, " %c ", piece_name[piece]);
				else
					fprintf(f, " %c ", piece_name[piece]+'a'-'A');
			} else {
				fprintf(f, " . ");
			}
		}
		fprintf(f, "| (%i)\n", 5-r);
	}
	fprintf(f, " +---------------+\n");
	if (b->side==black)
		fprintf(f, " Black to move\n");
	else
		fprintf(f, " White to move\n");

	return 0;
}


int
main(int argc, char* argv[])
{
	board b;

	init_moves();

	init_board(&b);
	show_board(stdout, &b);

	printf("\n");
	show_bitboard(stdout, move_step[black][king][2*5+3]);
	printf("\n");
	show_bitboard(stdout, move_step[black][silver][2*5+2]);
	printf("\n");
	show_bitboard(stdout, move_step[white][gold][1*5+0]);
	printf("\n");
	
	return 0;
}






