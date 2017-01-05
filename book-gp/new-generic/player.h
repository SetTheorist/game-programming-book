#ifndef INCLUDED_PLAYER__H_
#define INCLUDED_PLAYER__H_

#include <list>
#include <map>
#include <vector>
#include "gen-search.h"
#include "util.h"

enum special_move {
  regular_move, no_moves, resign, offer_draw, accept_draw, adjourn
};

template<typename Game>
struct Ply
{
  typedef typename Game::boardt boardt;
  typedef typename Game::movet movet;
  typedef typename Game::evalt evalt;
  typedef typename Game::colort colort;

  colort  side;
  evalt   evaluation;
  movet   move;
  int     pv_length;
  movet   pv[64];
  ui64    time_spent;
  ui64    nodes_searched;
  ui64    qnodes_searched;
  ui64    nodes_evaluated;
  ui64    qnodes_evaluated;
  special_move  sm;
  char    annotation[64];

  int showf(FILE* ff) const {
    int n=0;
    n += fstatus<movet,evalt>(ff, "[%c  %m  ({%q} %M) %.2fs %lluns/%lluqs/%llune/%lluqe \"%.64s\"]",
      (side==Game::first ? 'F' : side==Game::second ? 'S' : '?'),
      move,
      evaluation,
      pv, pv_length,
      (double)time_spent/1000000.0,
      nodes_searched, qnodes_searched, nodes_evaluated, qnodes_evaluated,
      annotation
      );
    return n;
  }
};


template<typename Game>
struct Player {
  typedef typename Game::boardt boardt;
  typedef typename Game::movet movet;
  typedef typename Game::evalt evalt;
  typedef typename Game::evaluatort evaluatort;

  enum {
    random_player, interactive_player, search_player
  } player_type;

  boardt  board;

  Player() : player_type(search_player), engine(100,0,64,64)
  { 
  }

  //////////////////////////////////////////
  //
  // Random Player
  //
  unsigned long long y;
  // Marsaglia prng
  unsigned long long rnd() { y^=y<<13; y^=y>>7; y^=y<<17; return y; }
  void RandomPlayer(const boardt& initial_board, unsigned long long seed=88172645463325252ULL)
  {
    player_type = random_player;
    board = initial_board;
    y = seed ? seed : 1;
    rnd(); rnd(); rnd();
  }
  special_move Think_random(boardt& b, Ply<Game>& outply) {
    evalt z(0);
    movet ml[boardt::MAXNMOVES];
    int n = b.gen_moves(ml);
    if (n==0) {outply.sm = no_moves; return no_moves; }
    int mi = rnd() % n;
    outply.side = b.to_move;
    outply.evaluation = z;
    outply.move = ml[mi];
    outply.pv_length = 1;
    outply.pv[0] = ml[mi];
    outply.nodes_searched = 0;
    outply.qnodes_searched = 0;
    outply.nodes_evaluated = 0;
    outply.qnodes_evaluated = 0;
    outply.sm = regular_move;
    outply.annotation[0] = '\x00';
    return regular_move;
  }
  //
  //
  //
  //////////////////////////////////////////

  //////////////////////////////////////////
  //
  // Interactive Player
  //
  FILE* fin;
  FILE* fout;
  void InteractivePlayer(const boardt& initial_board, FILE* ffin=stdin, FILE* ffout=stdout)
  {
    player_type = interactive_player;
    board = initial_board;
    fin = ffin;
    fout = ffout;
  }
  special_move Think_interactive(boardt& b, Ply<Game>& outply) {
    evalt z(0);
    movet ml[boardt::MAXNMOVES];
    int n = b.gen_moves(ml);
    b.showf(fout);
    fprintf(fout, "Available moves:\n");
    for (int i=0; i<n; ++i) {
      fstatus<movet,evalt>(fout, "\t(%i) %m", i, ml[i]);
      if (i%5==4) fprintf(fout, "\n");
    }
    if (n%5) fprintf(fout, "\n");
    if (n==0) { fprintf(fout, "*NO MOVES*\n"); outply.sm = no_moves; return no_moves; }
    fprintf(fout, "Enter move #: "); fflush(fout);
    char buff[64];
    fgets(buff, sizeof(buff), fin);
    int mi = abs(atoi(buff)) % n;
    outply.side = b.to_move;
    outply.evaluation = z;
    outply.move = ml[mi];
    outply.pv_length = 1;
    outply.pv[0] = ml[mi];
    outply.nodes_searched = 0;
    outply.qnodes_searched = 0;
    outply.nodes_evaluated = 0;
    outply.qnodes_evaluated = 0;
    outply.sm = regular_move;
    outply.annotation[0] = '\x00';
    return regular_move;
  }
  //
  //
  //
  //////////////////////////////////////////


