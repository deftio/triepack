# triepack — Kotlin

Native Kotlin implementation of the Triepack `.trp` binary format.

Reads and writes the same bytes as the C reference library and every other
Triepack implementation, checked by the [shared conformance
suite](https://github.com/deftio/triepack/tree/main/tests/conformance).

## Use

```kotlin
import com.deftio.triepack.*

val data = mapOf<String, TpValue?>(
    "hello" to TpValue.UInt(42),
    "world" to TpValue.Str("foo"),
)

val buf = encode(data)
val result = decode(buf)
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
