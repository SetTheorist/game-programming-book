/* $Id: $ */
#include <ctype.h>
#include <math.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "evaluate.h"
#include "search.h"
#include "util.h"
#include "util-bits.h"

/* **************************************** */

#define MAJOR_PIECE_BONUS

//#define OPENING_PHASE(pli)  (max(0, 100 - (pli)))
#define OPENING_PHASE(pli)  (max(0, 80 - (pli)))
#define MIDGAME_PHASE(pli)  (min(50, (pli)))
//#define ENDGAME_PHASE(pli)  (max(0, (pli) - 150))
#define ENDGAME_PHASE(pli)  (max(0, (pli) - 130))

/* **************************************** */

int compute_board_control_alt(const board* b, color side, int* counts)
{
  memset(counts, '\x00', sizeof(int)*12*12);
  piece ip;
  for (ip=1; ip<num_piece; ++ip)
  {
    int idx;
    for (idx=0; b->pl[side][ip][idx].valid; ++idx)
    {
      register const piece_info current_piece = b->pl[side][ip][idx];
      if (!current_piece.active) continue;
      int piece_mobility = 0;
      int max_mobility = 0;

      const int pro = current_piece.promoted;
      const int loc = current_piece.loc;
      const int x = loc % 12;
      const int y = loc / 12;

      /* special handling */
      if (ip==lion || (ip==kirin && pro))
      {
        const int miny = max( 0, y-2);
        const int maxy = min(11, y+2);
        int y1;
        for (y1=miny; y1<=maxy; ++y1)
        {
          const int minx = max( 0, x-2);
          const int maxx = min(11, x+2);
          int x1;
          for (x1=minx; x1<=maxx; ++x1)
          {
            const int loc1 = 12*y1 + x1;
            ++counts[loc1];
            ++piece_mobility;
          }
        }
        ((board*)b)->pl[side][ip][idx].mobility = piece_mobility;
        ((board*)b)->pl[side][ip][idx].max_mobility = 2;
        continue;
      }

      const int move_type = piece_move_type[side][ip][pro] & square_neighbor_map[loc];
      const int move_type_1 = (move_type & 0x0000FF);
      const int move_type_2 = (move_type & 0x00FF00)>>8;
      const int move_type_n = (move_type & 0xFF0000)>>16;

      /* one-step moves */
      if (move_type_1)
      {
        int d;
        for (d=0; d<8; ++d)
        {
          if (move_type_1 & (1<<d))
          {
            const int step = dir_map[d+1];
            const int loc1 = loc + step;
            ++counts[loc1];
            ++piece_mobility;
          }
        }
        max_mobility = 1;
      }

      /* two-step moves */
      if (move_type_2)
      {
        int d;
        for (d=0; d<8; ++d)
        {
          if (move_type_2 & (1<<d))
          {
            const int step = dir_map[d+1];
            const int loc1 = loc + 2*step;
            ++counts[loc1];
            ++piece_mobility;
          }
        }
        max_mobility = 2;
      }

      /* slide moves */
      if (move_type_n)
      {
        int d;
        for (d=0; d<8; ++d)
        {
          if (move_type_n & (1<<d))
          {
            const int step = dir_map[d+1];
            const int step_dx = dir_map_dx[d+1];
            const int step_dy = dir_map_dy[d+1];
            int maxi = min( (step_dx>0 ? 11-x : step_dx<0 ? x : 11),
                            (step_dy>0 ? 11-y : step_dy<0 ? y : 11) );
            int next_must_be_empty_flag = 0;
            int i;
            for (i=1; i<=maxi; ++i)
            {
              const int loc1 = loc + i*step;
              if (b->b[loc1].c == none)
              {
                counts[loc1] += 1;
                ++piece_mobility;
                max_mobility = max(i, max_mobility);
                next_must_be_empty_flag = 0;
              }
              else if (next_must_be_empty_flag)
              {
                break;
              }
              else if (b->b[loc1].c == (side^3))
              {
                counts[loc1] += 1;
                ++piece_mobility;
                max_mobility = max(i, max_mobility);
                break;
              }
              else if (b->b[loc1].c == side)
              {
                counts[loc1] += 1;
                ++piece_mobility;
                max_mobility = max(i, max_mobility);

                const int pt = b->b[loc1].piece;
                const int pp = b->rawpl[b->b[loc1].raw_idx].promoted;
                const int pm = piece_move_type[side][pt][pp] & square_neighbor_map[loc1];
                if (pm & (1<<(d+16)))
                {
                  /* slide through */
                }
                else if ((pm & (1<<(d+8))) && (pm & (1<<(d+0))))
                {
                  /* 1 & 2 step */
                  maxi = min(maxi,i+2);
                }
                else if (pm & (1<<(d+8)))
                {
                  /* 2 step only */
                  maxi = min(maxi,i+2);
                  next_must_be_empty_flag = 1;
                }
                else if (pm & (1<<(d+0)))
                {
                  /* 1 step only */
                  maxi = min(maxi,i+1);
                }
                else
                {
                  /* no x-ray */
                  break;
                }
              }
            }
          }
        }
      }
      ((board*)b)->pl[side][ip][idx].mobility = piece_mobility;
      ((board*)b)->pl[side][ip][idx].max_mobility = max_mobility;
    }
  }
  return 0;
}

int compute_board_control(const board* b, color side, int* counts)
{
  memset(counts, '\x00', sizeof(int)*12*12);

  /* pre-compute move-map */
  unsigned int move_map[12*12];
  {
    piece ip;
    memcpy(move_map, square_neighbor_map, sizeof(unsigned int)*12*12);
    const color xside = side^3;
    for (ip=1; ip<num_piece; ++ip)
    {
      int idx;
      for (idx=0; b->pl[xside][ip][idx].valid; ++idx)
      {
        register const piece_info current_piece = b->pl[xside][ip][idx];
        if (!current_piece.active) continue;
        move_map[current_piece.loc] = 0;
      }
    }
    for (ip=1; ip<num_piece; ++ip)
    {
      int idx;
      for (idx=0; b->pl[side][ip][idx].valid; ++idx)
      {
        register const piece_info current_piece = b->pl[side][ip][idx];
        if (!current_piece.active) continue;
        const int pro = current_piece.promoted;
        const int loc = current_piece.loc;
        move_map[loc] &= piece_move_type[side][ip][pro];
      }
    }
  }

  piece ip;
  for (ip=1; ip<num_piece; ++ip)
  {
    int idx;
    for (idx=0; b->pl[side][ip][idx].valid; ++idx)
    {
      register const piece_info current_piece = b->pl[side][ip][idx];
      if (!current_piece.active) continue;
      int piece_mobility = 0;
      int max_mobility = 0;

      const int pro = current_piece.promoted;
      const int loc = current_piece.loc;
      const int x = loc % 12;
      const int y = loc / 12;

      /* special handling */
      if (ip==lion || (ip==kirin && pro))
      {
        const int miny = max( 0, y-2);
        const int maxy = min(11, y+2);
        const int minx = max( 0, x-2);
        const int maxx = min(11, x+2);
        int y1;
        for (y1=miny; y1<=maxy; ++y1)
        {
          int x1;
          for (x1=minx; x1<=maxx; ++x1)
          {
            const int loc1 = 12*y1 + x1;
            ++counts[loc1];
          }
        }
        ((board*)b)->pl[side][ip][idx].mobility = (maxy-miny)*(maxx-minx);
        ((board*)b)->pl[side][ip][idx].max_mobility = 2;
        continue;
      }

      const int move_type = move_map[loc];
      const int move_type_1 = (move_type & 0x0000FF);
      const int move_type_2 = (move_type & 0x00FF00)>>8;
      const int move_type_n = (move_type & 0xFF0000)>>16;

      /* one-step moves */
      if (move_type_1)
      {
        int d;
        for (d=0; d<8; ++d)
        {
          if (move_type_1 & (1<<d))
          {
            const int step = dir_map[d+1];
            const int loc1 = loc + step;
            ++counts[loc1];
            ++piece_mobility;
          }
        }
        max_mobility = 1;
      }

      /* two-step moves */
      if (move_type_2)
      {
        int d;
        for (d=0; d<8; ++d)
        {
          if (move_type_2 & (1<<d))
          {
            const int step = dir_map[d+1];
            const int loc1 = loc + 2*step;
            ++counts[loc1];
            ++piece_mobility;
          }
        }
        max_mobility = 2;
      }

      /* slide moves */
      if (move_type_n)
      {
        int d;
        for (d=0; d<8; ++d)
        {
          if (move_type_n & (1<<d))
          {
            const int step = dir_map[d+1];
            const int step_dx = dir_map_dx[d+1];
            const int step_dy = dir_map_dy[d+1];
            int maxi = min( (step_dx>0 ? 11-x : step_dx<0 ? x : 11),
                            (step_dy>0 ? 11-y : step_dy<0 ? y : 11) );
            int next_must_be_empty_flag = 0;
            int i;
            for (i=1; i<=maxi; ++i)
            {
              const int loc1 = loc + i*step;
              const int pm = move_map[loc1];

              if (next_must_be_empty_flag)
                if (!(pm & (0x010001<<d)))
                  break;

              ++counts[loc1];
              ++piece_mobility;
              max_mobility = max(i, max_mobility);
              next_must_be_empty_flag = 0;

              if (pm & (1<<(d+16)))
              {
                /* slide through */
              }
              else if ((pm & (1<<(d+8))) && (pm & (1<<(d+0))))
              {
                /* 1 & 2 step */
                maxi = min(maxi,i+2);
              }
              else if (pm & (1<<(d+8)))
              {
                /* 2 step only */
                maxi = min(maxi,i+2);
                next_must_be_empty_flag = 1;
              }
              else if (pm & (1<<(d+0)))
              {
                /* 1 step only */
                maxi = min(maxi,i+1);
              }
              else
              {
                /* no x-ray */
                break;
              }
            }
          }
        }
      }
      ((board*)b)->pl[side][ip][idx].mobility = piece_mobility;
      ((board*)b)->pl[side][ip][idx].max_mobility = max_mobility;
    }
  }
  return 0;
}

/* ************************************************************ */