  //////////////////////////////////////////
  //
  // Interactive Player
  //
  searcher<Game>  engine;
  evaluatort      evaluator;
  void SearchPlayer(const boardt& initial_board, const evaluatort& e, int depth, int qdepth, int ttnum=1024, int qttnum=1024) {
    player_type = search_player;
    board = initial_board;
    engine = searcher<Game>(depth,qdepth,ttnum,qttnum);
    evaluator = e;
  }
  special_move Think_search(boardt& b, Ply<Game>& outply) {
    outply.side = b.to_move;
    outply.evaluation = engine.search(&b, &evaluator, outply.pv, &outply.pv_length);
    outply.move = outply.pv[0];
    outply.nodes_searched = engine.nodes_searched;
    outply.qnodes_searched = engine.qnodes_searched;
    outply.nodes_evaluated = engine.nodes_evaluated;
    outply.qnodes_evaluated = engine.qnodes_evaluated;
    outply.sm = regular_move;
    outply.annotation[0] = '\x00';
    return outply.sm;
  }
  //
  //
  //
  //////////////////////////////////////////

  special_move Think(boardt& b, Ply<Game>& outply) {
    switch (player_type) {
      case random_player: return Think_random(b, outply);
      case interactive_player: return Think_interactive(b, outply);
      case search_player: return Think_search(b, outply);
      default: return no_moves;
    }
  }

  special_move Play(Ply<Game>& outply) {
    ui64 bt = read_clock();
    special_move sm = Think(board, outply);
    ui64 et = read_clock();
    outply.time_spent = et - bt;
    return sm;
  }
};

template<typename Game>
struct GameRecord {
  typedef typename Game::boardt boardt;
  typedef typename Game::movet  movet;
  typedef typename Game::evalt  evalt;
  typedef typename Game::colort colort;

  boardt  initial_board;
  boardt  current_board;
  std::vector< Ply<Game> > move_list;

  Player<Game>* first_player;
  Player<Game>* second_player;

  int     terminal;
  colort  result;

  GameRecord(const boardt& b, Player<Game>* first, Player<Game>* second)
      : initial_board(b), current_board(b), first_player(first), second_player(second)
  {
    move_list.clear();
    terminal = b.terminal();
    result = b.result;
    first_player->board = initial_board;
    second_player->board = initial_board;
  }

  void add_ply(Ply<Game>& ply) {
    move_list.push_back(ply);
    current_board.make_move(ply.move);
    first_player->board.make_move(ply.move);
    second_player->board.make_move(ply.move);
    if (current_board.terminal()) {
      terminal = 1;
      result = current_board.result;
    }
  }

  void showf(FILE* ff) const {
    fprintf(ff, "#============================================================#\n");
    initial_board.showf(ff);
    for (typename std::vector< Ply<Game> >::const_iterator it=move_list.begin(); it!=move_list.end(); ++it) {
      it->showf(ff);
      fprintf(ff, "\n");
    }
    current_board.showf(ff);
    fprintf(ff, "#::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::#\n");
  }

  special_move step() {
    special_move  sm;
    Ply<Game>     ply;
    if      (current_board.to_move==Game::first)  sm = first_player->Play(ply);
    else if (current_board.to_move==Game::second) sm = second_player->Play(ply);
    else                                         return no_moves;
    add_ply(ply);
    return sm;
  }

  int play() {
    while (!current_board.terminal() && current_board.num_moves<256)
      if (step()!=regular_move) break;
    return current_board.terminal();
  }
};

template<typename Game>
struct Tourney
{
  typedef typename Game::boardt boardt;
  typedef typename Game::movet  movet;
  typedef typename Game::evalt  evalt;
  typedef typename Game::colort colort;
  typedef typename std::list< GameRecord<Game> >  game_list;
  typedef typename std::pair<int,int> ipair;

  const boardt initial_board;
  const std::vector<Player<Game>*>  players;
  std::map<ipair,game_list>  results;

  Tourney(const boardt& b, const std::vector<Player<Game>*>& ps)
      : initial_board(b), players(ps)
  {
    Reset();
  }

  void Reset() {
    for (int i=0; i<players.size(); ++i)
      for (int j=0; j<players.size(); ++j)
        results[std::make_pair(i,j)] = std::list<GameRecord<Game> >();
  }

  void Play() {
    for (int i=0; i<players.size(); ++i) {
      for (int j=0; j<players.size(); ++j) {
        if (i==j) continue;
        GameRecord<Game>  gr(initial_board, players[i], players[j]);
        gr.play();
        results[std::make_pair(i,j)].push_back(gr);
        gr.showf(stdout);
      }
    }
  }

  int showf(FILE* ff) const {
    int n = 0;
    for (int i=0; i<players.size(); ++i) {
      for (int j=0; j<players.size(); ++j) {
        if (i==j) {
          n += fprintf(ff, "  --\\--|--");
        } else {
          int result_counts[3] = {0,0,0};
          const game_list& gij = results.find(std::make_pair(i,j))->second;
          for (typename game_list::const_iterator it=gij.begin(); it!=gij.end(); ++it) {
            if      (it->result == Game::first)  ++result_counts[0];
            else if (it->result == Game::second) ++result_counts[1];
            else                                 ++result_counts[2];
          }
          n += fprintf(ff, "  %2i\\%2i|%2i", result_counts[0], result_counts[1], result_counts[2]);
        }
      }
      n += fprintf(ff, "\n");
    }
    return n;
  }
};

#endif/*INCLUDED_PLAYER__H_*/
