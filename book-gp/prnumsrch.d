import std.algorithm;
import std.stdio;

////////////////////////////////////////////////////////////

enum Color : ubyte
{
  None = 0,
  White = 1,
  Black = 2,
}

static immutable int N = 3;

struct Board
{
  bool is_terminal = false;
  Color to_move = Color.White;
  Color result = Color.None;

  Color[N*N] cells;

  int num_moves;
  ubyte[64] move_stack;

  Color winset(int start, int delta) {
    if (cells[start]==Color.None) return Color.None;
    for (int i=1; i<N; ++i)
      if (cells[start+i*delta] != cells[start])
        return Color.None;
    return cells[start];
  }
  bool check_terminal() {
    Color winner = Color.None;
    is_terminal = false;

    if (winner==Color.None)
      winner = winset(0,  N+1);
    if (winner==Color.None)
      winner = winset(N-1, N-1);
    for (int i=0; i<N; ++i)
      if (winner==Color.None)
        winner = winset(i, N);
    for (int i=0; i<N; ++i)
      if (winner==Color.None)
        winner = winset(i*N, 1);
    
    if (num_moves==N*N || winner!=Color.None) {
      is_terminal = true;
      result = winner;
    }

    return is_terminal;
  }
  bool make_move(ubyte m) {
    cells[m] = to_move;
    to_move ^= 3;
    move_stack[num_moves++] = m;
    check_terminal();
    return false;
  }
  bool unmake_move() {
    is_terminal = false;
    to_move ^= 3;
    cells[move_stack[--num_moves]] = Color.None;
    return false;
  }
  ubyte[] gen_moves() { 
    ubyte[] ml = [];
    for (int i=0; i<N*N; ++i)
    {
      if (cells[i]==Color.None)
      {
        ++ml.length;
        ml[ml.length-1] = cast(ubyte)i;
      }
    }
    return ml;
  }
  string fen() { return "xxx"; }
}

////////////////////////////////////////////////////////////

enum : ubyte
{
  f_internal = 1,
  f_maxnode = 2,
  f_terminal = 4,
  f_win = 8,
}

struct Node
{
  ubyte premove;
  ubyte flags;
  uint pn;
  uint dp;
  Node* parent;
  Node*[] children;
}

Node* update(Board* b, Node* v)
{
  do {
    if (v.flags & f_maxnode) {
      uint pn = 1000000;
      uint dp = 0;
      foreach (Node* child; v.children) {
        pn = min(pn, child.pn);
        dp += child.dp;
      }
      dp = min(dp, 1000000);
      v.pn = pn;
      v.dp = dp;
    } else {
      uint pn = 0;
      uint dp = 1000000;
      foreach (Node* child; v.children) {
        pn += child.pn;
        dp = min(dp, child.dp);
      }
      pn = min(pn, 1000000);
      v.pn = pn;
      v.dp = dp;
    }
    v = v.parent;
    if (v)
      b.unmake_move();
  } while (v);
  return v;
}

Node* determine_most_proving_node(Board* b, Node* v)
{
  while (v.flags & f_internal) {
    Node* old_v = v;
    //writef("(%X)", v);
    if (v.flags & f_maxnode) {
      foreach (Node* child; v.children) {
        if (child.pn == v.pn) {
          v = child;
          b.make_move(v.premove);
          break;
        }
      }
    } else {
      foreach (Node* child; v.children) {
        if (child.dp == v.dp) {
          v = child;
          b.make_move(v.premove);
          break;
        }
      }
    }
    if (v == old_v) { stdin.flush(); throw new Exception("oops"); }
  }
  //writefln("(%X)", v);
  return v;
}

Node* expand(Board* b, Node* v)
{
  int i;
  ubyte[]  ml = b.gen_moves();
  v.children.length = ml.length;
  for (i=0; i<ml.length; ++i)
  {
    v.children[i] = new_node(v);
    v.children[i].premove = ml[i];
    b.make_move(ml[i]);
    v.children[i].flags |= (b.to_move==Color.White) ? f_maxnode : 0;
    if (b.is_terminal)
    {
      v.children[i].flags |= f_terminal;
      //if (b.result == Color.White)
      if (b.result != Color.Black)
        v.children[i].flags |= f_win;
    }
    b.unmake_move();
  }
  v.flags |= f_internal;
  
  foreach (Node* child; v.children) {
    if (child.flags & f_terminal) {
      if (child.flags & f_win) {
        child.pn = 0;
        child.dp = 1000000;
      } else {
        child.pn = 1000000;
        child.dp = 0;
      }
    }
  }
  return v;
}

Node* new_node(Node* parent)
{
  Node* n = new Node;
  n.pn = 1;
  n.dp = 1;
  n.flags = 0;
  n.parent = parent;
  n.children = [];
  return n;
}

void show(Node* n) { show_rec(n, 0); }
void show_rec(Node* n, int id)
{
  writef("%*s", id, "");
  if (n.pn==1000000) writef(" oo"); else writef("%3u", n.pn);
  writef("|");
  if (n.dp==1000000) writef("oo "); else writef("%-3u", n.dp);
  writef("  %c%c%c%c  <%X>\n",
    n.flags & f_internal ? 'I' : '-',
    n.flags & f_maxnode  ? 'M' : '-',
    n.flags & f_terminal ? 'T' : '-',
    n.flags & f_win      ? 'W' : '-',
    n
    );

  foreach (Node* child; n.children)
    show_rec(child, id+4);
}

int main() {
  Board b;
  writeln("Hello");
  Node* n = new_node(null);
  n.flags |= b.to_move==Color.White ? f_maxnode : 0;
  uint count = 0;
  //show(n);
  //writefln("Count : %u\n", count);
  while (n.pn!=0 && n.dp!=0 && count<10000000) {
    ++count;
    Node* mp = determine_most_proving_node(&b, n);
    //writeln(b.fen());
    //show(mp);
    expand(&b, mp);
    update(&b, mp);
    //show(n);
    if (count%1000 == 1) {
      writefln("%u|%-u  Count:%u", n.pn, n.dp, count);
      stdout.flush();
    }
  }
  writefln("%u|%-u  Count:%u", n.pn, n.dp, count);
  if (n.pn == 0)
    writeln("Won");
  else if (n.dp == 0)
    writeln("Lost");
  else
    writeln("*aborted*");

  return 0;
}
