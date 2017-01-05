#ifndef INCLUDED_BOARD_H_
#define INCLUDED_BOARD_H_
/* $Id: board.h,v 1.1 2010/12/21 16:30:32 ahogan Exp ahogan $ */
#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

#include <stdio.h>
#include "util.h"

/* **************************************** */

#define MAXBOARDI (144)
#define MAXNCAPMOVES (256)
#define MAXNMOVES (512)

/* **************************************** */

/*
 *
 *          Normal move
 * LSB                          MSB
 * 0         1         2         3
 * 01234567890123456789012345678901
 * [ from ][  to  ][pc ]p[id]scpq01
 *        ispromoted----^    |||||+-valid=1
 *                    side---+|||+--special=0
 *                    capture-+|+---can_promote
 *                    promote--+
 *
 *          Special move - null-move
 * LSB                          MSB
 * 0         1         2         3
 * 01234567890123456789012345678901
 * 0000000000000000000000000[]00011
 *                          | **||+-valid
 *                    side--+   |+--is_special=1
 *                              +---movetype=0
 *
 *          Special move - lion-move
 * LSB                          MSB
 * 0         1         2         3
 * 01234567890123456789012345678901
 * [ from ][1][2]       [id][]cc111
 *          |  |            | ||||+-valid=1
 *     to1--+  |     side---+ |||+--is_special=1
 *     to2-----+     capture1-+|+---movetype=1
 *                   capture2--+
 *
 *
 */
typedef union movet {
  ui32 i;
  struct {
    unsigned from:8;
    unsigned to:8;
    unsigned piece: 5;
    unsigned promoted_piece:1;
    unsigned idx: 4;
    unsigned side_is_white:1;
    unsigned capture:1;
    unsigned promote:1;
    unsigned can_promote:1;
    unsigned is_special:1;  /* = 0 */
    unsigned valid:1;       /* = 1 */
  };
  struct {
    unsigned from:8;
    unsigned to1:4;
    unsigned to2:4;
    unsigned piece: 5;
    unsigned promoted_piece:1;
    unsigned idx: 4;
    unsigned side_is_white:1;
    unsigned capture1:1;
    unsigned capture2:1;
    unsigned movetype:1;    /* null=0, lion-type=1 */
    unsigned is_special:1;  /* = 1 */
    unsigned valid:1;       /* = 1 */
  } special;
} movet;

#define MOVE_FROM_I(m_) ((m_).from)
#define MOVE_TO_I(m_) ((m_).to)
#define MOVE_IS_VALID(m_) ((m_).valid)
#define MOVE_IS_NULL(m_) ((m_).is_special && !(m_).special.movetype)
#define MOVE_IS_LION(m_) ((m_).is_special &&  (m_).special.movetype)

typedef enum piece {
  empty=0, pawn, go_between, reverse_chariot, lance,
  king, free_king, lion, dragon_king, dragon_horse, rook, bishop, kirin,
  phoenix, drunk_elephant, blind_tiger, ferocious_leopard,
  gold_general, silver_general, copper_general, vertical_mover, side_mover,
  num_piece,
  promoted=32, piece_mask=31
} piece;

#define PROMOTION_MASK (\
  (1<<bishop)| \
  (1<<blind_tiger)| \
  (1<<copper_general)| \
  (1<<dragon_horse)| \
  (1<<dragon_king)| \
  (1<<drunk_elephant)| \
  (1<<ferocious_leopard)| \
  (1<<go_between)| \
  (1<<gold_general)| \
  (1<<kirin)| \
  (1<<lance)| \
  (1<<pawn)| \
  (1<<phoenix)| \
  (1<<reverse_chariot)| \
  (1<<rook)| \
  (1<<side_mover)| \
  (1<<silver_general)| \
  (1<<vertical_mover) )

typedef enum color {
  none=0, white=1, black=2,
  num_color
} color;

typedef enum state {
  playing=0, white_win, black_win, draw
} state;

typedef enum reason {
  no_reason=0, by_mate, by_repetition, by_flagged, by_length
} reason;

typedef union square {
  ui32  i;
  struct {
    unsigned c: 2;
    unsigned piece: 5;
    unsigned promoted: 1;
    unsigned idx: 4;
    unsigned raw_idx: 10;
  };
} square;

