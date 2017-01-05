#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum Sides {
	None, XX, OO
} Sides;
typedef int Move;
typedef struct Board {
	int x;
	int o;
	Sides side;
} Board;
int init_board(Board* b) {
	b->x = 0;
	b->o = 0;
	b->side = XX;
	return 0;
}
int gen_moves(Board* b, Move* mp) {
	int i, n=0;
	int f = b->x | b->o;
	for (i=0; i<9; ++i)
		if (!(f & (1<<i)))
			mp[n++] = 1<<i;
	return n;
}
int make_move(Board* b, Move m) {
	if (b->side == XX) {
		b->x |= m;
	} else if (b->side == OO) {
		b->o |= m;
	} else return 1;
	b->side ^= 3;
	return 0;
}
int unmake_move(Board* b, Move m) {
	if (b->side == XX) {
		b->o &= ~m;
	} else if (b->side == OO) {
		b->x &= ~m;
	} else return 1;
	b->side ^= 3;
	return 0;
}
static int trips[8] = { 0007, 0070, 0700, 0111, 0222, 0444, 0124, 0421 };
int terminal(Board* b) {
	int i;
	for (i=0; i<8; ++i)
		if ((b->x & trips[i]) == trips[i])
			return XX;
		else if ((b->o & trips[i]) == trips[i])
			return OO;
	return 0;
}
int show_board(Board* b) {
	printf("%c%c%c\n%c%c%c\n%c%c%c\n",
		(b->o&0001) ? 'O' : (b->x&0001) ? 'X' : '-',
		(b->o&0002) ? 'O' : (b->x&0002) ? 'X' : '-',
		(b->o&0004) ? 'O' : (b->x&0004) ? 'X' : '-',
		(b->o&0010) ? 'O' : (b->x&0010) ? 'X' : '-',
		(b->o&0020) ? 'O' : (b->x&0020) ? 'X' : '-',
		(b->o&0040) ? 'O' : (b->x&0040) ? 'X' : '-',
		(b->o&0100) ? 'O' : (b->x&0100) ? 'X' : '-',
		(b->o&0200) ? 'O' : (b->x&0200) ? 'X' : '-',
		(b->o&0400) ? 'O' : (b->x&0400) ? 'X' : '-');
	printf("%s to move\n", b->side==XX ? "X" : b->side==OO ? "O" : "?");
	return 0;
}

typedef struct PNNode {
  Move pre_move;
  int proof_number;
  int disproof_number;
  struct PNNode* parent;
  int num_children;
	int is_max_node;
  struct PNNode* children;
} PNNode;


void expand_node(PNNode* node) {
}

void update_node(PNNode* node) {
}

PNNode* find_node(PNNode* node) {
	if (node->is_max_node) {
		
	} else {
	}
}

int main(int argc, char* argv[]) {
	Board b;
	Move m[16];
	int n;
	init_board(&b);
	show_board(&b);
	while (!terminal(&b)) {
		n = gen_moves(&b, m);
		printf("%i\n", n);
		make_move(&b, m[13%n]);
		show_board(&b);
	}
	return 0;
}
