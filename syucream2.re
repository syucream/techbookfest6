= Scala とクロージャと分散処理

こんにちは、はじめまして、 @syu_cream です。
シュークリームは、そんなに好きな訳ではありません
この記事では Scala でコーディングした結果をつらつらとお伝えします。


== はじめに

Scala はオブジェクト指向と関数型のマルチパラダイムなプログラミング言語です。
JVM を動作ターゲットプラットフォームとしており既存の Java の資産を活用できます。
また、新し目のプログラミング言語だけあって以下のようなリッチな機能を有しています。

 * パターンマッチ
 * null 安全な型 Option
 * エラーが起こる可能性があることを表現する型 Either
 * 例外が起こる可能性があることを表現する型 Try

また Scala の特徴的な機能として以下も挙げられます。

 * implicit による暗黙の型変換や既存クラスへの機能追加
 * object や case class など多用なクラス定義方法
 * for 構文によるリッチな処理

加えて Scala は Spark など分散処理フレームワークで処理を記述するのにもしばしば使われます。

本記事では特に分散処理において Scala で処理を記述する時に生じる、クロージャとそれが Serializable であることを担保する際に生じる課題についてつらつらと記述します。


== Scala のクロージャ

クロージャ自体は Scala に限定した機構ではなくて、例えば JavaScript などに慣れた方ならなじみが深いでしょう。
ざっくりいうと、自由変数を参照可能な（エンクローズする）無名関数です。

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
例えば以下のように一度定義したロガーをクロージャの各処理で引き回したい時などに全体的に見通しが良いコードを記述できるようになると思われます。

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
Hadoop エコシステムは多くの場合 JVM を実行環境としており、特に Apache Spark @<fn>{apache_spark} はそれ自体が Scala で記述されていることもあり、しばしば分散処理のコードを Scala で実装することもあるでしょう。

Apache Spark, あるいは Apache Beam @<fn>{apache_beam} などで分散処理を記述する場合、各処理が Serializable であることを要求されます。
これは各処理をシリアライズして各ワーカに配布して分散処理が可能にするためです。
ただしこの Serializable の担保は余程気をつけてコーディングしないとハマることが多々あると思われます。
特に Scala のクロージャのような自由変数を気軽に参照する場合には、 Serializable にするにはどうすればいいのか、そもそも何が原因で Serializable にならないのかを確認するのが困難になることもあるでしょう。

//footnote[apache_spark][Apache Spark: https://spark.apache.org/]
//footnote[apache_beam][Apache Beam: https://beam.apache.org/]

===[column] Serializable インタフェースと Scala

不要だと思われますが、 Serializable インタフェースと Scala のクラスについて確認しておきます。
Serializable インタフェースは、単なる「そのクラスはシリアライズ可能だよ」と伝えるためのマークです。
何らかのメソッドの実装を要求したりしません。
シリアライズ対象のオブジェクトのクラス自体が Serializable であることと、そこから参照されるメンバがすべて Serializable であれば、そのオブジェクトが実際にシリアライズできます。

また Scala において case class や case object を使うと、そのクラスは Serializable が自動的に mixin されます。
この挙動は普段あまり意識することが無いかも知れませんが、 Scala で Serializable が要求されるシーンでは重要なノウハウになってきます。

===[/column]


== Scala のクロージャのシリアライズについて

どのような時にクロージャがシリアライズできなくなるのでしょうか。
ここではいくつかのクロージャの記述方法を比較しながらその動作の差異を確認してみます。

=== Serializable 確認の準備

実際のクロージャを使ったコードを記述する前に、 Serializable かどうかをチェックするためのコードを準備します。
ここでは以下の様な簡素な Serializer インタフェースを考えます。

//source[serializer.scala]{
trait Serializer {
  def serialize(data: AnyRef): Array[Byte]
}
//}

実装には Kryo @<fn>{kryo} など既存の効率的なシリアライザを用いてもいいのですが、ここではシンプルに自前で実装しておきます。

//source[default_serializer.scala]{
import java.io.{ByteArrayOutputStream, ObjectOutputStream}

object DefaultSerializer extends Serializer {

  override def serialize(data: AnyRef): Array[Byte] = {
    val buffer = new ByteArrayOutputStream()
    val outStream = new ObjectOutputStream(buffer)
    outStream.writeObject(data)

    buffer.toByteArray
  }

}
//}

