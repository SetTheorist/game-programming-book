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
typedef uint64	hasht;

typedef uint32 bb;

typedef enum piece_type {
	pawn, gold, silver, bishop, rook, king,
	tokin, promoted_silver, horse, dragon,
	n_piece_type
} piece_type;
typedef enum color_type {
	black, white, n_color_type
} color_type;
char piece_name[] = {'P','G','S','B','R','K','T','N','H','D'};

piece_type	promotes[10] = {};
piece_type	unpromotes[10] = {};

typedef enum flags {
	f_promote=1,
} flags;
typedef struct move {
	uint8 from;
	uint8 to;
	uint8	piece;
	uint8 flags;
} move;

typedef struct board {
	bb p[n_color_type][n_piece_type];
	int hand[n_color_type][n_piece_type];
	hasht	h;
	color_type side;
	int ply;
} board;

/* move arrays */
bb	board_mask = 0x01FFFFFF;
bb	row_mask[5] = {0x01F00000, 0x000F8000, 0x00007C00, 0x000003E0, 0x0000001F};
bb	col_mask[5] = {0x00108421, 0x00210842, 0x00421084, 0x00842108, 0x01084210};
bb	promotion[2] = {0x01F00000, 0x0000001F};
bb	pawn_drop[2] = {0x000FFFFF, 0x01FFFFE0};


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
	return 0;
}

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






