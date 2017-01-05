import Element.elem
abstract class Element(
    val contents: Array[String],
    val height: Int,
    val width: Int
    ) {
  def above(that: Element): Element = {
    val this1 = this widen that.width
    val that1 = that widen this.width
    val height = this1.height + that1.height
    val width = this1.width
    elem(this1.contents ++ that1.contents, height, width)
  }
  def beside(that: Element): Element = {
    val this1 = this heighten that.height
    val that1 = that heighten this.height
    val height = this1.height
    val width = this1.width + that1.width
    elem(
      (for ( (line1, line2) <- this1.contents zip that1.contents )
        yield line1 + line2),
      height,
      width
    )
  }
  def widen(w: Int): Element = {
    if (w <= width) this
    else {
      val left = elem(' ', height, (w - width) / 2)
      val right = elem(' ', height, w - width - left.width)
      left beside this beside right
    }
  }
  def heighten(h: Int): Element = {
    if (h <= height) this
    else {
      val top = elem(' ', (h - height) / 2, width)
      val bot = elem(' ', h - height - top.height, width)
      top above this above bot
    }
  }
  override def toString = contents mkString "\n"
}
object Element {
  // internals:
  private class ArrayElement(contents: Array[String], height: Int, width: Int)
      extends Element(contents,
                      if (width==0) 0 else height,
                      if (height==0) 0 else width) {
    def this(contents: Array[String]) =
      this(contents, contents.length,
           if (contents.length==0) 0 else contents(0).length)
  }
  private class LineElement(s: String, height: Int, width: Int)
      extends ArrayElement(Array(s), height, width) {
    def this(s: String) = this(s, 1, s.length)
  }
  private class UniformElement(
    ch: Char,
    height: Int,
    width: Int
  ) extends Element(Array.make(height, ch.toString * width), height, width)
  // factory members:
  def elem(contents: Array[String]): Element =
    new ArrayElement(contents)
  def elem(contents: Array[String], height: Int, width: Int): Element =
    new ArrayElement(contents, height, width)
  def elem(chr: Char, height: Int, width: Int): Element =
    new UniformElement(chr, height, width)
  def elem(line: String): Element =
    new LineElement(line)
  def elem(line: String, width: Int): Element =
    new LineElement(line, 1, width)
}
//static const char* normal = "\033[0m";
//static const char* bg_squares[] = {
//  "\033[47m", "\033[44m", "\033[43m", "\033[43m" , "\033[42m", "\033[42m", };
////static const char* fg_colors[] = { "\033[31m", "\033[30m", "\033[37m", };
//static const char* fg_colors[] = { "\033[31;1m", "\033[30;1m", "\033[35m", };


import Element.elem
object Spiral {
  val space = elem(" ")
  val corner = elem("\033[43m"+"+"+"\033[0m", "+".length)
  def spiral(nEdges: Int, direction: Int): Element = {
    if (nEdges == 1) elem("*")
    else {
      val spir = spiral(nEdges-1, (direction + 3) % 4)
      def vBar = elem('|', spir.height, 1)
      def hBar = elem('-', 1, spir.width)
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