typedef enum promotion_modes {
  normal_pm=0, promote_pm=1, capture_pm=2, never_pm=3
} promotion_modes;

typedef struct piece_info {
  unsigned tempi: 9;
  unsigned loc: 8;
  unsigned valid: 1;
  unsigned active: 1;
  unsigned captured: 1;
  unsigned promoted: 1;
  unsigned cannot_promote_unless_capture: 1;
  /* workspace for evaluation */
  unsigned mobility: 6;
  unsigned max_mobility: 4;
} piece_info;

typedef struct board_flags {
  unsigned no_lion_recapture: 1;
  unsigned last_mover_had_no_promote_unless_capture: 1;
} board_flags;

/* max 12 of each kind of piece */
typedef struct board {
  hasht       h;
  int         ply;
  square      b[12*12];
  union {
    piece_info  pl[num_color][num_piece][13]; /* extra element as invalid goalpost element */
    piece_info  rawpl[num_color * num_piece * 13];
  };
  ui32        king_mask[num_color];
  ui32        prince_mask[num_color];
  color       to_move;
  state       play;
  reason      why;
  board_flags flags;

#ifdef  INCREMENTAL_MATERIAL
  int         material;
  int         material_square[3];
  int         material_stack[1024];
  int         material_square_stack[1024][3];
  struct evaluator*  e;
#endif

  movet       move_stack[1024];
  board_flags flags_stack[1024];
  hasht       hash_stack[1024];

  int         num_caps;
  square      cap_stack[2*num_piece*12+1];
} board;


/* **************************************** */

int gen_moves(const board* b, movet* ml);
int gen_moves_cap(const board* b, movet* ml);
movet null_move();

int init_hash();
//hasht board_hash(const board* b);
#define board_hash(b_) ((b_)->h)

int init_board(board* b);
int terminal(const board* b);
int draw_by_repetition(const board* b);

typedef enum check_status {
  no_check=0, king_check=1, lion_check=2, kinglion_check=3
} check_status;
check_status in_check(const board* b, color c);
check_status in_check_lion(const board* b, color c);

int make_move(board* b, movet m);
int unmake_move(board* b);
//movet last_move(const board* b);
#define last_move(b_) ((b_)->move_stack[(b_)->ply-1])

int board_to_fen(const board* b, char* chp);
int fen_to_board(board* b, const char* chp);
int board_to_sgf(const board* b, char* chp);
int sgf_to_board(board* b, const char* chp);

int move_to_winboard(movet m, char* buff1, char* buff2);
movet winboard_to_move(const board* b, const char* movestring);
int wbfen_to_board(board* b, const char* chp);
int board_to_wbfen(board* b, const char* chp);

int show_move(FILE* f, movet m);
int show_move_eng(FILE* f, movet m);
int show_move_kanji(FILE* f, movet m);
int show_board(FILE* f, const board* b);

int validate_board(const board* b);

extern const char* piece_names[num_piece];
extern const char* piece_kanji[2][num_piece][2];
extern const char* other_king_kanji[2];
extern const char* piece_kanji1[2][num_piece];

/* **************************************** */
/* **************************************** */

static const int dir_map[9]    = { 0, +11, +12, +13, +1, -11, -12, -13, -1 };
static const int dir_map_dx[9] = { 0,  -1,   0,  +1, +1,  +1,   0,  -1, -1 };
static const int dir_map_dy[9] = { 0,  +1,  +1,  +1,  0,  -1,  -1,  -1,  0 };

