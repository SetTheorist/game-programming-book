


typedef struct move {
	unsigned int from : 5;
	unsigned int to   : 5;
  unsigned int f_capture : 1;
} move;

typedef struct board {
	uint32 white;
	uint32 black;
	int ply;
	move moves[256];
	uint64 hash;
} board;