/* ************************************************************
   ************************************************************

  Hodges' strategy tips

  *******************
  *** The Opening ***
  *******************

    In the opening phase of a game of Middle Shogi, pieces are crowded together in the initial position and often block
    each other's natural developing squares. Generals can be advanced either to the wings or the centre without too
    much trouble. But, once their direction has been chosen, it will be difficult and time-consuming to re-deploy them,
    since they are slow and have poor lateral movement ability. There is also the constant problem of how best to use
    your Lion to restrict your opponent's central of wing expansions, whilst trying to cope with the effect of your
    opponent's Lion on your own front line. The following principles should be kept in mind during the opening:

  (O1) The Lion belongs in front of your Pawns and in as centralised a location as possible. Often it may sit in the
    centre, seemingly passive for a long time; yet it exerts enormous influence there and restricts the ways in which the
    opponent can develop, expand and begin an attack.

  (O2) Pawns should be advanced cautiously, avoiding squares which cannot be defended from the enemy Lion.
    Sacrificing one or two Pawns to gain time and to open up lines that can be used to harass the enemy Lion is often
    good, but it requires an energetic follow-up; otherwise it may result in a disadvantage, since Pawns are useful in the
    middle game to oppose enemy Pawns and to shield pieces from long-range attack.

  (O3) The front line of Pawns should be supported directly from behind with Copper and Silver Generals and possibly,
    by the two Ferocious Leopards and one Gold.

  (O4) It is recommended that the King be left on its starting square during the opening (moving it so early is overly
    committal and too slow) and that a minimum of one Gold, both Blind Tigers and the Drunk Elephant be kept near
    the King for defensive purposes. This 'castle' may turn out to be indispensable if a semeai (mutual mating attack)
    occurs, in which each side has a supported Lion near each other's King, late in the game. Depending upon how the
    middle game develops, a player may either keep his castle intact into the endgame, or he may decide to move the
    King one or two squares to one side or the other and to advance some or all of the castle's weak pieces to the
    battle front.

  (O5) Long-range pieces should generally be placed on the back ranks, training their sights on lines that are apt to
    become open in the middle game. If they are placed well early on, they should gain greater and greater scope as the
    board opens up and they may not need to move again until the endgame.

  (O6) As an exception to (5) above, dragon Horses are usually best brought out to the wings early on. See Note (e) in
    the Illustrative Opening below.

  (O7) Until the course of the game becomes clear, make moves that are as non-committal as possible. If you know you
    will have to play, for example, C-4b, soon or later, do it sooner. Do not, instead, risk making a move that you may
    later want to retract; for the loss of even a single tempo is serious.

  (O8) Connecting the two Rooks, by opening a line of communication between your two flanks, is a good idea. Usually,
    this means clearing the third rank of pieces, although it is sometimes possible to clear the second rank instead. In
    either case, you will usually find that the most awkward piece to get out of the way is the Kylin.

  (O9) Moving the Phoenix to 8i 9or 5d) is joseki (a standard opening move). Always develop it there.

  (O10) Advancing the edge Pawns and Side Movers on square each creates a good shape on the wings. See note (v) of
    the Illustrative Opening below.

  (O11) The most usual middle game plan for each player will be to attack on his right, which is the side nearer his
    opponent's King, further from his own King and further from his own Phoenix, which serves as an excellent
    defender at its normal post - see (9) above - but is a poor attacker. Keep your plans flexible at all times, but do bear
    in mind where the attack is most likely to come from.

  (O12) It is usually better to develop Coppers and Silvers towards the centre than on the wings. Central squares are
    more important to control; and from the centre, they can still manoeuvre to participate in an attack against a flank.



  ***********************
  *** The Middle Game ***
  ***********************

    A typical Middle Shogi middle game position resembles a battlefield. For about a hundred moves after the opening,
    each player manoeuvres his men behind his lines, advances weak pieces and tries to break into the enemy camp to
    achieve promotions. Because so many pieces have a limited range, most early middle game positions have a
    clearly defined battle front, unlike Western Chess or Shogi. Each player must be careful to watch his front lines for
    weaknesses that can be attacked by the enemy Lion. It is also important to prevent incursions by enemy weak pieces,
    which have the effect of establishing a beachhead for invasion by the lion. Typically, the players will develop strong
    positions on opposite wings, few pieces will be exchanged for a long time and the strategy will focus upon slowing
    up the opponent's advance, even by means of a modest material sacrifice, whilst trying to advance one's own weak
    pieces and Lion as close to the enemy King and promotion zone as possible. Sometimes, however, the centre will
    open up early, many long-range pieces will be exchanged and an early endgame may result. More than in Western
    Chess or in Shogi, becoming a good Middle Shogi middle game player requires a good instinctive grasp of the
    requirements of a position. It is not terribly important to be good tactician, who can read sequences many moves
    deep, because there are simply too many moves to consider. Also, because looking ahead, even 30 to 40 moves, were
    it somehow possible, might not be adequate enough to ascertain whether to play a certain move! To develop an
    instinct for the middle game, playing experience is, of course, very important. But learning the principles set forth
    here should be a helpful shortcut. Most importantly, a player should discipline himself to playing logically, by
    formulating a plan that takes into account the important features of the position. The following principles of middle
    game play have been grouped according to the types of pieces to which they pertain:-

  +++++++++
  + Lions +
  +++++++++

  (M1) Keep your Lion as centralised as possible. Once the centre of the board becomes so open that your Lion can no
    longer avoid frequent harassment there, move it to the side of the board where you intend to concentrate your
    middle game attack. Remember that Lions are much stronger in attack than in defence.

  (M2) Watch for opportunities for both player's Lions to invade and win material, always being careful to determine
    whether a Lion incursion can be repelled without serious loss and also whether a Lion too far advanced may become
    trapped. If a Lion is undefended, watch for ways that the opposing Lion might suddenly attack both it and another
    piece simultaneously.

  (M3) Try to harass your opponent's Lion to drive it to less active squares and to gain time.

  +++++++++++++++++++++++++++++++++++++++++++++++
  + Weak Pieces (Generals, Leopards and Tigers) +
  +++++++++++++++++++++++++++++++++++++++++++++++

  (M4) On the side of the board where you intend to attack, bring up weak pieces to the front lines quite relentlessly,
    re-inforcing them with other weak pieces to create a strong, slowly-advancing phalanx behind your Pawns.

  (M5) On the side of the board where you are defending against an opposing advance,bring up weak pieces to support
    your Pawns on the fifth rank, but do not try to expand your position. Do push Pawns to the fifth rank, however, to
    avoid being too cramped. The greater the distance that your opponent must cross to reach your position, naturally,
    the more tempi he will have to use to bring his pieces within striking distance of your King. Therefore, sit tight, make
    him advance as far as possible and concentrate on your own attack. When the opponent is in a position to advance
    a weak piece into your position, so as to cause a breach in your line, have a comparable weak piece ready to
    exchange with the intruder; but make sure that such exchanges will not allow the enemy Lion to invade. Do
    remember that for every weak piece exchanged in this manner, you will have spent just three or four tempi to move
    your weak piece, while your opponent will have expended seven or eight.

  (M6) Try to avoid retreating weak pieces, since every move back and up again is a loss of two valuable tempi.

  (M7) Since most exchanges gain a tempo for the side that does not initiate them, try to force your opponent to be the
    one who has to make the first capture in a local battle. If a point in your front line comes under attack, try to defend
    by moving up a weak piece to support rather than by exchanging off the threatened piece, unless you can see that
    you will later be forced to exchange off the threatened piece anyway.
    
  (M8) In attacking, try to bring up at least one more weak piece to an area where you wish to break through than your
    opponent has defending that area. To achieve a breakthrough in an area defended by weak pieces, it is necessary to
    exchange your own weak pieces for the defending pieces. Sacrificing long-range pieces in order to break through
    more quickly, is generally a losing strategy, as both breakthroughs and tempi are much less important than they are
    in normal Shogi.

  (M9) This may be the most important principle of attack, yet it can be the hardest one to obey in practice: Refrain from
    making exchanges on the side of the board where you have a spatial advantage until you have brought up as many
    weak pieces as possible as far as possible to support your breakthrough. As long as your opponent's position remains
    cramped, he will have trouble organising a defence; but if you make exchanges too soon, it will only help him relieve
    the cramp. IMPORTANT: if you find, after breaking through, that you cannot make more progress without first
    taking time to bring up another weak piece, then your breakthrough was premature. A premature breakthrough gives
    your opponent the open lines he needs to harass your Lion; space he needs to bring in new defenders and time he
    needs to launch a counter-attack.

  (M10) Blind Tigers are better in defence than attack because they advance so awkwardly. if you have advanced a Blind
    Tiger, be glad to exchange it for another weak piece unless you think you can promote it.

  +++++++++++++++++++++
  + Long-range Pieces +
  +++++++++++++++++++++

  (M11) Keep long-range pieces on the back ranks, where they should be used to support the advance of weak pieces and
    to harass the enemy Lion.

  (M12) Try to maintain an open line of communication between your flanks, so that your Rook-type pieces can switch
    back and forth as needed.

  (M13) Use long-range pieces to seize and control open lines.

  (M14) Try to promote a Vertical Mover. This is a standard early middle game objective.

  (M15) Do not struggle or hasten to promote a Dragon Horse or a Dragon King; it is never urgent to do so.


  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  + Other Pieces (Phoenixes, Kylins, Drunk Elephants and Kings) +
  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  (M16) Phoenixes should not be advanced beyond the fourth rank unless they have a clear path to their promotion.

  (M17) Kylins are more expendable than Phoenixes, hence it is not wrong to bring them into the thick of the battle. In a
    position with an open centre in which an early endgame seems likely, try to preserve your Kylin; for, typically in such
    positions, most long-range pieces are exchanged, with the result that the Kylin's promotion value is enhanced (since
    a Lion's diminishing strength in the endgame is largely due to its increasing susceptibility to harassment from
    long-range pieces).

  (M18) Drunk Elephants should be kept back to help defend the King in most middle game positions. Their potential to
    become a second King will rarely be useful in the endgame.

  (M19) Kings should be kept in the centre as long as possible. When it looks as though a semeai (a mutual mating attack)
    is likely to occur, consider moving your King two or three squares to the side on which it will be safer, provided that
    you are certain that your opponent will be losing more time than you as a result of this manoeuvre.

  ++++++++++++++
  + In general +
  ++++++++++++++

  (M20) Over-protection of vital defensive points is useful, as in Western Chess. If a square that must be held is
    protected by two pieces more than it apparently needs to be, then two of the defending pieces are effectively free to
    move elsewhere. Over-protection also reduces the likelihood of suddenly finding that a piece is over-worked, or that
    the opponent has unexpectedly made a simultaneous attack on two vital squares. Since this is a much more intricate
    game than Western Chess, one cannot expect to be able to anticipate every threat or variation. Mental energy should,
    therefore, be spent watching for ways to strengthen a position or exploit the opponent's weaknesses, without trying
    to foresee every contingency. If your position is good, you should be well-equipped to handle any unexpected moves.

  (M21) in positions where players are attacking on opposite wings, be willing to make material sacrifices to delay your
    opponent. Also, be willing to allow your opponent to break through first, if this is necessary for you to have time to
    prepare your own breakthrough properly. It is less important to break through first, than to have the ability to
    follow up a breakthrough by promoting weak pieces and Vertical Movers and, by bringing your Lion closer to the
    enemy King. An attack that cannot be followed up properly, because it was inadequately prepared, will fizzle out
    against proper defence.


  **********************
  *** Mating Attacks ***
  **********************

    In Shogi and Go, the term semeai refers to a mutual life-or-death struggle in which only one side (usually) can
    survive (Jishogi and Seki are exceptions). Such positions arise in Shogi because of the drop rule, which gives attacks
    so much momentum that defensive moves alone, however accurate, are unlikely to succeed in repelling them; and
    an attacked player must, therefore, counter-attack to have a chance. In Middle Shogi, semeai positions frequently
    arise when Lions are close to the enemy Kings. If a Lion reaches the ninth rank and cannot be driven away, a speedy
    counter-attack is essential; for mate is almost inevitable once re-inforcements are brought up in the form of weak
    pieces to help pry open the castle and long-range pieces to pin key defenders. Sacrificial mating combinations
    abound whenever a Lion is near a castle that is even slightly weakened.
    The positions we consider below emphasise speed in mating attacks and they illustrate the importance and power of
    the Lion, in such attacks. First, though, let us consider some general principles.

  ++++++++++++++++++++++++
  + Principles of Attack +
  ++++++++++++++++++++++++

  (A1) Bring the Lion as close to the enemy King as possible. If the opponent has set up a 'moat' to keep the Lion out,
    by placing Side Movers on two successive ranks, try to 'build a bridge' by side-moving pieces. Often a single weak
    piece is enough, since the Lion only needs to reach the ninth rank to be able to put direct pressure on the castle.

  (A2) Bring up one or two weak pieces to help pry open the castle. Because of the Lions igui power, it can support a
    weak piece from an adjacent square, no matter how many defenders are attacking it. Concentrate on exchanging off
    pieces - usually Golds or a Drunk Elephant - that are guarding the 'blind spots' in front of Blind Tigers.

  (A3) Try to pin defending-castle pieces with your own long-range pieces. If these pieces can also keep an eye on
    defending your own castle, so much the better. In conjunction with an advanced Lion, Rooks and other
    vertically-moving pieces placed in front of the castle, especially when aligned with the opposing King, often open up
    possibilities of decisive combinations.

  (A4) Look for ways to break open the castle by sacrificing your own long-range pieces. Do not underestimate the Lion's
    ability to mate a King unassisted, even with some defending pieces in the area. If you can force the King out of its
    castle with a Lion in pursuit, you will almost always have a forced mate. But also watch for chances to break open
    a castle by means of a Lion sacrifice, provided you are sure that your long-range pieces will then suffice to mate the
    exposed King.

  (A5) In semeai positions, as in Shogi endgames, go for speed rather than material.

  (A6) When in doubt, attack rather than defend.

  +++++++++++++++++++++++++
  + Principles of Defence +
  +++++++++++++++++++++++++

  (D1) Build a moat to keep the opposing Lion away from your castle. If possible, defend your fourth rank from both sides
    (usually with Side Movers), to make it harder for the Lion to advance by building a bridge; also make it harder for
    enemy weak pieces to promote. In addition to covering your fourth rank securely, keep a Rook on the third or fifth
    rank to keep the Lion from crossing over.

  (D2) Keep the blind spots in front of your Blind Tigers (if these are part of your castle) defended by as many pieces as
    possible, both long-range and short-range.

  (D3) If the enemy Lion crosses your moat and enters a corner, set up a wall of weak pieces between the Lion and your
    King. The wall should be free of defects and preferably anchored by a gold on the first rank. Open an escape route
    for your King towards the opposite corner to avoid any 'smothered mates' by a Lion, Horned Falcon or Soaring
    Eagle.

  (D4) Unless your own counter-attack is hopelessly slow, make only the most essential defensive moves and try and
    counter-attack.


  *****************
  *** Exchanges ***
  *****************

    Following these notes on strategy and a few of the above examples of actual playing positions, it will be clear that
    the table of relative piece values given on Page 50, although a useful tool, does not give the whole picture. By way
    of further explanation, the following additional notes will be useful:-
    As mentioned above, within each group, the pieces are of roughly equal value, although pieces that appear first are
    slightly better. The reason that a Dragon Horse is grouped with a Horned Falcon, is that it is nearly as good in a
    closed position and it can promote, virtually at will, when the board opens up. Thus, the table tells a player that he
    may safely exchange a Horned Falcon for a Dragon Horse, without having to feel at a disadvantage.
    Whilst the table is useful in deciding whether to make one-for-one piece exchanges, it does not indicate how much
    better one piece or group of pieces is than the next; and so is of little help in deciding two-for-one or more
    complicated exchanges. Also, the table shows only average piece-values, and, in an actual game, the values will vary
    according to the proximity of pieces to the promotion zone. The following additional guidelines may, therefore, be
    useful:-

  (X1) Exchanging a Lion for a Free King-type piece (any piece in the group that includes the Free King):
    Early in the game, do not give up your Lion unless you can get in return something more than two Free King-type
    pieces. As the board begins to open up, the Lion's exchange value will gradually diminish, but it will remain more
    valuable than a single Free King-type piece; even in the endgame, provided it is aggressively posted and
    adequately supported. A Lion in the vicinity of the enemy King is powerful when accompanied by one or more
    friendly weak (General-type) pieces. They serve to shield the Lion from attacks by Long-range pieces and threaten
    to help break up the enemy castle. Such a Lion should not be exchanged for a Free King. But if a Lion is posted
    defensively, or is unsupported, or cannot escape continued harassment from long-range pieces, it is just as well to
    give it up for a Free King-type piece in the later stages of the game.

  (X2) Exchanging a Free King-type piece for two or more lesser pieces:
    In judging whether to give up a Free King for some combination of weaker pieces, remember that the player with
    the greater number of Rook-type pieces (including Free King-type pieces) will enjoy something akin to 'air
    superiority' in the endgame, which is important in helping to advance and to promote the remaining weak pieces.
    Therefore, try not to exchange a Free King unless one of the pieces obtained in return is a Rook or better.
    Generally, an exchange of a Free King-type piece for a Rook plus a medium piece (Side Mover or Gold, for
    example) is about even. On the other hand, Free Kings cannot cope with numerous adverse pieces in the same way
    a Queen can in Western Chess. because of rules against repetition and perpetual check. For this reason, it may be
    good to give up your Free King for almost any three opposing pieces, other than Pawns or Go-betweens, which have
    little or no exchange value. Note that in an endgame of King and two Blind Tigers against King and Free King, the
    Blind Tigers will win by force, since their promotion cannot be prevented. Therefore, taking even two weak pieces
    for a Free King should be considered when there are only a few pieces left on the board.

  (X3) Sacrificing one piece in order to promote another:
    This type of opportunity arises fairly frequently in play. Generally, if both pieces are initially weak it is acceptable
    to sacrifice one in order to promote the other. If either piece is strong, it is usually better to play patiently and delay
    the promotion until it can be achieved without sacrifice. As an exception to this principle, it is usually worth
    making a reasonable sacrifice to promote a Phoenix, Kylin, Vertical Mover or Side Mover.


  *******************
  *** The Endgame ***
  *******************

    Although many close games of Middle Shogi end with a semeai, others simplify into positions in which neither King
    is in immediate danger. If the material is fairly even, the advantage will generally go to the player first able to
    promote one or more short-range pieces into long-range pieces. Once a player has a numerical advantage in
    long-range pieces, particularly Rook-type and Free King-type pieces, he will be able to use these pieces to expedite
    additional promotions and to hamper his opponent from doing the same. He may also be able to threaten the enemy
    King directly.
    Endgames may occur with so many different combinations of pieces that volumes could be written on the subject.
    Fortunately, common sense will suffice to guide players in most positions. The following points may be helpful:-

  (E1) Promote as quickly as possible the piece within your camp that has the best promoted value. Generally, this means
    either a Phoenix, Kylin, Side Mover or Gold General. Advance pieces in groups only when necessary for their
    protection.

  (E2) If you have an advantage in long-range pieces and at least equality in short-range pieces, try to exchange off
    long-range pieces. An advantage of one long-range piece to none is greater than an advantage of two long-ranges
    pieces to one.

  (E3) If you have an advantage in long-range pieces, but an irremediable inferiority in short-range pieces, avoid
    exchanges and try to promote enough additional pieces to organise a matin attack.

  (E4) In the endgame, promoted Rooks are very much better than promoted Bishops and promoted Gold Generals are
    very much better than promoted Ferocious Leopards.

  (E5) Chess players should be aware of the drastic effects of three rules that differ from Western Chess:-

    (i) Perpetual check is illegal. as a result, an ending such as King and Free King against King and two Blind Tigers
    is a loss for the Free King side, because there is no way to prevent the Blind Tigers from promoting, driving the King
    to the edge and winning the Free King for one Flying Stag. It is believed that a King and a Free King can draw
    against a King and two Generals, unless one of the Generals is an unpromoted Gold; but this area of the game needs
    further research

    (ii) A stalemated King loses, because of the bare King rule. This rule is not of much importance except in rare
    problem-like positions.

    (iii) Reducing the opponent to a bare King position is deemed sufficient to win the game. As a result, certain
    endgames that could not ordinarily be won by force (King and Pawn versus King; King and Go-between versus King
    and King and promoted Ferocious Leopard versus King) are won nonetheless, as in Shatranj and many other old
    Chess variants. Incidentally, any other combinations of King and one or two other pieces versus King seems
    sufficient, at least, to stalemate an enemy King, which wins anyway; a fact that may be noteworthy in variants in
    which the existence of a bare King rule is uncertain.

  (E6) Promotion of the Drunk Elephant should be given a fairly high priority. A player with two Kings should keep at
    least one of them protected when there are ranging pieces about; generally this will force the opponent to exchange
    a long-range piece for one of the Kings before attacking the other,

  (E7) Certain endings in which each player has a King and just one other piece have outcomes that may surprise
    players of Western Chess. For instance, King and Gold can draw against King and Free King, but King and
    promoted Gold (Rook) lose against a Free King. In the ending of King and Lion versus King and Free King, only
    the Lion side has winning chances; and the Lion will, in fact, win anytime it can safely check the enemy King. A great
    deal of research needs to be done on endings with more than one piece per side, to determine which piece combinations
    can defeat which.


  *******************
  *** Step Movers ***
  *******************

    Let us now turn our attention to the Step Movers - the pieces (other than Kings and Pawns) that move only one
    square at a time. In Middle Shogi these pieces play a vital strategic role, similar to that of Pawns in Western Chess.
    When I play Western Chess, Grand Chess or other games that have orthodox Pawns, my primary strategic focus is
    on Pawn structure: keeping mine sound, while trying to weaken my opponent's. When I play Middle Shogi, in which
    Pawn structure, in the Chess sense, doesn't exist, I concentrate instead on the efficient deployment of my step movers.
    Being so much weaker than the ranging pieces of Middle Shogi, step movers logically belong in the front lines, just
    behind the Pawns. Yet in the opening position, the Copper, Silver and Gold Generals, as well as the Ferocious
    Leopards, are relegated to the first rank. Why?
    Because of a good decision by the game's designer(s). If a General or Leopard started on the third rank, its sphere
    of action - barring a foolish time-losing retreat - would mainly be limited to the V-shaped set of squares in front of
    it. A Copper starting on 8c, for example, would not be able to participate in a defence of a potentially important
    promotion square on the flank, such as 12d or 11d. A Copper starting on 10a, on the other hand, can reach any thirdrank
    square from 12c through 8c.
    A well-designed game maximises players' strategic choices. If step movers started on the third rank, Middle Shogi
    would require a bit less patience to play, but, strategically, it would be a far less interesting game.
    So how do you get your step movers past the ranging pieces that are in their way at the start of the game? I
    recommend the following three principles: First, plan ahead; re-arranging your pieces in the cramped opening
    position is like solving a sliding-block problem and one wrong move can cost you more than one tempo. Second, to
    minimise loss of time, make as few moves as possible with your ranging pieces. Third, always try to make the least
    committal move possible - that is, choose the move you are most certain you will play eventually. It's important to
    appreciate that a decision to advance a step mover towards the left, centre or right side of the board has long-term
    implications for your position. Although not quite irrevocable like a Pawn move, a step mover advance is
    impractical to undo because of the loss of time involved.
    There's something else to keep in mind when advancing your step movers. If you spend eight moves to advance your
    Silver and end up exchanging it for an opponent's Silver that has only moved three times, you've just lost five tempi.
    That's obviously very bad, unless you get compensation, such as being able to promote a piece because the
    defending Silver is out of the way. So the purpose of advancing step movers on a wing where you hope to attack is
    not to exchange them off for defending pieces; it's to secure space and build up a strong position in which you will
    gain solid material benefits, when the position finally opens up. Ideally, you want to have at least one more step
    mover in an area of attack than the opponent has defending; so that even if all possible pairs of Pawns and pieces
    are exchanged, you will still have at least one step mover left in the area. This extra piece will not only promote, but
    also establish a beachhead that will facilitate further incursions into the promotion zone with your ranging pieces.
    If you don't end up having this extra piece, then your breakthrough was premature and you should have brought up
    another step mover - even a Gold General or Blind Tiger, if necessary - before opening up the position.
    Conversely, when your opponent is preparing a breakthrough on your side of the board, you want to have at least
    as many step movers in the area as your opponent does. Each time you manage to exchange a third- or fourth-rank
    step mover for an opposing step mover, you'll have gained several tempi.
    When choosing which step mover to advance early in the game, you need to imagine how the game will develop and
    ask yourself which step mover will be needed for defence, which ones will be needed for attack and in which area
    of the board each one should be deployed. As the game unfolds and your opponent's plans become clearer, you
    should continually re-examine these questions and adjust your plans as necessary.

   ************************************************************
   ************************************************************ */

