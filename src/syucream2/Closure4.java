import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.function.Supplier;

class Closure4 {
  public static void main(String[] args) {
    MyClosure myClosure = new MyClosure();

    try {
      DefaultSerializer.serialize(myClosure.serializable());
      DefaultSerializer.serialize(myClosure.notSerializable());
    } catch (Exception e) {
      e.printStackTrace();
    }
  }
}

interface SerializableSupplier<T> extends Supplier<T>, Serializable {};

class MyClosure {
  private final int value = 1;

  public SerializableSupplier<Integer> notSerializable() {
    return () -> {
      return this.value;
    };
  }

  public SerializableSupplier<Integer> serializable() {
    return () -> {
      return MyClosureObject.getInstance().valueInObject();
    };
  }
}

final class MyClosureObject {
  private static MyClosureObject instance;
  private int valueInObject = 1;

  static {
    instance = new MyClosureObject();
  }

  public static MyClosureObject getInstance() {
    return instance;
  }

  public int valueInObject() {
    return instance.valueInObject;
  }
}

class DefaultSerializer {
  public static byte[] serialize(Object data) throws IOException {
    ByteArrayOutputStream buffer = new ByteArrayOutputStream();
    ObjectOutputStream outStream = new ObjectOutputStream(buffer);
    outStream.writeObject(data);

    return buffer.toByteArray();
  }
}
