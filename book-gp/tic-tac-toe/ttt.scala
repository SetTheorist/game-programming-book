////////// ////////// ////////// //////////
class TTTGame extends Game {
  type GameBoard = TTTBoard
  type GameEvaluator = TTTEvaluator
  type Move = Int
  var evaluator = new TTTEvaluator
  var board = new TTTBoard
  class TTTEvaluator extends Evaluator {
    override def apply(b : TTTBoard) = Value.Draw
  }
  class TTTBoard extends Board {
    var b : Int = 0
    var s : Side = XX
    override def ToMove() = s
    override def Moves() : List[Move] = {
      var l : List[Move] = Nil
      for (i <- 0 to 8)
        if ((b&(3<<(2*i)))==0)
          l = i::l
      return l
    }
    override def MakeMove(m : Move) : Boolean = {
      b |= (s.I << (2*m))
      s = s.Swap
      return true
    }
    override def UnMakeMove(m : Move) : Boolean = {
      b &= (~(3 << (2*m)))
      s = s.Swap
      return true
    }
    def X3(n : Int) : Boolean = {
      if ((n&21)==21) return true
      if ((n&4161)==4161) return true
      if ((n&65793)==65793) return true
      if ((n&1344)==1344) return true
      if ((n&86016)==86016) return true
      if ((n&16644)==16644) return true
      if ((n&4368)==4368) return true
      if ((n&66576)==66576) return true
      return false
    }
    override def Result() : Value = {
      val x = X3(b)
      val o = X3(b>>1)
      if (x && o) { println("!"); return Value.Invalid }
      if (x) return Value.X_Win
      if (o) return Value.O_Win
      return 0
    }
    override def Terminal() : Boolean = {
      if (X3(b) || X3(b>>1)) return true
      for (i <- 0 to 8)
        if ((b&(3<<(2*i)))==0)
          return false
      return true
    }
    override def Hash() : Long = b
  }
}


object Runner {
  def main(args: Array[String]) {
    var g = new TTTGame

    //g.board.MakeMove(4);
    //g.board.MakeMove(5);

    //Minimax.nodes = 0
    //val r_mm = Minimax.Search(g, 10)
    //println("Minimax  ", r_mm, Minimax.nodes)

    Negamax.nodes = 0
    for (i <- 1 to 100) {
      var g = new TTTGame
      Negamax.Search(g, 11)
    }
    println("Negamax  ", Negamax.nodes)

    //Alphabeta.nodes = 0
    //val r_ab = Alphabeta.Search(g, 10)
    //println("Alphabeta", r_ab, Alphabeta.nodes)
  }
}