もしシリアライズ可能でないクロージャを serialize() に投げ込んだら、例外が起こるはずです。
ここでは Scalatest で例外が起こるかどうかをチェックできるようにしておきます。

//source[assert_serialize.scala]{
import org.scalatest.{FlatSpec, Matchers}
import scala.util.{Failure, Success, Try}
...
class ClosureSpec extends FlatSpec with Matchers {
  ...
  def assertSerializable(closure: AnyRef, serializable: Boolean): Unit = {
    val result = Try(DefaultSerializer.serialize(closure))
    if (serializable) {
      result shouldBe a[Success[_]]
    } else {
      result shouldBe a[Failure[_]]
    }

  }
//}

//footnote[kryo][Kryo: https://github.com/EsotericSoftware/kryo]

=== 簡単なシリアライズ可能な場合

簡単なケースから確認していきましょう。

まず自由変数を何も参照しない、更に何も引数に取らないクロージャを考えます。
これは特に問題なくシリアライズ可能です。

//source[serializable01.scala]{
class ClosureSpec extends FlatSpec with Matchers {
  ...
  it should "serializable" in {
    ...
    val closure = () => 1
    assertSerializable(closure, true)  // passed!
//}

次に引数を取るようにしてみます。
これもシリアライズ可能です。

//source[serializable02.scala]{
class ClosureSpec extends FlatSpec with Matchers {
  ...
  it should "serializable" in {
    ...
    val closure = (x: Int) => x + 1
    assertSerializable(closure, true)  // passed!
//}

今度はラムダの外の自由変数、ただしクロージャを定義したのと同じブロックで宣言されている変数を参照するようにしてみます。
これも問題ありません。

//source[serializable03.scala]{
class ClosureSpec extends FlatSpec with Matchers {
  ...
  it should "serializable" in {
    ...
    val localValue = 1
    val closure = (x: Int) => localValue + 1
    assertSerializable(closure, true)  // passed!
//}

localValue の値を外のブロックで一度定義した後 localValue に代入した場合でも同様です。

//source[serializable04.scala]{
class ClosureSpec extends FlatSpec with Matchers {
  ...
  private val someSerializableValue = 1

  it should "serializable" in {
    ...
    val localValue = someSerializableValue
    val closure = (x: Int) => localValue + 1
    assertSerializable(closure, true)  // passed!
//}

では敢えて Serializable でないクラスを定義して、そのオブジェクトを参照した場合はどうでしょうか？
一見失敗してしまうのでは？と思われるこの例でもシリアライズには成功します。

//source[serializable05.scala]{
class ClosureSpec extends FlatSpec with Matchers {
  ...
  private val someSerializableValue = 1

  it should "serializable" in {
    ...
    val closure = () => new NonSerializable(1)
    assertSerializable(closure, true)  // passed!
    ...
  }
  ...
}


class NonSerializable(id: Int) {  // Serializable を継承していない！
  ...
//}

=== 簡単なシリアライズ不可能なケース

対して、シリアライズ不可能になるのはどんなケースでしょうか？
まずは通りそうで通らないケースから触れてみます。

シリアライズ可能な、 Serializable でないクラスのメンバを参照するクロージャがまずシリアライズ不可能です。
前述の例で localValue として一度同じスコープで再定義した場合にはシリアライズ可能だったことを思い出すと、直感的でないと感じる方もいるかも知れません。

//source[nonserializable01.scala]{
class ClosureSpec extends FlatSpec with Matchers {
  ...
  private val someSerializableValue = 1

  it should "serializable" in {
    ...
    val closure = () => someSerializableValue
    assertSerializable(closure, false)
    ...
//}

このクラスのメンバを参照すると、関数でも Serializable でないクラスのオブジェクトでも、同様にシリアライズ不可能になってしまいます。

//source[nonserializable01.scala]{
class ClosureSpec extends FlatSpec with Matchers {
  ...
  private val someSerializableMethod() = 1
  private val someNonSerializableValue = new NonSerializable(1)
  private val someNonSerializableMethod() = new NonSerializable(1)

  it should "serializable" in {
    ...
    val closure2 = () => someSerializableMethod()
    val closure3 = () => someNonSerializableValue
    val closure4 = () => someNonSerializableMethod()

    assertSerializable(closure2, false)
    assertSerializable(closure3, false)
    assertSerializable(closure4, false)
//}

=== 簡単なシリアライズ不可能なケースの対処法

このシリアライズ不可能なケース、うっかりこうしたコードを書いてしまいそうですね。
手っ取り早いシリアライズ可能にする方法はあるのでしょうか？
ここでは 2 種類の回避方法を紹介します。

１つ目は、可能であれば参照先メンバを Serializable なクラスに持たせてしまうことです。
例えば object で定義したクラスに持たせてしまうなどで対処できます。

//source[to_serializable01.scala]{
class ClosureSpec extends FlatSpec with Matchers {
  ...

  import ClosureSpec._

  it should "serializable" in {
    ...
    val closure06 = () => someSerializableValueInObject
    val closure07 = () => someSerializableMethodInObject()
    val closure08 = () => someNonSerializableValueInObject
    val closure09 = () => someNonSerializableMethodInObject()

    assertSerializable(closure06, true)
    assertSerializable(closure07, true)
    assertSerializable(closure08, true)
    assertSerializable(closure09, true)
    ...
  }
  ...
}

object ClosureSpec {

  private val someSerializableValueInObject = 1
  private val someNonSerializableValueInObject = new NonSerializable
  private def someSerializableMethodInObject() = 1
  private def someNonSerializableMethodInObject() = new NonSerializable

...
//}

2つ目は、 Serializable でないクラスのメンバを直接参照するのをやめてブロック内で再定義しておくケースです。
メンバがもともと Serializable な型であればこれで回避可能です。
例えば以下のようにします。

//source[to_serializable02.scala]{
class ClosureSpec extends FlatSpec with Matchers {
  ...

  import ClosureSpec._

  it should "serializable" in {
    ...
    val someSerializableValueInLocal =  someSerializableValue
    val someSerializableMethodInLocal =  someSerializableMethod()
    val closure1 = () => someSerializableValueInLocal
    val closure2 = () => someSerializableMethodInLocal

    assertSerializable(closure1, true)
    assertSerializable(closure2, true)
  }
  ...
//}

ちなみにここでクラスのメンバを遅延評価するとシリアライズ不可能になってしまいます。

//source[to_nonserializable01.scala]{
class ClosureSpec extends FlatSpec with Matchers {
  ...

  import ClosureSpec._

  it should "serializable" in {
    ...
    // lazy!
    lazy val someSerializableValueInLocal =  someSerializableValue
    lazy val someSerializableMethodInLocal =  someSerializableMethod()
    val closure1 = () => someSerializableValueInLocal
    val closure2 = () => someSerializableMethodInLocal

    assertSerializable(closure1, true)
    assertSerializable(closure2, true)
  }
  ...
//}

=== 複雑なシリアライズ可能なケース

やや複雑な例を提示してみます。
このようにブロックがネストしていたり、クロージャをネストして呼び出していても、個別のブロック、クロージャが Serializable であればシリアライズが可能です。

//source[serializable11.scala]{
class ClosureSpec extends FlatSpec with Matchers {
  ...

  import ClosureSpec._

  it should "serializable" in {
    ...
    val localValue = someSerializableValue
    val closure1 = (x: Int) => x +localValue
    val closure2 = (x: Int, y: Int) => closure1(x) + y
    val closure3 = (x: Int, y: Int, z: Int) => closure2(x, y) + z

    val closure4 = {x: Int =>
      def addOne(v: Int): Int = v + localValue
      addOne(x)
    }

    val closure5 = {(x: Int, y:Int) =>
      val v1 = x + localValue
      val v = {
        val v2 = y + 2
        v1 + v2
      }
      v
    }

    assertSerializable(closure1, true)
    assertSerializable(closure2, true)
    assertSerializable(closure3, true)
    assertSerializable(closure4, true)
    assertSerializable(closure5, true)
//}

=== 複雑なシリアライズ不可能なケース

一方でネストしたブロック、クロージャからシリアライズ不可能な値を参照すると、やはりクロージャ全体がシリアライズ不可能になります。

この例は、本誌のように順序立ててシリアライズ可能性について注意深く検証していればどうしてシリアライズ出来ないのか特定するのは簡単でしょう。
しかしながら実世界で、ある関数を自作していてその後 closure3 のようにそれを呼び出すだけのクロージャを作った上で、シリアライズを要求する処理を組み立てたらどうでしょうか？
コードの複雑性にもよるでしょうが、シリアライズが失敗する原因を特定するのが難しくなる場合もあるでしょう。

//source[serializable12.scala]{
class ClosureSpec extends FlatSpec with Matchers {
  ...

  import ClosureSpec._

  it should "serializable" in {
    ...
    val closure1 = (x: Int) => x + someSerializableValue
    val closure2 = (x: Int, y: Int) => closure1(x) + y
    val closure3 = (x: Int, y: Int, z: Int) => closure2(x, y) + z

    val closure4 = {x: Int =>
      def addOne(v: Int): Int = v + someSerializableValue
      addOne(x)
    }

    val closure5 = {(x: Int, y:Int) =>
      val v1 = x + someSerializableValue
      val v = {
        val v2 = y + 2
        v1 + v2
      }
      v
    }

    assertSerializable(closure1, false)
    assertSerializable(closure2, false)
    assertSerializable(closure3, false)
    assertSerializable(closure4, false)
    assertSerializable(closure5, false)
//}

===[column] Scala 2.12 とクロージャ、そして ClosureCleaner

おそらく現在ひろく使われているであろう Scala 2.12 とその前の Scala 2.11 の間には無名関数における変更が入っています。
主要な点として、無名関数のとる型 FunctionN が SAM(Single Abstract Method, メソッドが 1 つしかない abstract class) となったことが挙げられます。
これにより Java8 と Scala の互換性が高まりました。

加えて、 Scala 2.12 ではクロージャのキャプチャの挙動に以下のような変更が入っています。

 * 使用していない不要な参照をキャプチャするのをやめるようになった
 * ローカルメソッドがインスタンスメンバを参照しない場合に静的なものに変換するようになった

不要な参照を除外することによって、シリアライズ後のバイナリサイズを削減し、予期せぬシリアライズの失敗を避けることができます。
またメソッドはインスタンスメンバへの参照を持つため、シリアライズの失敗を招く可能性があるのですが、これが解消されました。
例えば Scala 2.11 以下ではこのようなローカルメソッドをキャプチャするクロージャのシリアライズには失敗していました。

//source[localdef.scala]{
class ClosureSpec extends FlatSpec with Matchers {
  ...

  import ClosureSpec._

  it should "serializable" in {
    ...
    def a = 1
    val closure = (x: Int) => x + a

    assertSerializable(closure, false)
//}

Apache Spark では以前より、このようなシリアライズに関する問題の緩和策として、 ClosureCleaner @<fn>{closurecleaner} というクロージャをクリンナップする仕組みを設けていました。

//footnote[scala212][Scala 2.12: https://www.scala-lang.org/news/2.12.0/]
//footnote[scala212_lambda_capturing][Scala 2.12 lambda: https://www.scala-lang.org/news/2.12.0/#lambdas-capturing-outer-instances]
//footnote[closurecleaner][ClosureCleaner: https://www.quora.com/Apache-Spark/What-does-Closure-cleaner-func-mean-in-Spark]

===[/column]


== おわりに

本記事で挙げた通り、 Scala でクロージャを書く時シリアライズ可能になるかどうかは、その書き方に依存します。
Scala はリッチな表現が出来るお陰か、注意していないと予期せぬ罠にハマる可能性があるかも知れません。
まずは以下のことを注意すると良いでしょう。

 * クロージャ内から参照する自由変数の定義の場所を意識する
 * シリアライズ可能であることを要求されるコーディングにおいて、可能な限り class でなく case class, object を使ってクラス定義する

余談ですが、筆者は最近 Apache Beam によるストリームデータ処理を行うコードを仕事で記述しています。
そこで Scala でデータ処理記述するにあたり Spotify の Scio @<fn>{scio} を使用しています。
Scio はまるで List や Seq などの標準コレクションを操作するかのように入力データコレクションに対して map, flatMap, filter, reduce などのメソッドが使えて便利なのです。
一方、これらのメソッドに渡すクロージャから Serializable でないクラスのメンバの、ロガーなどを参照するコードを量産してしまってえらくハマってしまった経験があります。
もしみなさんが ETL 処理を Scala で記述する際に、同じ轍を踏まないことを祈っております。
この記事が少しでもそのための役に立てれば幸いです。

//footnote[scio][Scio: https://github.com/spotify/scio]
