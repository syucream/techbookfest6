object Closure3 {
  def main(args: Array[String]) {
    val one = 1
    val logger = MyLogger()
    val plusOne = { n: Int =>
      logger.print("plusOne() was called.")
      n + one
    }
    println(plusOne(1))  // => 2
  }
}

case class MyLogger() {
  def print(message: String): Unit =
    println(message)
}
