# triepack — Java

Native Java implementation of the Triepack `.trp` binary format.

Reads and writes the same bytes as the C reference library and every other
Triepack implementation, checked by the [shared conformance
suite](https://github.com/deftio/triepack/tree/main/tests/conformance).

## Use

```java
import com.deftio.triepack.*;

Map<String, TpValue> data = new LinkedHashMap<>();
data.put("hello", TpValue.ofUInt(42));
data.put("world", TpValue.ofString("foo"));

byte[] buf = TriePack.encode(data);
Map<String, TpValue> result = TriePack.decode(buf);
```

## Build and test

```bash
gradle build
gradle test
```

## Links

- [Documentation](https://deftio.github.io/triepack/)
- [API reference](https://deftio.github.io/triepack/guide/api-reference/)
- [Source and issues](https://github.com/deftio/triepack)

## License

Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