static inline int dist(int i, int j)
{
  const int dr = (i/12) - (j/12);
  const int dc = (i-j)%12;
  return ((dr >= 0 ? dr : -dr) + (dc >= 0 ? dc : -dc));
}

int dump_evaluation = 0;

evalt evaluate(const evaluator* e, const board* b, int ply, evalt alpha, evalt beta)
{
  if (draw_by_repetition(b)) return b->to_move==white ? W_WIN_IN_N(ply) : B_WIN_IN_N(ply);
  if (!b->king_mask[black] && !b->prince_mask[black]) return W_WIN_IN_N(ply);
  if (!b->king_mask[white] && !b->prince_mask[white]) return B_WIN_IN_N(ply);

  /* TODO:
   *   - lion "moats" - (SM/rook/etc. on adjacent ranks)
   *   - move material/piece_square to incremental ([un]make_move)
   *   - attacked piece malus
   *   - hanging piece malus
   *   - 
   */

  /* These values can be pre-computed and stored in an array before starting a search... */
  const int opening_phase = OPENING_PHASE(b->ply);
  const int midgame_phase = MIDGAME_PHASE(b->ply);
  const int endgame_phase = ENDGAME_PHASE(b->ply);
  const evalt total_phase = opening_phase + midgame_phase + endgame_phase;

  const evalt protected_piece_weight = ( e->protected_piece[opening] * opening_phase
                                       + e->protected_piece[midgame] * midgame_phase
                                       + e->protected_piece[endgame] * endgame_phase ) / total_phase;
  const evalt mobility_square_control_weight = ( e->mobility_square_control[opening] * opening_phase
                                               + e->mobility_square_control[midgame] * midgame_phase
                                               + e->mobility_square_control[endgame] * endgame_phase) / total_phase;
  const evalt mobility_square_count_weight = ( e->mobility_square_count[opening] * opening_phase
                                             + e->mobility_square_count[midgame] * midgame_phase
                                             + e->mobility_square_count[endgame] * endgame_phase) / total_phase;
  const evalt mobility_promotion_control_weight = ( e->mobility_promotion_control[opening] * opening_phase
                                                  + e->mobility_promotion_control[midgame] * midgame_phase
                                                  + e->mobility_promotion_control[endgame] * endgame_phase) / total_phase;

  const int mating_scale = min(300, (b->ply)); /* TODO: use piece-count instead ? */

  // TODO: for TD-lambda, for example, we don't need to recompute this for every change of weights
  //       (when doing partials...)  let's cache these computations somehow
  int counts_[num_color][12*12];
  compute_board_control(b, white, counts_[white]);
  compute_board_control(b, black, counts_[black]);

  // TODO: BUG in compute_board_control() vs compute_board_control_alt() ... different numbers...
  if (0 && logfile()) {
    int alt_counts_[num_color][12*12];
    compute_board_control_alt(b, white, alt_counts_[white]);
    compute_board_control_alt(b, black, alt_counts_[black]);
    if (memcmp(counts_[white], alt_counts_[white], sizeof(counts_[white]))
        || memcmp(counts_[black], alt_counts_[black], sizeof(counts_[black])))
    {
      show_board(logfile(), b);
      int r, c;
      for (r=11; r>=0; --r)
      {
        for (c=0; c<12; ++c)
        {
          fprintf(logfile(), "| %2i%s%-2i",
              counts_[white][r*12+c], (counts_[white][r*12+c]==alt_counts_[white][r*12+c]) ? "." : "*", alt_counts_[white][r*12+c]);
          fprintf(logfile(), "%s", b->b[r*12+c].c ? piece_kanji1[b->b[r*12+c].promoted][b->b[r*12+c].piece] : "  ");
          fprintf(logfile(), "%2i%s%-2i",
              counts_[black][r*12+c], (counts_[black][r*12+c]==alt_counts_[black][r*12+c]) ? "." : "*", alt_counts_[black][r*12+c]);
        }
        fprintf(logfile(), "\n");
      }
      exit(-17);
    }
  }

  evalt material = 0;
  evalt mobility = 0;
  evalt positional_mating = 0;
  evalt king_tropism = 0;

  alpha=alpha; beta=beta;

  int material_[num_color] = {0,0,0};
  int protected_piece_[num_color] = {0,0,0};
  int lion_safety_[num_color] = {0,0,0};
  int king_safety_[num_color] = {0,0,0};
  int king_tropism_[num_color] = {0,0,0};
  int positional_mating_[num_color] = {0,0,0};
  int piece_count_longs_[num_color][6] = {{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0}};

  /* find _a_ king/prince for each side */
  int king_loc[num_color] = {0,-1,-1};
  {
    color c;
    for (c=white; c<=black; ++c)
    {
      if (b->king_mask[c])
        king_loc[c] = b->pl[c][king][bit_scan_forward32(b->king_mask[c])].loc;
      else if (b->prince_mask[c])
        king_loc[c] = b->pl[c][drunk_elephant][bit_scan_forward32(b->prince_mask[c])].loc;
    }
  }

  {
    color c;
    for (c=white; c<=black; ++c)
    {
      const color oc = c^3;

      {
        /* multi-king bonus */
        const int num_kings = count_bits32(b->king_mask[c] | (b->prince_mask[c]<<16));
        if (num_kings > 1) material_[c] += 300;
      }

      piece ip;
      for (ip=1; ip<num_piece; ++ip)
      {
        int j;
        for (j=0; b->pl[c][ip][j].valid; ++j)
        {
          register const piece_info current_piece = b->pl[c][ip][j];
          if (!current_piece.active) continue;

          const int pro = current_piece.promoted;
          const int loc = current_piece.loc;
          const int xloc = (c==white) ? loc : (143-loc);

          /* material */
          material_[c] += e->piece_vals[ip][pro]
             + ( opening_phase * e->piece_square_bonus[opening][ip][pro][xloc]
               + midgame_phase * e->piece_square_bonus[midgame][ip][pro][xloc]
               + endgame_phase * e->piece_square_bonus[endgame][ip][pro][xloc] ) / total_phase;

          material_[c] += 10*current_piece.tempi/(10 + b->ply);

          if (ip != king)
          {
            const int mob = current_piece.mobility;
            material_[c] += mob;

            /* long-distance piece */
            if (piece_move_type[c][ip][pro]&0xFF0000) 
            {
              const int num_dir = count_bits32(piece_move_type[c][ip][pro]&0xFF0000);

              /* penalize trapped rangers */
              const int min_mob = 5*num_dir/2;

              if (mob < min_mob)
                material_[c] -= 10*(min_mob - mob)*endgame_phase / total_phase;

              const int max_mob = current_piece.max_mobility;
              if (max_mob < 8)
                material_[c] -= 10*(8 - max_mob)*endgame_phase / total_phase;

              ++piece_count_longs_[c][min(num_dir,6)-1];
            }
          }

          /* king safety */
          if (ip == king)
          {
            const int x = loc % 12;
            const int y = loc / 12;
            int x1, y1;
            int ok_squares = 0;
            int defended_squares = 0;
            int total_attacks = 0;
            for (x1=max(0,x-1); x1<=min(11,x+1); ++x1)
            {
              for (y1=max(0,y-1); y1<=min(11,y+1); ++y1)
              {
                if (x1==x && y1==y) continue;
                const int loc1 = 12*y1 + x1;
                if (!counts_[oc][loc1]) ++ok_squares;
                else total_attacks += counts_[oc][loc1]*2;
              }
            }
            for (x1=max(0,x-2); x1<=min(11,x+2); ++x1)
            {
              for (y1=max(0,y-2); y1<=min(11,y+2); ++y1)
              {
                if (x1==x && y1==y) continue;
                const int loc1 = 12*y1 + x1;
                if (counts_[c][loc1] > 1) ++defended_squares;
              }
            }
            king_safety_[c] += e->king_safe[min(3,ok_squares)]
                             + min(defended_squares,5) * 2
                             - total_attacks * e->king_attacked;
          }

          /* lion safety */
          if (ip == lion)
          {
            const int x = loc % 12;
            const int y = loc / 12;
            int ok_squares = 0, x1, y1;
            for (x1=max(0,x-2); x1<=min(11,x+2); ++x1)
            {
              for (y1=max(0,y-2); y1<=min(11,y+2); ++y1)
              {
                if (x1==x && y1==y) continue;
                const int loc1 = 12*y1 + x1;
                if (!counts_[oc][loc1] && (b->b[loc1].c != c)) ++ok_squares;
              }
            }
            lion_safety_[c] += e->lion_free[min(3,ok_squares)]
                             + e->lion_protected[min(3,counts_[c][loc])];
          }

          /* king tropism */
          const int okl = king_loc[oc];
          if (okl >= 0)
          {
            /* (A1) bring lion close to enemy king */
            if (ip==lion || (ip==kirin && pro))
              positional_mating_[c] += 2*(20 - dist(loc, okl));

            switch (ip)
            {
              case king: break;
              case bishop:
                king_tropism_[c] -= abs(abs((loc-okl)%12) - abs((loc/12)-(okl/12))) * e->king_tropism_vals[ip][pro];
                break;
              case rook:
              case dragon_king:
                king_tropism_[c] -= min(abs((loc-okl)%12), abs((loc/12)-(okl/12))) * e->king_tropism_vals[ip][pro];
                break;
              case vertical_mover:
                king_tropism_[c] -= abs((loc-okl)%12) * e->king_tropism_vals[ip][pro];
                break;
              case side_mover:
                king_tropism_[c] -= abs((loc/12)-(okl/12)) * e->king_tropism_vals[ip][pro];
                break;
              default:
                king_tropism_[c] += 2*e->king_tropism_vals[ip][pro] / dist(loc, okl);
                break;
            }
          }

          /* protected piece bonus/malus */
          protected_piece_[c] += counts_[c][loc] ? min(counts_[c][loc], 3) : -2;
        }
      }
    }
  }
  positional_mating = (positional_mating_[white] - positional_mating_[black]);
  king_tropism = (king_tropism_[white] - king_tropism_[black]);
  material = (material_[white] - material_[black]);

  {
    int j;
    /* bonus for domination in long-distance pieces */
    // TODO: check for status of step movers ...
    // TODO: include imbalance bonus for weaker rangers also
    // TODO: clean this up a bit - not sure it makes complete sense...
    int major_total = 0;
    for (j=5; j>=0; --j)
    {
      if (piece_count_longs_[white][j] > piece_count_longs_[black][j])
      {
        material += endgame_phase * 10 * (j+1)
            * (1 + piece_count_longs_[white][j] + major_total) / (1 + piece_count_longs_[black][j] + major_total) / total_phase;
        break;
      }
      else if (piece_count_longs_[white][j] < piece_count_longs_[black][j])
      {
        material -= endgame_phase * 10 * (j+1)
            * (1 + piece_count_longs_[black][j] + major_total) / (1 + piece_count_longs_[white][j] + major_total) / total_phase;
        break;
      }
      else
      {
        major_total += piece_count_longs_[white][j];
      }
    }
  }

  mobility += (protected_piece_[white] - protected_piece_[black]) * protected_piece_weight;
  mobility += (lion_safety_[white] - lion_safety_[black]);
  mobility += (king_safety_[white] - king_safety_[black]);

  {
    int i;

    int mobility_square_count_w = 0, mobility_square_count_b = 0;
    int mobility_square_control_w = 0, mobility_square_control_b = 0;
    for (i=0; i<12*12; ++i)
    {
      /* count possible squares (including x-ray moves) */
      mobility_square_count_w += max(3,counts_[white][i]);
      mobility_square_count_b += max(3,counts_[black][i]);
    }

    for (i=0; i<12*12; ++i)
    {
      /* square control bonus */
      if      (counts_[white][i] && !counts_[black][i]) ++mobility_square_control_w;
      else if (counts_[black][i] && !counts_[white][i]) ++mobility_square_control_b;
    }

    mobility += (mobility_square_count_w - mobility_square_count_b) * mobility_square_count_weight;
    mobility += (mobility_square_control_w - mobility_square_control_b) * mobility_square_control_weight;

    /* promotion square control */
    int mobility_promotion_control_w = 0, mobility_promotion_control_b = 0;

    for (i=0   ; i< 4*12; ++i)
      mobility_promotion_control_w += counts_[white][i] ?  min(2,counts_[white][i])
                                    : counts_[black][i] ? -min(2,counts_[black][i])*5
                                    : 0;

    for (i=8*12; i<12*12; ++i)
      mobility_promotion_control_b += counts_[black][i] ?  min(2,counts_[black][i])
                                    : counts_[white][i] ? -min(2,counts_[white][i])*5 :
                                    0;

    mobility += (mobility_promotion_control_w - mobility_promotion_control_b) * mobility_promotion_control_weight;
  }

  /* TODO: unified black/white code */
  evalt positional = 0;
  {
    /* opening */
    evalt positional_fortress = 0;
    evalt positional_pawns = 0;
    {
      /* (O3) Pawns s.b. supported by Copper or Silver / maybe 2 Ferocious Leopard & Gold */
      color c;
      for (c=white; c<=black; ++c)
      {
        int j;
        const int dir = (c==white) ? -12 : +12;
        evalt support_[num_color] = {0,0,0};
        for (j=0; b->pl[c][pawn][j].valid; ++j)
        {
          if (!b->pl[c][pawn][j].active || b->pl[c][pawn][j].promoted) continue;
          const int pl = b->pl[c][pawn][j].loc;
          const int pr = pl / 12;
          const int pc = pl % 12;
          const int maxr = (c==white) ? min(4,pr) : min(4,11-pr);
          int r;
          for (r=1; r<maxr; ++r)
          {
            if (b->b[pl+dir*r].c==c)
            {
              const piece p = b->b[pl+12*r].piece;
              if (p==copper_general || p==silver_general)       support_[c] += 7/r;
              else if (p==ferocious_leopard || p==gold_general) support_[c] += 4/r;
            }
          }
          if (pc>0)
            for (r=1; r<maxr; ++r)
            {
              if (b->b[pl+dir*r-1].c==c)
              {
                const piece p = b->b[pl+12*r-1].piece;
                if (p==copper_general || p==silver_general)       support_[c] += 6/r;
                else if (p==ferocious_leopard || p==gold_general) support_[c] += 3/r;
              }
            }
          if (pc<11)
            for (r=1; r<maxr; ++r)
            {
              if (b->b[pl+dir*r+1].c==c)
              {
                const piece p = b->b[pl+12*r+1].piece;
                if (p==copper_general || p==silver_general)       support_[c] += 6/r;
                else if (p==ferocious_leopard || p==gold_general) support_[c] += 3/r;
              }
            }
        }
        positional_pawns += (support_[white] - support_[black])
            + ((support_[white] < 4) ? -5 : 0)
            + ((support_[black] < 4) ? +5 : 0);
      }

      /* (O4) king s.b. in back with G, 2BT, DE for defence */
      {
        color c;
        evalt king_fortress_[num_color] = {0,0,0};
        for (c=white; c<=black; ++c)
        {
          if (b->pl[c][king][0].active) 
          {
            int j;
            const int kl = b->pl[c][king][0].loc;
            for (j=0; b->pl[c][gold_general][j].valid; ++j)
            {
              if (!b->pl[c][gold_general][j].active || b->pl[c][gold_general][j].promoted) continue;
              const int l = b->pl[c][gold_general][j].loc;
              if ((abs((kl-l)%12)<=1) && (abs((kl/12)-(l/12))<=1))       king_fortress_[c] += 10;
              else if ((abs((kl-l)%12)<=2) && (abs((kl/12)-(l/12))<=2))  king_fortress_[c] +=  2;
            }
            for (j=0; b->pl[c][blind_tiger][j].valid; ++j)
            {
              if (!b->pl[c][blind_tiger][j].active || b->pl[c][blind_tiger][j].promoted) continue;
              const int l = b->pl[c][blind_tiger][j].loc;
              if ((abs((kl-l)%12)<=1) && (abs((kl/12)-(l/12))<=1))       king_fortress_[c] += 30;
              else if ((abs((kl-l)%12)<=2) && (abs((kl/12)-(l/12))<=2))  king_fortress_[c] +=  3;
            }
            for (j=0; b->pl[c][drunk_elephant][j].valid; ++j)
            {
              if (!b->pl[c][drunk_elephant][j].active) continue;
              const int l = b->pl[c][drunk_elephant][j].loc;
              if ((abs((kl-l)%12)<=1) && (abs((kl/12)-(l/12))<=1))       king_fortress_[c] += 30;
              else if ((abs((kl-l)%12)<=2) && (abs((kl/12)-(l/12))<=2))  king_fortress_[c] +=  3;
            }
          }
        }
        positional_fortress += (king_fortress_[white] - king_fortress_[black]);
      }
    }

    positional += (opening_phase/3 + midgame_phase + endgame_phase/5) * positional_fortress / total_phase
               + (opening_phase + 2*midgame_phase/3 + endgame_phase/4) * positional_pawns / total_phase
               + mating_scale * positional_mating / 100;
  }

  /* randomness */
  const double randomness = ((int)((unsigned int)b->h % 256) - 128)/(64.0 + b->ply);

  /* tempo bonus */
  const double tempo = 50.0*(b->to_move==white ? +3 : -3)/(50 + b->ply);

  return (material + king_tropism + (evalt)(randomness + tempo) + positional*7 + mobility);
}

