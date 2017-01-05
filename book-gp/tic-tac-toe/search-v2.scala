sealed abstract class Side {
  def Swap: Side
  def I: Int
}
case object XX extends Side {
  override def Swap = OO
  override def I = 1
}
case object OO extends Side {
  override def Swap = XX
  override def I = 2
}
sealed case class Value(v: Int) {
  def < (a: Value): Boolean = (v < a.v)
  def <=(a: Value): Boolean = (v <= a.v)
  def > (a: Value): Boolean = (v > a.v)
  def >=(a: Value): Boolean = (v >= a.v)
  def ==(a: Value): Boolean = (v == a.v)
  def !=(a: Value): Boolean = (v != a.v)
  def step() = if ((v<= -900) && (v>= -1000)) Value(v+1) else if ((v>=900) && (v<=1000)) Value(v-1) else this
  def unary_-(): Value = if ((v<=1001) && (v>= -1001)) Value(-v) else this
  override def toString =
    if (v==0) "<Draw>"
    else if (v==99900) "<Unknown>"
    else if (v==99901) "<Invalid>"
    else if (v > 900) "<X-mate-in-"+(1000-v)+">"
    else if (v < -900) "<O-mate-in-"+(v+1000)+">"
    else "<"+v+">"
}
object Value {
  implicit def IntegerToValue(n: Int): Value = Value(n)
  def Draw: Value = Value(0)
  val X_Inf = Value(1001)
  val X_Win = Value(1000)
  def X_WinIn(n: Int) = Value(1000 - n)
  val O_Inf = Value(-1001)
  val O_Win = Value(-1000)
  def O_WinIn(n: Int) = Value(-1000 + n)
  val Unknown = Value(99900)
  val Invalid = Value(99901)
}

abstract class Game {
  type GameBoard <: Board
  type Move
  abstract class Board {
    def ToMove(): Side
    def Moves(): List[Move]
    def MakeMove(m: Move): Boolean
    def UnMakeMove(m: Move): Boolean
    def Result(): Value
    def Terminal(): Boolean
    def Hash(): Long
  }
  var board: GameBoard
}
abstract class Evaluator[G <: Game] extends (G#GameBoard => Value)

/*
class TranspositionTable(num: Int) {
  class Entry(
    var lock: Long,
    var upper: Value,
    var lower: Value,
    var flags: Int
  )
  val tt  = new Array[Entry](num)
}
*/

////////// ////////// ////////// //////////
abstract class Searcher {
  def Search[G <: Game](g: G, depth: Int, eval: Evaluator[G]): Value
}
object Minimax extends Searcher {
  var nodes = 0
  def Search[G <: Game](g: G, depth: Int, eval: Evaluator[G]): Value = {
    nodes += 1
    if (depth==0) return eval(g.board)
    if (g.board.Terminal()) return g.board.Result()
    g.board.ToMove() match {
      case XX => {
        var best_v = Value.O_Inf
        for (m <- g.board.Moves()) {
          g.board.MakeMove(m)
          var res = Search(g, depth-1, eval)
          g.board.UnMakeMove(m)
          res = res.step
          if (res > best_v) best_v = res
        }
        return best_v
      }
      case OO => {
        var best_v = Value.X_Inf
        for (m <- g.board.Moves()) {
          g.board.MakeMove(m)
          var res = Search(g, depth-1, eval)
          g.board.UnMakeMove(m)
          res = res.step
          if (res < best_v) best_v = res
        }
        return best_v
      }
    }
  }
}
object Negamax extends Searcher {
  var nodes = 0
  def Search[G <: Game](g: G, depth: Int, eval: Evaluator[G]): Value = {
    nodes += 1
    if (depth==0)
      return (if (g.board.ToMove()==XX) eval(g.board) else -eval(g.board))
    if (g.board.Terminal())
      return (if (g.board.ToMove()==XX) g.board.Result() else -g.board.Result())
    var best_v = Value.O_Inf
    for (m <- g.board.Moves()) {
      g.board.MakeMove(m)
      var res = -Search(g, depth-1, eval)
      g.board.UnMakeMove(m)
      res = res.step
      if (res > best_v) best_v = res
    }
    return best_v
  }
}
object Alphabeta extends Searcher {
  var nodes = 0
  def Search[G <: Game](g: G, depth: Int, eval: Evaluator[G]): Value =
    WindowSearch(g, depth, Value.O_Inf, Value.X_Inf, eval)
  def WindowSearch[G <: Game](g:G, depth:Int,
                              alpha:Value, beta:Value,
                              eval: Evaluator[G]): Value = {
    nodes += 1
    if (depth==0)
      return (if (g.board.ToMove()==XX) eval(g.board) else -eval(g.board))
    if (g.board.Terminal)
      return (if (g.board.ToMove()==XX) g.board.Result else -g.board.Result)
    var nalpha = alpha
    for (m <- g.board.Moves()) {
      g.board.MakeMove(m)
      var res = -WindowSearch(g, depth-1, -beta, -nalpha, eval)
      g.board.UnMakeMove(m)
      res = res.step
      if (res > nalpha) nalpha = res
      if (nalpha>=beta) return nalpha
    }
    return nalpha
  }
}
