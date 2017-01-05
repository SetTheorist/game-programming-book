import std.bitmanip;
import std.stdio;

import std.conv;
import std.format;
import std.range;
import std.string;
//import std.system;
//import std.traits;

enum P
{
  empty, lion, elephant, giraffe, zebra, crocodile, monkey, pawn, superpawn
}
enum C
{
  none, white, black
}
enum S
{
  blank, river, wden, bden
}
enum move_result
{
  invalid_move,
  other_side_moves,
  monkey_move
}

alias hasht = ulong;

union M
{
  struct {
    mixin(bitfields!(
      bool, "white",      1,
      uint, "f",          6,
      uint, "t",          6,
      uint, "piece",      4,
      bool, "f_capture",  1,
      bool, "f_drown",    1,
      bool, "f_nullmove", 1,
      bool, "f_monkey",   1,
      bool, "f_promote",  1,
      bool, "", 10
    ));
  }
  uint i;
  const void toString(scope void delegate(const(char)[]) sink, FormatSpec!char fmt) const
  {
    switch(fmt.spec) {
//      case 'i':
//        return (cast(int)i).toString(sink, fmt);
//      case 'b': {
//        char[] ch = format!(char)(fmt, cast(uint)i);
//        sink.put(ch);
//        return;
//      }
      case 's':
        sink.put("<");
        sink.put(white ? "W" : "B");
        if (f_nullmove) {
          sink.put("pass");
        } else {
          sink.put(to!string(to!P(piece)));
          sink.put(to!string(f));
          sink.put(f_capture ? "x" : "-");
          sink.put(to!string(t));
          if (f_promote) sink.put("+");
        }
        if (f_monkey) sink.put("*");
        if (f_drown) sink.put("!");
        sink.put(">");
        return;
      default:
        throw new Exception("Unhandled format specifier: %" ~ fmt.spec);
    }
  }
}

struct board
{
  P       pieces[7*7];
  C       colors[7*7];
  C       side;
  C       result;
  hasht   hash;
  // TODO: use Array type...
  int     ply;
  M[]     move_stack;
  hasht[] hash_stack;
  int     num_captures;
  P[]     capture_stack;
  int     num_drowns;
  P[]     drown_stack;
  int     num_monkeys;
  P[]     monkey_stack;

  int gen_moves(M[] ml) {
    return 0;
  }
  move_result make_move(M m) {
    return move_result.invalid_move;
  }
  move_result unmake_move() {
    return move_result.invalid_move;
  }

  static immutable S[7*7] squares = [
    S.blank, S.blank, S.wden, S.wden, S.wden, S.blank, S.blank, 
    S.blank, S.blank, S.wden, S.wden, S.wden, S.blank, S.blank, 
    S.blank, S.blank, S.wden, S.wden, S.wden, S.blank, S.blank, 
    S.river, S.river, S.river, S.river, S.river, S.river, S.river,
    S.blank, S.blank, S.bden, S.bden, S.bden, S.blank, S.blank, 
    S.blank, S.blank, S.bden, S.bden, S.bden, S.blank, S.blank, 
    S.blank, S.blank, S.bden, S.bden, S.bden, S.blank, S.blank, 
    ];
  static immutable int[7*7] board_to_box = [
    24, 25, 26, 27, 28, 29, 30,
    35, 36, 37, 38, 39, 40, 41,
    46, 47, 48, 49, 50, 51, 52,
    57, 58, 59, 60, 61, 62, 63,
    68, 69, 70, 71, 72, 73, 74,
    79, 80, 81, 82, 83, 84, 85,
    90, 91, 92, 93, 94, 95, 96,
    ];
  static immutable int[(7+4)*(7+4)] box_to_board = [
    -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
    -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
    -1,  -1,   0,   1,   2,   3,   4,   5,   6,  -1,  -1,
    -1,  -1,   7,   8,   9,  10,  11,  12,  13,  -1,  -1,
    -1,  -1,  14,  15,  16,  17,  18,  19,  20,  -1,  -1,
    -1,  -1,  21,  22,  23,  24,  25,  26,  27,  -1,  -1,
    -1,  -1,  28,  29,  30,  31,  32,  33,  34,  -1,  -1,
    -1,  -1,  35,  36,  37,  38,  39,  40,  41,  -1,  -1,
    -1,  -1,  42,  43,  44,  45,  46,  47,  48,  -1,  -1,
    -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
    -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
    ];
  static immutable string[3] piece_char = [
    "?????????",
    "-LEGZCMPS",
    "_legzcmps",
    ];

  void initialize() {
    pieces = [
      P.giraffe, P.monkey, P.elephant, P.lion, P.elephant, P.crocodile, P.zebra,
      P.pawn, P.pawn, P.pawn, P.pawn, P.pawn, P.pawn, P.pawn,
      P.empty, P.empty, P.empty, P.empty, P.empty, P.empty, P.empty,
      P.empty, P.empty, P.empty, P.empty, P.empty, P.empty, P.empty,
      P.empty, P.empty, P.empty, P.empty, P.empty, P.empty, P.empty,
      P.pawn, P.pawn, P.pawn, P.pawn, P.pawn, P.pawn, P.pawn,
      P.giraffe, P.monkey, P.elephant, P.lion, P.elephant, P.crocodile, P.zebra,
      ];
    colors = [
      C.white, C.white, C.white, C.white, C.white, C.white, C.white,
      C.white, C.white, C.white, C.white, C.white, C.white, C.white,
      C.none, C.none, C.none, C.none, C.none, C.none, C.none,
      C.none, C.none, C.none, C.none, C.none, C.none, C.none,
      C.none, C.none, C.none, C.none, C.none, C.none, C.none,
      C.black, C.black, C.black, C.black, C.black, C.black, C.black,
      C.black, C.black, C.black, C.black, C.black, C.black, C.black,
      ];
    side = C.white;
    result = C.none;
    ply = 0;
    num_captures = 0;
    num_drowns = 0;
    num_monkeys = 0;
  }
  string to_fen() {
    auto app = appender!string();
    for (int r=6; r>=0; --r) { 
      for (int c=0; c<7; ++c) {
        if (colors[r*7+c]==C.none) {
          int n = 0;
          while (c<7 && colors[r*7+c]==C.none)
            ++n, ++c;
          app.put(cast(char)('0'+n));
        } else {
          app.put(piece_char[colors[r*7+c]][pieces[r*7+c]]);
        }
      }
      if (r>0) app.put('/');
    }
    app.put(' ');
    app.put(side==C.white ? 'W' : side==C.black ? 'B' : '-');
    app.put(' ');
    return app.data;
  }
  void from_fen(string fen) {
  }

  void show() {
  }

  bool terminal() {
    return false;
  }
  bool repetition() {
    return false;
  }
}


int main() {
  M m;
  writefln("%s", M.sizeof);
  m.f_nullmove = true;
  writefln("%032b = %032b = %s", m.i, m.i, m);
  m.f_nullmove = false;
  m.white = true;
  m.piece = P.lion;
  m.f = 7;
  m.t = 2;
  m.f_drown = true;
  writefln("%032b = %032b = %s", m.i, m.i, m);
  auto x = [m,m,m];
  writefln("%s", x);

  board b;
  b.initialize();
  writefln("%s", b.to_fen());
  return 0;
}