/* ************************************************************ */

evalt evaluate_relative(const evaluator* e, const board* b, int ply, evalt alpha, evalt beta)
{
  return b->to_move==white ? evaluate(e, b, ply, alpha, beta) : -evaluate(e, b, ply, -beta, -alpha);
  return DRAW;
}

/* ************************************************************ */

int evaluate_moves_for_search(const evaluator* e, const board* b, const movet* ml, evalt* vl, int nm)
{
  const int opening_phase = OPENING_PHASE(b->ply);
  const int midgame_phase = MIDGAME_PHASE(b->ply);
  const int endgame_phase = ENDGAME_PHASE(b->ply);
  const int total_phase = opening_phase + midgame_phase + endgame_phase;

  int i;
  for (i=0; i<nm; ++i)
  {
    const movet m = ml[i];
    evalt v = 0;
    if (m.is_special)
    {
      if (m.special.movetype)
      {
        /* lion-type */
        const int to = m.special.from + dir_map[m.special.to1];
        const int to2 = to + dir_map[m.special.to2];
        if (m.special.capture1 || m.special.capture2)
        {
          int caps = 0;
          if (m.special.capture1)
          {
            const int p = b->b[to].piece;
            const int pro = b->b[to].promoted;
            caps += e->piece_vals[p][pro];
          }
          if (m.special.capture2)
          {
            const int p2 = b->b[to2].piece;
            const int pro2 = b->b[to2].promoted;
            caps += e->piece_vals[p2][pro2];
          }
          v = caps - e->piece_vals[m.special.piece][m.special.promoted_piece]/100 + 50;
        }
        else
        {
          if (to2 == m.from)
          {
            v = -1000000;
          }
          else
          {
            // TODO: not clear if this really helps...
            // definitely makes it slower, but does it improve anything???
            const piece p = m.piece;
            const int pro = m.promoted_piece;
            const int from = m.side_is_white ? m.from : 143 - m.from;
            const int to = m.side_is_white ? to2 : 143 - to2;
            v = ((e->piece_square_bonus[opening][p][pro][to] - e->piece_square_bonus[opening][p][pro][from])*opening_phase
               + (e->piece_square_bonus[midgame][p][pro][to] - e->piece_square_bonus[midgame][p][pro][from])*midgame_phase
               + (e->piece_square_bonus[endgame][p][pro][to] - e->piece_square_bonus[endgame][p][pro][from])*endgame_phase
               )/total_phase;
          }
        }
      }
      else
      {
        v = -1000000;
      }
    }
    else
    {
      v = 0;
      if (m.capture)
      {
        const int op = b->b[m.to].piece;
        const int opro = b->b[m.to].promoted;
        v += e->piece_vals[op][opro] - e->piece_vals[m.piece][m.promoted_piece]/80 + 50;
      }

      {
        // TODO: not clear if this really helps...
        // definitely makes it slower, but does it improve anything???
        const piece p = m.piece;
        const int pro = m.promoted_piece;
        const int from = m.side_is_white ? m.from : 143 - m.from;
        const int to = m.side_is_white ? m.to : 143 - m.to;
        v += ((e->piece_square_bonus[opening][p][pro][to] - e->piece_square_bonus[opening][p][pro][from])*opening_phase
           + (e->piece_square_bonus[midgame][p][pro][to] - e->piece_square_bonus[midgame][p][pro][from])*midgame_phase
           + (e->piece_square_bonus[endgame][p][pro][to] - e->piece_square_bonus[endgame][p][pro][from])*endgame_phase
           )/total_phase;
      }

      if (ml[i].promote)
        v += (e->piece_vals[m.piece][1] - e->piece_vals[m.piece][0])/10;
    }
    vl[i] = v;
  }
  return 0;
}