static const unsigned int square_neighbor_map[144] = {
  0x0E0E0E, 0x8F0E8F, 0x8F8F8F, 0x8F8F8F, 0x8F8F8F, 0x8F8F8F, 0x8F8F8F, 0x8F8F8F, 0x8F8F8F, 0x8F8F8F, 0x8F838F, 0x838383,
  0x3E0E3E, 0xFF0EFF, 0xFF8FFF, 0xFF8FFF, 0xFF8FFF, 0xFF8FFF, 0xFF8FFF, 0xFF8FFF, 0xFF8FFF, 0xFF8FFF, 0xFF83FF, 0xE383E3,
  0x3E3E3E, 0xFF3EFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFE3FF, 0xE3E3E3,
  0x3E3E3E, 0xFF3EFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFE3FF, 0xE3E3E3,
  0x3E3E3E, 0xFF3EFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFE3FF, 0xE3E3E3,
  0x3E3E3E, 0xFF3EFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFE3FF, 0xE3E3E3,
  0x3E3E3E, 0xFF3EFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFE3FF, 0xE3E3E3,
  0x3E3E3E, 0xFF3EFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFE3FF, 0xE3E3E3,
  0x3E3E3E, 0xFF3EFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFE3FF, 0xE3E3E3,
  0x3E3E3E, 0xFF3EFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFE3FF, 0xE3E3E3,
  0x3E383E, 0xFF38FF, 0xFFF8FF, 0xFFF8FF, 0xFFF8FF, 0xFFF8FF, 0xFFF8FF, 0xFFF8FF, 0xFFF8FF, 0xFFF8FF, 0xFFE0FF, 0xE3E0E3,
  0x383838, 0xF838F8, 0xF8F8F8, 0xF8F8F8, 0xF8F8F8, 0xF8F8F8, 0xF8F8F8, 0xF8F8F8, 0xF8F8F8, 0xF8F8F8, 0xF8E0F8, 0xE0E0E0,
};

/* **************************************** */

