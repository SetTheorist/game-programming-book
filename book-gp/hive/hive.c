/* $Id: $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>


/*     |   |   |   |   |
 *    / \ / \ / \ / \ / \
 * A | 1 | 2 | 3 | 4 | 5 |
 *    \ / \ / \ / \ / \ /
 * B   | 1 | 2 | 3 | 4 |
 *    / \ / \ / \ / \ / \
 * C | 1 | 2 | 3 | 4 | 5 |
 *    \ / \ / \ / \ / \ /
 *     |   |   |   |   |
 *
 *       +-+     +-+     +-+     +-+     +-+
 *      /   \   /   \   /   \   /   \   /   \
 * A   +  1  +-+  2  +-+  3  +-+  4  +-+  5  +-+
 *      \   /   \   /   \   /   \   /   \   /
 * B     +-+  1  +-+  2  +-+  3  +-+  4  +-+
 *      /   \   /   \   /   \   /   \   /   \
 * C   +  1  +-+  2  +-+  3  +-+  4  +-+  5  +-+
 *      \   /   \   /   \   /   \   /   \   /
 *       +-+     +-+     +-+     +-+     +-+
 *
 *
 * AABBCCDDEEFF
 *  112233445566
 * aabbccddeeff
 *
 *   + o - x + o -
 *  +++ --- +++ ---
 *   + x - o + x - o
 *    xxx ooo xxx ooo
 *   - x + o -
 *  --- +++ ---
 *   - o + x -
 *
 *
 *  ++oo--xx++--
 * ++++----++++----
 *  ++xx--oo++xx
 *   xxxxooooxxxx
 *  --xx++oo--xx
 * ----++++----
 *  --oo++xx--oo
 */

/* Queen Bee (Yellow-Gold): one-step
 * 2 Spiders (Brown): 3 steps
 * 2 Beetles (Purple): one-step including on-top
 * 3 Grasshoppers (Green): straight-line jump
 * 3 Soldier Ants (Blue): any outside
 */

typedef enum piecet {
	empty, bee, spider, beetle, grasshopper, ant
} piecet;

typedef struct piece piece;
struct piece {
	piecet	type;
	piece*	up;
	piece*	down;
	piece*	side[6];
	int		x, y;
};



int
main(int argc, char* argv[])
{
/* Queen Bee (Yellow-Gold): one-step
 * 2 Spiders (Brown): 3 steps
 * 2 Beetles (Purple): one-step including on-top
 * 3 Grasshoppers (Green): straight-line jump
 * 3 Soldier Ants (Blue): any outside
 */

	printf("[ \033[33m Queen Bee   \033[0m ]\t");
	printf("[ \033[40m\033[33m Queen Bee   \033[0m ]\n");

	printf("[ \033[35m Beetle      \033[0m ]\t");
	printf("[ \033[40m\033[35m Beetle      \033[0m ]\n");

	printf("[ \033[32m Grasshopper \033[0m ]\t");
	printf("[ \033[40m\033[32m Grasshopper \033[0m ]\n");

	printf("[ \033[34m Soldier Ant \033[0m ]\t");
	printf("[ \033[40m\033[34m Soldier Ant \033[0m ]\n");

	printf("[ \033[31m Spider      \033[0m ]\t");
	printf("[ \033[40m\033[31m Spider      \033[0m ]\n");

	printf("\n\n");
	printf("[ \033[30m Red \033[0m ]\n");
	printf("[ \033[31m Red \033[0m ]\n");
	printf("[ \033[32m Green \033[0m ]\n");
	printf("[ \033[33m Yellow \033[0m ]\n");
	printf("[ \033[34m Blue \033[0m ]\n");
	printf("[ \033[35m Foo \033[0m ]\n");
	printf("[ \033[36m Foo \033[0m ]\n");
	printf("[ \033[37m Foo \033[0m ]\n");
	printf("[ \033[38m Foo \033[0m ]\n");
	printf("[ \033[39m Foo \033[0m ]\n");
	printf("[ \033[40m Red \033[0m ]\n");
	printf("[ \033[41m Red \033[0m ]\n");
	printf("[ \033[42m Green \033[0m ]\n");
	printf("[ \033[43m Yellow \033[0m ]\n");
	printf("[ \033[44m Blue \033[0m ]\n");
	printf("[ \033[45m Foo \033[0m ]\n");
	printf("[ \033[46m Foo \033[0m ]\n");
	printf("[ \033[47m Foo \033[0m ]\n");
	printf("[ \033[48m Foo \033[0m ]\n");
	printf("[ \033[49m Foo \033[0m ]\n");
	return 0;
}
