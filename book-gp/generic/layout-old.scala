import Element.elem
abstract class Element {
  def contents: Array[String]
  def height: Int = contents.length
  def width: Int = if (height==0) 0 else contents(0).length
  def above(that: Element): Element = {
    val this1 = this widen that.width
    val that1 = that widen this.width
    elem(this1.contents ++ that1.contents)
  }
  def beside(that: Element): Element = {
    val this1 = this heighten that.height
    val that1 = that heighten this.height
    elem(
      for ( (line1, line2) <- this1.contents zip that1.contents )
        yield line1 + line2
    )
  }
  def widen(w: Int): Element =
    if (w <= width) this
    else {
      val left = elem(' ', (w - width) / 2, height)
      val right = elem(' ', w - width - left.width, height)
      left beside this beside right
    }
  def heighten(h: Int): Element =
    if (h <= height) this
    else {
      val top = elem(' ', width, (h - height) / 2)
      val bot = elem(' ', width, h - height - top.height)
      top above this above bot
    }
  override def toString = contents mkString "\n"
}
object Element {
  private class ArrayElement(
    val contents: Array[String]
  ) extends Element
  private class LineElement(s: String) extends ArrayElement(Array(s)) {
    override def width = s.length + 1
    override def height = 1
  }
  private class SpecialLineElement(s: String, w: Int) extends
      ArrayElement(Array(s)) {
    override def width = w
    override def height = 1
  }
  private class UniformElement(
    ch: Char,
    override val width: Int,
    override val height: Int
  ) extends Element {
    private val line = ch.toString * width
    def contents = Array.make(height, line)
  }
  def elem(contents: Array[String]): Element =
    new ArrayElement(contents)
  def elem(chr: Char, width: Int, height: Int): Element =
    new UniformElement(chr, width, height)
  def elem(line: String): Element =
    new LineElement(line)
  def elem(pre: String, line: String, post: String): Element =
    new SpecialLineElement(pre+line+post, line.length)
}
//static const char* normal = "\033[0m";
//static const char* bg_squares[] = {
//  "\033[47m", "\033[44m", "\033[43m", "\033[43m" , "\033[42m", "\033[42m", };
////static const char* fg_colors[] = { "\033[31m", "\033[30m", "\033[37m", };
//static const char* fg_colors[] = { "\033[31;1m", "\033[30;1m", "\033[35m", };


import Element.elem
object Spiral {
  val space = elem(" ")
  val corner = elem("\033[47m", "+", "\033[0m")
  println(corner.height, corner.width)
  def spiral(nEdges: Int, direction: Int): Element = {
    if (nEdges == 1) elem("*")
    else {
      val spir = spiral(nEdges-1, (direction + 3) % 4)
      def vBar = elem('|', 1, spir.height)
      def hBar = elem('-', spir.width, 1)
      if (direction == 0)
        (corner beside hBar) above(spir beside space)
      else if (direction == 1)
        (spir above space) beside (corner above vBar)
      else if (direction == 2)
        (space beside spir) above (hBar beside corner)
      else
        (vBar above corner) beside (space above spir)
    }
  }
  def main(args: Array[String]) {
    val ns = args(0).toInt
    println(spiral(ns, 0))
  }
}