/* ************************************************************ */

int simple_evaluator(evaluator* e)
{
  memset(e, '\x00', sizeof(evaluator));

  /* **************************************** */

  if (0)
  { // needs further testing (TODO)

    // "controller" weights:
    e->mobility_square_count[opening] = 3;
    e->mobility_square_control[opening] = 10;
    e->mobility_promotion_control[opening] = 5;
    e->protected_piece[opening] = 10;

    e->mobility_square_count[midgame] = 1;
    e->mobility_square_control[midgame] = 2;
    e->mobility_promotion_control[midgame] = 2;
    e->protected_piece[midgame] = 5;

    // old weights:
    e->mobility_square_count[endgame] = 1;//0.5;
    e->mobility_square_control[endgame] = 1;
    e->mobility_promotion_control[endgame] = 1;
    e->protected_piece[endgame] = 2;
  }
  else
  { // needs further testing (TODO)

    // "controller" weights:
    e->mobility_square_count[opening] = 1;//0.5;
    e->mobility_square_control[opening] = 1;
    e->mobility_promotion_control[opening] = 1;
    e->protected_piece[opening] = 2;

    e->mobility_square_count[midgame] = 1;//0.5;
    e->mobility_square_control[midgame] = 1;
    e->mobility_promotion_control[midgame] = 1;
    e->protected_piece[midgame] = 2;

    // old weights:
    e->mobility_square_count[endgame] = 1;//0.5;
    e->mobility_square_control[endgame] = 1;
    e->mobility_promotion_control[endgame] = 1;
    e->protected_piece[endgame] = 2;
  }


  e->lion_protected[0] = -15;
  e->lion_protected[1] = +25;
  e->lion_protected[2] = +50;
  e->lion_protected[3] = +60;

  e->lion_free[0] = -250;
  e->lion_free[1] = -100;
  e->lion_free[2] =  -10;
  e->lion_free[3] =  +30;

  e->king_attacked = 1;
  e->king_safe[0] = -150;
  e->king_safe[1] =  -50;
  e->king_safe[2] =   -5;
  e->king_safe[3] =  +20;

  /* **************************************** */

  e->piece_vals[king][0] = 500;

  e->piece_vals[lion][0] = 2200;
  e->piece_vals[kirin][1] = 2200;

  e->piece_vals[free_king][0] = 1450;
  e->piece_vals[phoenix][1] = 1450;
  e->piece_vals[dragon_king][1] = 1275;
  e->piece_vals[dragon_horse][1] = 1250;
  e->piece_vals[dragon_king][0] = 790;
  e->piece_vals[dragon_horse][0] = 755;

  e->piece_vals[vertical_mover][1] = 670;
  e->piece_vals[side_mover][1] = 630;

  e->piece_vals[rook][1] = 570;
  e->piece_vals[rook][0] = 535;
  e->piece_vals[phoenix][0] = 505;

  e->piece_vals[bishop][1] = 470;
  e->piece_vals[bishop][0] = 410;
  e->piece_vals[vertical_mover][0] = 400;
  e->piece_vals[gold_general][1] = 393;

  e->piece_vals[kirin][0] = 388;
  e->piece_vals[side_mover][0] = 382;

  e->piece_vals[lance][1] = 380;
  e->piece_vals[reverse_chariot][1] = 375;
  e->piece_vals[drunk_elephant][1] = 500;

  e->piece_vals[reverse_chariot][0] = 310;
  e->piece_vals[lance][0] = 300;

  e->piece_vals[blind_tiger][1] = 290;

  e->piece_vals[gold_general][0] = 280;
  e->piece_vals[drunk_elephant][0] = 270;

  e->piece_vals[ferocious_leopard][1] = 250;

  e->piece_vals[silver_general][1] = 230;
  e->piece_vals[copper_general][1] = 220;

  e->piece_vals[ferocious_leopard][0] = 202;
  e->piece_vals[blind_tiger][0] = 196;
  e->piece_vals[silver_general][0] = 190;

  e->piece_vals[go_between][1] = 170;
  e->piece_vals[pawn][1] = 160;
  e->piece_vals[copper_general][0] = 150;

  e->piece_vals[go_between][0] = 130;
  e->piece_vals[pawn][0] = 100;

  /* **************************************** */

  e->king_tropism_vals[king][0] = 0;
  e->king_tropism_vals[lion][0] = 7;
  e->king_tropism_vals[free_king][0] = 1;

  e->king_tropism_vals[pawn][0] = 0;
  e->king_tropism_vals[pawn][1] = 5;
  e->king_tropism_vals[go_between][0] = 0;
  e->king_tropism_vals[go_between][1] = 4;
  e->king_tropism_vals[reverse_chariot][0] = 1;
  e->king_tropism_vals[reverse_chariot][1] = 2;
  e->king_tropism_vals[lance][0] = 1;
  e->king_tropism_vals[lance][1] = 3;
  e->king_tropism_vals[dragon_king][0] = 2;
  e->king_tropism_vals[dragon_king][1] = 1;
  e->king_tropism_vals[dragon_horse][0] = 1;
  e->king_tropism_vals[dragon_horse][1] = 1;
  e->king_tropism_vals[rook][0] = 0.1;
  e->king_tropism_vals[rook][1] = 1;
  e->king_tropism_vals[bishop][0] = 0;
  e->king_tropism_vals[bishop][1] = 1;
  e->king_tropism_vals[kirin][0] = 1;
  e->king_tropism_vals[kirin][1] = 10;
  e->king_tropism_vals[phoenix][0] = 1;
  e->king_tropism_vals[phoenix][1] = 6;
  e->king_tropism_vals[drunk_elephant][0] = 0;
  e->king_tropism_vals[drunk_elephant][1] = 0;
  e->king_tropism_vals[blind_tiger][0] = 0;
  e->king_tropism_vals[blind_tiger][1] = 0;
  e->king_tropism_vals[ferocious_leopard][0] = 0.5;
  e->king_tropism_vals[ferocious_leopard][1] = 2;
  e->king_tropism_vals[gold_general][0] = 1;
  e->king_tropism_vals[gold_general][1] = 5;
  e->king_tropism_vals[silver_general][0] = 1;
  e->king_tropism_vals[silver_general][1] = 3;
  e->king_tropism_vals[copper_general][0] = 0;
  e->king_tropism_vals[copper_general][1] = 3;
  e->king_tropism_vals[vertical_mover][0] = 0;
  e->king_tropism_vals[vertical_mover][1] = 1;
  e->king_tropism_vals[side_mover][0] = 0;
  e->king_tropism_vals[side_mover][1] = 1;

  /* **************************************** */

  /* Opening */
  {
    /* push side pawns, side-movers */
    {
      e->piece_square_bonus[opening][pawn][0][4*12 +  0] += 40;
      e->piece_square_bonus[opening][pawn][0][4*12 + 11] += 40;
      e->piece_square_bonus[opening][side_mover][0][3*12 +  0] += 40;
      e->piece_square_bonus[opening][side_mover][0][3*12 + 11] += 40;

      e->piece_square_bonus[midgame][pawn][0][4*12 +  0] += 15;
      e->piece_square_bonus[midgame][pawn][0][4*12 + 11] += 15;
      e->piece_square_bonus[midgame][side_mover][0][3*12 +  0] += 15;
      e->piece_square_bonus[midgame][side_mover][0][3*12 + 11] += 15;
    }
    /* be conservative with pawn pushes */
    {
      int c;
      for (c=0; c<12; ++c)
      {
        e->piece_square_bonus[opening][pawn][0][5*12 + c] +=  -5;
        e->piece_square_bonus[opening][pawn][0][6*12 + c] += -10;
      }
    }

    /* (1) Lions s.b. in front of pawns & centralized */
    {
      int r, c, ph;
      for (r=0; r<12; ++r)
      {
        for (c=0; c<12; ++c)
        {
          e->piece_square_bonus[opening][lion][0][r*12 + c] -= max(0,3-c)*25 + max(0,c-8)*25 + max(0,4-r)*2 + max(0, r-8)*10;
          e->piece_square_bonus[midgame][lion][0][r*12 + c] -= max(0,3-c)*10 + max(0,c-8)*10 + max(0,4-r)*2 + max(0, r-8)*5;
          e->piece_square_bonus[endgame][lion][0][r*12 + c] -= max(0,3-c)*5 + max(0,c-8)*5 + max(0,4-r)*2 + max(0, r-8)*2;
        }
      }

      /* lion out of back corners */
      for (ph=0; ph<num_phases; ++ph)
      {
        e->piece_square_bonus[ph][lion][0][11*12 +  0] += -100;
        e->piece_square_bonus[ph][lion][0][11*12 +  1] +=  -75;
        e->piece_square_bonus[ph][lion][0][10*12 +  0] +=  -75;
        e->piece_square_bonus[ph][lion][0][10*12 +  1] +=  -50;

        e->piece_square_bonus[ph][lion][0][11*12 + 11] += -100;
        e->piece_square_bonus[ph][lion][0][11*12 + 10] +=  -75;
        e->piece_square_bonus[ph][lion][0][10*12 + 11] +=  -75;
        e->piece_square_bonus[ph][lion][0][10*12 + 10] +=  -50;
      }
      for (r=0; r<3; ++r)
      {
        for (c=0; c<3; ++c)
        {
          e->piece_square_bonus[opening][lion][0][(11-r)*12 +    c] += -100;
          e->piece_square_bonus[opening][lion][0][(11-r)*12 + 11-c] += -100;
          e->piece_square_bonus[midgame][lion][0][(11-r)*12 +    c] += -100;
          e->piece_square_bonus[midgame][lion][0][(11-r)*12 + 11-c] += -100;
          e->piece_square_bonus[endgame][lion][0][(11-r)*12 +    c] +=  -50;
          e->piece_square_bonus[endgame][lion][0][(11-r)*12 + 11-c] +=  -50;
        }
      }
    }

    /* early-game lion bonus */
    {
      int i;
      for (i=0; i<144; ++i)
        e->piece_square_bonus[opening][lion][0][i] += 200;
      for (i=0; i<144; ++i)
        e->piece_square_bonus[midgame][lion][0][i] +=  50;
    }

    /* (4) king s.b. in back with G, 2BT, DE for defence */
    {
      int ph;
      for (ph=0; ph<num_phases; ++ph)
      {
        e->piece_square_bonus[ph][king][0][0]  += -15;
        e->piece_square_bonus[ph][king][0][1]  +=  -7;
        e->piece_square_bonus[ph][king][0][2]  +=  -3;
        e->piece_square_bonus[ph][king][0][3]  +=  -1;
        e->piece_square_bonus[ph][king][0][4]  +=   2;
        e->piece_square_bonus[ph][king][0][5]  +=  25;
        e->piece_square_bonus[ph][king][0][6]  +=  15;
        e->piece_square_bonus[ph][king][0][7]  +=   2;
        e->piece_square_bonus[ph][king][0][8]  +=  -1;
        e->piece_square_bonus[ph][king][0][9]  +=  -3;
        e->piece_square_bonus[ph][king][0][10] +=  -7;
        e->piece_square_bonus[ph][king][0][11] += -15;
      }
      e->piece_square_bonus[opening][drunk_elephant][0][6] += 5;
      e->piece_square_bonus[opening][drunk_elephant][0][1*12+5] += 15;
      e->piece_square_bonus[opening][drunk_elephant][0][1*12+6] += 5;
      e->piece_square_bonus[opening][blind_tiger][0][4] += 15;
      e->piece_square_bonus[opening][blind_tiger][0][6] += 15;

      e->piece_square_bonus[midgame][drunk_elephant][0][6] += 2;
      e->piece_square_bonus[midgame][drunk_elephant][0][1*12+5] += 7;
      e->piece_square_bonus[midgame][drunk_elephant][0][1*12+6] += 2;
      e->piece_square_bonus[midgame][blind_tiger][0][4] += 7;
      e->piece_square_bonus[midgame][blind_tiger][0][6] += 7;

      int r, c;
      for (r=1; r<12; ++r)
      {
        for (c=0; c<12; ++c)
        {
          e->piece_square_bonus[opening][king][0][r*12 + c] += -50;
          e->piece_square_bonus[midgame][king][0][r*12 + c] += -20;
        }
      }
    }

    /* (5) long-range pieces should be in the back with open lines */
    {
      int c, r;
      for (c=0; c<12; ++c)
      {
        e->piece_square_bonus[opening][free_king  ][0][0*12 + c] += 30;
        e->piece_square_bonus[opening][dragon_king][0][0*12 + c] += 20;
        e->piece_square_bonus[opening][bishop     ][0][0*12 + c] += 10;

        e->piece_square_bonus[midgame][free_king  ][0][0*12 + c] += 30/2;
        e->piece_square_bonus[midgame][dragon_king][0][0*12 + c] += 20/2;
        e->piece_square_bonus[midgame][bishop     ][0][0*12 + c] += 10/2;

        e->piece_square_bonus[opening][free_king  ][0][1*12 + c] += 25/3;
        e->piece_square_bonus[opening][dragon_king][0][1*12 + c] += 20;
        e->piece_square_bonus[opening][bishop     ][0][1*12 + c] += 15;

        e->piece_square_bonus[midgame][free_king  ][0][1*12 + c] += 25/3/2;
        e->piece_square_bonus[midgame][dragon_king][0][1*12 + c] += 20/2;
        e->piece_square_bonus[midgame][bishop     ][0][1*12 + c] += 15/2;

        for (r=4; r<12; ++r)
        {
          e->piece_square_bonus[opening][free_king][0][r*12 + c] -= 10;
          e->piece_square_bonus[midgame][free_king][0][r*12 + c] -=  5;
        }
      }
    }

    /* (9) Phoenix at 8i/5d joseki */
    e->piece_square_bonus[opening][phoenix][0][3*12 + 4] += 50;
    e->piece_square_bonus[midgame][phoenix][0][3*12 + 4] += 25;
    e->piece_square_bonus[opening][pawn][0][4*12 + 4] += 25;
    e->piece_square_bonus[midgame][pawn][0][4*12 + 4] += 10;

    /* (12) develop copper/silver towards center, not flanks */
    {
      int r, c;
      for (r=0; r<12; ++r)
      {
        for (c=0; c<12; ++c)
        {
          e->piece_square_bonus[opening][copper_general][0][r*12 + c] -= fabs(5.5 - c);
          e->piece_square_bonus[midgame][copper_general][0][r*12 + c] -= fabs(5.5 - c)/2;
          e->piece_square_bonus[opening][silver_general][0][r*12 + c] -= fabs(5.5 - c);
          e->piece_square_bonus[midgame][silver_general][0][r*12 + c] -= fabs(5.5 - c)/2;
        }
      }
    }

    /* push sm from starting square */
    e->piece_square_bonus[opening][side_mover][0][2*12 +  0] += -20;
    e->piece_square_bonus[midgame][side_mover][0][2*12 +  0] += -10;
    e->piece_square_bonus[opening][side_mover][0][2*12 + 11] += -20;
    e->piece_square_bonus[midgame][side_mover][0][2*12 + 11] += -10;
  }

  /* Middlegame */
  {
    /* push side pawns... */
    {
      int r;
      for (r=0; r<8; ++r)
      {
        e->piece_square_bonus[midgame][pawn][0][r*12 +  0] += (r - 3)*(10 - (r - 3))/2;
        e->piece_square_bonus[midgame][pawn][0][r*12 + 11] += (r - 3)*(10 - (r - 3))/2;
      }
    }

    /* (14) try to promote a VM */
    {
      int r, c;
      for (r=0; r<8; ++r)
        for (c=0; c<12; ++c)
          e->piece_square_bonus[midgame][vertical_mover][0][r*12 + c] += max(-1,(r - 3)) * 2;
    }

    /* (16) phoenixes should not advance past 4th rank unless a clear path to promotion */
    {
      int r, c;
      for (r=4; r<12; ++r)
        for (c=0; c<12; ++c)
          e->piece_square_bonus[midgame][phoenix][0][r*12 + c] += -(r - 3);
    }
  }
  /* Endgame */
  {
    /* push unpromoted step-movers ... */
    {
      piece ip;
      int r, c;
      for (ip=1; ip<num_piece; ++ip)
      {
        if (ip==drunk_elephant) continue;
        if (!(PROMOTION_MASK & (1<<ip))) continue;  // only promotable pieces
        if (piece_move_type[white][ip][0]&0xFF0000) continue;  // only step (1 or 2) movers
        const double factor = min(20, (e->piece_vals[ip][1] - e->piece_vals[ip][0])/3/(ip==pawn ? 1.5 : 2));
        for (r=0; r<8; ++r)
        {
          for (c=0; c<12; ++c)
          {
            if (ip!=kirin && ip!=phoenix && ip!=blind_tiger)
              e->piece_square_bonus[midgame][ip][0][r*12 + c] += (r - 4) * factor/2 - fabs(5.5 - c)/6;
            e->piece_square_bonus[endgame][ip][0][r*12 + c] += ((r - 4) * factor - fabs(5.5 - c)/3)/(ip==blind_tiger ? 2 : 1);
          }
        }
      }
    }
    /* mild drift to centralize major pieces */
    {
      int r, c;
      for (r=0; r<12; ++r)
      {
        for (c=0; c<12; ++c)
        {
          e->piece_square_bonus[endgame][free_king   ][0][r*12 + c] += -(fabs(5.5 - r) + fabs(5.5 - c)) / 3;
          e->piece_square_bonus[endgame][bishop      ][0][r*12 + c] += -(fabs(5.5 - r) + fabs(5.5 - c)) / 3;
          e->piece_square_bonus[endgame][bishop      ][1][r*12 + c] += -(fabs(5.5 - r) + fabs(5.5 - c)) / 3;
          e->piece_square_bonus[endgame][rook        ][0][r*12 + c] += -(fabs(5.5 - r) + fabs(5.5 - c)) / 3;
          e->piece_square_bonus[endgame][rook        ][1][r*12 + c] += -(fabs(5.5 - r) + fabs(5.5 - c)) / 3;
          e->piece_square_bonus[endgame][dragon_horse][0][r*12 + c] += -(fabs(5.5 - r) + fabs(5.5 - c)) / 3;
          e->piece_square_bonus[endgame][dragon_horse][1][r*12 + c] += -(fabs(5.5 - r) + fabs(5.5 - c)) / 3;
          e->piece_square_bonus[endgame][dragon_king ][0][r*12 + c] += -(fabs(5.5 - r) + fabs(5.5 - c)) / 3;
          e->piece_square_bonus[endgame][dragon_king ][1][r*12 + c] += -(fabs(5.5 - r) + fabs(5.5 - c)) / 3;
        }
      }
    }
  }

  return 0;
}

