object Closure1 {
  def main(args: Array[String]) {
    val plusOne = { n: Int => n + 1 }
    println(plusOne(1))  // => 2
  }
}
