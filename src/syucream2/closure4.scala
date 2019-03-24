import java.io.{ByteArrayOutputStream, ObjectOutputStream}
object Closure4 {
  def main(args: Array[String]) {
    val myClosure = new MyClosure
    DefaultSerializer.serialize(myClosure.serializable)  // => passed!
    DefaultSerializer.serialize(myClosure.notSerializable)  // => failed...
} }
class MyClosure {
  import MyClosure._
  private val value = 1
  def notSerializable = () => value
  def serializable = () => valueInObject
}
object MyClosure {
  val valueInObject = 1
}
object DefaultSerializer {
  def serialize(data: AnyRef): Array[Byte] = {
    val buffer = new ByteArrayOutputStream()
    val outStream = new ObjectOutputStream(buffer)
    outStream.writeObject(data)
    buffer.toByteArray
  }
}