/* ************************************************************ */

int renormalize_evaluator(evaluator* e)
{
  if (0)
  {
    int i;
    const double pawn_scale = 100.0 / e->piece_vals[pawn][0];

    for (i=0; i<NUM_WEIGHTS; ++i)
    {
      e->weights[i] = (evalt)(e->weights[i] * pawn_scale);
      //if (fabs(e->weights[i]) < 1e-10) e->weights[i] = 0;
    }
  }
  else if (0)
  {
    e->piece_vals[pawn][0] = 100;
  }

  /* piece-square bonus should be on-average 0 */
  if (0)
  {
    piece ip;
    for (ip=1; ip<num_piece; ++ip)
    {
      int j;
      for (j=0; j<=1; ++j)
      {
        int ph;
        for (ph=0; ph<num_phases; ++ph)
        {
          int i;
          double ave = 0.0;
          for (i=0; i<144; ++i)
            ave += e->piece_square_bonus[ph][ip][j][i];
          ave /= 144;
          for (i=0; i<144; ++i)
            e->piece_square_bonus[ph][ip][j][i] -= ave;
          e->piece_vals[ip][j] += ave/num_phases;
        }
      }
    }
  }

  /* force certain weights to be non-negative */
  {
    piece ip;
    game_phase ph;
    for (ph=0; ph<num_phases; ++ph)
    {
      if (e->mobility_square_count[ph] < 0)       e->mobility_square_count[ph] = 0;
      if (e->mobility_square_control[ph] < 0)     e->mobility_square_control[ph] = 0;
      if (e->mobility_promotion_control[ph] < 0)  e->mobility_promotion_control[ph] = 0;
      if (e->protected_piece[ph] < 0)             e->protected_piece[ph] = 0;
    }
    if (e->king_attacked < 0) e->king_attacked = 0;
    for (ip=1; ip<num_piece; ++ip)
    {
      if (e->king_tropism_vals[ip][0] < 0) e->king_tropism_vals[ip][0] = 0;
      if (e->king_tropism_vals[ip][1] < 0) e->king_tropism_vals[ip][1] = 0;
    }
  }

  return 0;
}

