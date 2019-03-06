= Scala とクロージャと分散処理

== はじめに

Scala はオブジェクト指向と関数型のマルチパラダイムなプログラミング言語です。
JVM を動作ターゲットプラットフォームとしており既存の Java の資産を活用することができます。
Scala は近年よく見られるパターンマッチ機構や null 安全に評価可能な型を持っていたり、 implicit など独自の仕組みを持っていたりかなりリッチな言語であるといえます。
また Spark など分散処理フレームワークで処理を記述するのにもしばしば使われます。

本記事では特に分散処理において Scala で処理を記述する時に生じる、クロージャとそれが Serializable であることを担保する際に生じる課題についてつらつらと記述します。

== Scala のクロージャ

クロージャ自体は Scala に限定した機構ではなくて、例えば JavaScript などに明るい方ならなじみが深いと思います。
ざっくりいうと、自由変数を参照可能な無名関数です。

まず自由変数の参照のない簡素なクロージャを示してみます。

//source[closure1.scala]{
object Closure1 {
  def main(args: Array[String]) {
    val plusOne = { n: Int => n + 1 }
    println(plusOne(1))  // => 2
  }
}
//}

このコードのうち、クロージャ内の "1" をクロージャの外で定義しても同じ動作をしてくれます。

//source[closure2.scala]{
object Closure2 {
  def main(args: Array[String]) {
    val one = 1
    val plusOne = { n: Int => n + one }
    println(plusOne(1))  // => 2
  }
}
//}

クロージャのこの動作により、定義した関数に明示的に引数として外の変数を渡すなど煩わしいコードを書かなくて済み、記述可能なロジックにも柔軟性が持たせられます。
例えば以下のように一度定義したロガーをクロージャの各処理で引き回したい時などに全体的に見通しが良いコードを記述することができるようになると思われます。

//source[closure3.scala]{
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
//}

== Scala と分散処理

クロージャから視点を少し外し、 Scala と分散処理フレームワークについて考えてみます。
Hadoop エコシステムは多くの場合 JVM を実行環境としており、特に Apache Spark はそれ自体が Scala で記述されていることもあり、しばしば分散処理のコードを Scala で実装することもあるでしょう。

Spark 、あるいは Apache Flink, Apache Beam などで分散処理を記述する場合、各処理が Serializable であることが必要になります。
これは各処理をシリアライズして各ワーカに配布して分散処理が可能にするためです。
ただしこの Serializable の担保は余程気をつけてコーディングしないとハマることが多々あると思われます。
特に Scala のクロージャのような、自由変数を気軽に参照する場合には、 Serializable にするにはどうすればいいのか、そもそも何が原因で Serializable にならないのかを確認するのが困難になることもあるでしょう。

== Scala のクロージャのシリアライズについて

実際にどういう時にクロージャが Serializable でなくて、その時どのような回避策があるのでしょうか。
ここではいくつかのクロージャの記述方法を比較しながらその動作の差異を確認してみます。

=== Serializable 確認の準備

実際のクロージャを使ったコードを記述する前に、 Serializable かどうかをチェックするためのコードと、分散処理で発生しうるコレクションに対する map 処理の準備をしてみます。