#define NW1  (1<<(0+0))
#define NN1  (1<<(1+0))
#define NE1  (1<<(2+0))
#define EE1  (1<<(3+0))
#define SE1  (1<<(4+0))
#define SS1  (1<<(5+0))
#define SW1  (1<<(6+0))
#define WW1  (1<<(7+0))
#define NW2  (1<<(0+8))
#define NN2  (1<<(1+8))
#define NE2  (1<<(2+8))
#define EE2  (1<<(3+8))
#define SE2  (1<<(4+8))
#define SS2  (1<<(5+8))
#define SW2  (1<<(6+8))
#define WW2  (1<<(7+8))
#define NWn  (1<<(0+16))
#define NNn  (1<<(1+16))
#define NEn  (1<<(2+16))
#define EEn  (1<<(3+16))
#define SEn  (1<<(4+16))
#define SSn  (1<<(5+16))
#define SWn  (1<<(6+16))
#define WWn  (1<<(7+16))
static const int piece_move_type[num_color][num_piece][2] = {
  {},
  {
    /*empty*/             {(0), (0)},
    /*pawn*/              {(NN1),                             (NW1|NN1|NE1|EE1|SS1|WW1)},
    /*go_between*/        {(NN1|SS1),                         (0x0000FF&~SS1)},
    /*reverse_chariot*/   {(NNn|SSn),                         (NNn|SEn|SSn|SWn)},
    /*lance*/             {(NNn),                             (NWn|NNn|NEn|SSn)},
    /*king*/              {(0x0000FF),                        (0x0000FF)},
    /*free_king*/         {(0xFF0000),                        (0xFF0000)},
    /*lion*/              {(0x00FFFF),                        (0x00FFFF)},
    /*dragon_king*/       {(NE1|SE1|SW1|NW1|NNn|EEn|SSn|WWn), (NNn|EEn|SEn|SSn|SWn|WWn|NW1|NW2|NE1|NE2)},
    /*dragon_horse*/      {(NEn|SEn|SWn|NWn|NN1|EE1|SS1|WW1), (NEn|EEn|SEn|SSn|SWn|WWn|NWn|NN1|NN2)},
    /*rook*/              {(NNn|EEn|SSn|WWn),                 (NE1|SE1|SW1|NW1|NNn|EEn|SSn|WWn)},
    /*bishop*/            {(NEn|SEn|SWn|NWn),                 (NEn|SEn|SWn|NWn|NN1|EE1|SS1|WW1)},
    /*kirin*/             {(NN2|NE1|EE2|SE1|SS2|SW1|WW2|NW1), (0x00FFFF)},
    /*phoenix*/           {(NN1|NE2|EE1|SE2|SS1|SW2|WW1|NW2), (0xFF0000)},
    /*drunk_elephant*/    {(0x0000FF&~SS1),                   (0x0000FF)},
    /*blind_tiger*/       {(0x0000FF&~NN1),                   (NNn|NE1|EE1|SE1|SSn|SW1|WW1|NW1)},
    /*ferocious_leopard*/ {(NW1|NN1|NE1|SE1|SS1|SW1),         (NEn|SEn|SWn|NWn)},
    /*gold_general*/      {(NW1|NN1|NE1|EE1|SS1|WW1),         (NNn|EEn|SSn|WWn)},
    /*silver_general*/    {(NW1|NN1|NE1|SE1|SW1),             (NNn|SSn|EE1|WW1)},
    /*copper_general*/    {(NW1|NN1|NE1|SS1),                 (NN1|SS1|EEn|WWn)},
    /*vertical_mover*/    {(NNn|SSn|EE1|WW1),                 (NWn|NNn|NEn|SEn|SSn|SWn)},
    /*side_mover*/        {(NN1|SS1|EEn|WWn),                 (NEn|EEn|SEn|SWn|WWn|NWn)},
  },
  {
    /*empty*/             {(0), (0)},
    /*pawn*/              {(SS1),                             (SW1|SS1|SE1|EE1|NN1|WW1)},
    /*go_between*/        {(SS1|NN1),                         (0x0000FF&~NN1)},
    /*reverse_chariot*/   {(SSn|NNn),                         (SSn|NEn|NNn|NWn)},
    /*lance*/             {(SSn),                             (SWn|SSn|SEn|NNn)},
    /*king*/              {(0x0000FF),                        (0x0000FF)},
    /*free_king*/         {(0xFF0000),                        (0xFF0000)},
    /*lion*/              {(0x00FFFF),                        (0x00FFFF)},
    /*dragon_king*/       {(SE1|NE1|NW1|SW1|SSn|EEn|NNn|WWn), (SSn|EEn|NEn|NNn|NWn|WWn|SW1|SW2|SE1|SE2)},
    /*dragon_horse*/      {(SEn|NEn|NWn|SWn|SS1|EE1|NN1|WW1), (SEn|EEn|NEn|NNn|NWn|WWn|SWn|SS1|SS2)},
    /*rook*/              {(SSn|EEn|NNn|WWn),                 (SE1|NE1|NW1|SW1|SSn|EEn|NNn|WWn)},
    /*bishop*/            {(SEn|NEn|NWn|SWn),                 (SEn|NEn|NWn|SWn|SS1|EE1|NN1|WW1)},
    /*kirin*/             {(SS2|SE1|EE2|NE1|NN2|NW1|WW2|SW1), (0x00FFFF)},
    /*phoenix*/           {(SS1|SE2|EE1|NE2|NN1|NW2|WW1|SW2), (0xFF0000)},
    /*drunk_elephant*/    {(0x0000FF&~NN1),                   (0x0000FF)},
    /*blind_tiger*/       {(0x0000FF&~SS1),                   (SSn|SE1|EE1|NE1|NNn|NW1|WW1|SW1)},
    /*ferocious_leopard*/ {(SW1|SS1|SE1|NE1|NN1|NW1),         (SEn|NEn|NWn|SWn)},
    /*gold_general*/      {(SW1|SS1|SE1|EE1|NN1|WW1),         (SSn|EEn|NNn|WWn)},
    /*silver_general*/    {(SW1|SS1|SE1|NE1|NW1),             (SSn|NNn|EE1|WW1)},
    /*copper_general*/    {(SW1|SS1|SE1|NN1),                 (SS1|NN1|EEn|WWn)},
    /*vertical_mover*/    {(SSn|NNn|EE1|WW1),                 (SWn|SSn|SEn|NEn|NNn|NWn)},
    /*side_mover*/        {(SS1|NN1|EEn|WWn),                 (SEn|EEn|NEn|NWn|WWn|SWn)},
  }
};
#undef NW1
#undef NN1
#undef NE1
#undef EE1
#undef SE1
#undef SS1
#undef SW1
#undef WW1
#undef NW2
#undef NN2
#undef NE2
#undef EE2
#undef SE2
#undef SS2
#undef SW2
#undef WW2
#undef NWn
#undef NNn
#undef NEn
#undef EEn
#undef SEn
#undef SSn
#undef SWn
#undef WWn

/* **************************************** */

#ifdef  __cplusplus
};
#endif/*__cplusplus*/
#endif /* INCLUDED_BOARD_H_ */