/* ************************************************************ */

int save_evaluator(FILE* f, const evaluator* e)
{
  int i;
  fprintf(f, "%i %i %i\n", 1, 1, 1);
  for (i=0; i<NUM_WEIGHTS; ++i)
  {
    fprintf(f, "  %i", (int)e->weights[i]*100);
    if (!((i+1)%12) || (i==NUM_WEIGHTS-1)) fprintf(f, "\n");
  }
  return 0;
}

int load_evaluator(FILE* f, evaluator* e)
{
  int i;
  fscanf(f, "%i %i %i", &i, &i, &i);
  for (i=0; i<NUM_WEIGHTS; ++i)
  {
    int w;
    fscanf(f, "%i", &w);
    e->weights[i] = w/(evalt)100;
  }
  return 0;
}

int show_evaluator_pieces_only(FILE* f, const evaluator* e)
{
  int i;

  fprintf(f, "piece_vals:{");
  for (i=1; i<num_piece; ++i)
  {
    fprintf(f, "%s(%i;%i) ", piece_names[i], (int)e->piece_vals[i][0], (int)e->piece_vals[i][1]);
    if (!(i%7) && i!=num_piece-1) fprintf(f, "\n");
  }
  fprintf(f, "}\n");

  return 0;
}

int show_evaluator(FILE* f, const evaluator* e)
{
  int i;

  fprintf(f, "piece_vals:{");
  for (i=1; i<num_piece; ++i)
  {
    fprintf(f, "%s(%i;%i) ", piece_names[i], (int)e->piece_vals[i][0], (int)e->piece_vals[i][1]);
    if (!(i%7) && i!=num_piece-1) fprintf(f, "\n");
  }
  fprintf(f, "}\n");

  fprintf(f, "king_tropism_vals:{");
  for (i=1; i<num_piece; ++i)
  {
    fprintf(f, "%s(%i;%i) ", piece_names[i], (int)e->king_tropism_vals[i][0], (int)e->king_tropism_vals[i][1]);
    if (!(i%7) && i!=num_piece-1) fprintf(f, "\n");
  }
  fprintf(f, "}\n");

  fprintf(f, "mobility_square_count:{");
  for(i=0; i<num_phases; ++i) fprintf(f, "%s%i", i?",":"", (int)e->mobility_square_count[i]);
  fprintf(f, "}\n");
  fprintf(f, "mobility_square_control:{");
  for(i=0; i<num_phases; ++i) fprintf(f, "%s%i", i?",":"", (int)e->mobility_square_control[i]);
  fprintf(f, "}\n");
  fprintf(f, "mobility_promotion_control:{");
  for(i=0; i<num_phases; ++i) fprintf(f, "%s%i", i?",":"", (int)e->mobility_promotion_control[i]);
  fprintf(f, "}\n");
  fprintf(f, "protected_piece:{");
  for(i=0; i<num_phases; ++i) fprintf(f, "%s%i", i?",":"", (int)e->protected_piece[i]);
  fprintf(f, "}\n");

  fprintf(f, "lion_protected:{");
  for(i=0; i<4; ++i) fprintf(f, "%s%i", i?",":"", (int)e->lion_protected[i]);
  fprintf(f, "}\n");
  fprintf(f, "lion_free:{");
  for(i=0; i<4; ++i) fprintf(f, "%s%i", i?",":"", (int)e->lion_free[i]);
  fprintf(f, "}\n");

  fprintf(f, "king_attacked:{%i}\n", (int)e->king_attacked);
  fprintf(f, "king_safe:{");
  for(i=0; i<4; ++i) fprintf(f, "%s%i", i?",":"", (int)e->king_safe[i]);
  fprintf(f, "}\n");

  fprintf(f, "piece_square_bonus:{\n");
  for (i=1; i<num_piece; ++i)
  {
    int r, c, ph;
    fprintf(f, "%s:{\n", piece_names[i]);
    for (ph=0; ph<num_phases; ++ph)
    {
      fprintf(f, "  {\n");
      for (r=11; r>=0; --r)
      {
        fprintf(f, "  ");
        for (c=0; c<12; ++c)
          fprintf(f, " %4i", (int)e->piece_square_bonus[ph][i][0][r*12+c]);
        fprintf(f, "  ");
        for (c=0; c<12; ++c)
          fprintf(f, " %4i", (int)e->piece_square_bonus[ph][i][1][r*12+c]);
        fprintf(f, "\n");
      }
      fprintf(f, "  }\n");
    }
    fprintf(f, "}\n");
  }

  return 0;
}

