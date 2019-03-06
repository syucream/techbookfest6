object Closure2 {
  def main(args: Array[String]) {
    val one = 1
    val plusOne = { n: Int => n + one }
    println(plusOne(1))  // => 2
  }
}
