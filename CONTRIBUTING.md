# Contributing to TriePack

Thanks for taking the time. This page is about *this* repository's workflow
— the build, the tests, and the two rules that are stricter than you might
expect.

By participating you agree to the [Code of Conduct](CODE_OF_CONDUCT.md).
Security issues go through [SECURITY.md](SECURITY.md), not the issue tracker.

## The two rules that matter most

**1. The format is a contract across ten implementations.** The C library and
nine bindings must produce byte-identical output for the same input, and all
ten are checked against one shared corpus in `tests/conformance/`. A change to
the encoder is a change to every binding's expected output. If you are
changing what the bytes look like, say so in the pull request — that is a
format change, not a refactor, and it has to go through
[the North Star](docs/triepack-northstar.md) §4.

**2. Claims in the docs have to be true.** `docs/status.md` exists because the
gap between what this project documented and what it implemented was once
large and undocumented. If you add a feature flag, an enum value or a struct
field that nothing reads, that is a defect. If you write a number in a
document, the command that produced it belongs next to it.

## Building and testing

You need CMake 3.15+ and a C99 compiler. Everything else is optional and only
needed for the binding you are touching.

```bash
cmake -B build -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

**The build must produce zero warnings.** CI fails on any, and gcc is
stricter than Apple clang, so a clean build on macOS is not proof. Check with
a container before opening a pull request if you can:

```bash
./scripts/test-ci-linux.sh c            # the gcc build and the warning gate
./scripts/test-ci-linux.sh sanitizers   # ASan, UBSan and LeakSanitizer
./scripts/test-ci-linux.sh coverage     # lcov against the CI floors
```

LeakSanitizer only exists on Linux, so the leak half of the sanitizer job
cannot run on macOS at all. That is not a formality: the memory leak fixed in
1.3.2 had been exercised by an existing test for a long time with nothing
watching.

The containers mount this repository rather than copying it, so a container
build can leave artifacts the host then trips over. The Swift job builds into
`/tmp` for exactly that reason — if you add a job, give it a scratch path
outside the tree.

### Bindings

Each is self-contained and runs from its own directory:

```bash
cd bindings/python     && python3 -m pytest -q
cd bindings/javascript && npm ci && npx jest
cd bindings/typescript && npm ci && npx tsc --noEmit && npx jest
cd bindings/rust       && cargo test
cd bindings/go         && go vet ./... && go test ./...
cd bindings/swift      && swift test
cd bindings/java       && ./gradlew test
cd bindings/kotlin     && ./gradlew test
```

`./scripts/test-jvm.sh` downloads a toolchain for the JVM ones if you do not
have one.

### The conformance corpus

The bindings check themselves against fixtures generated from the C library,
so those files have to be exactly what the generators produce from the
current source. If you change the encoder, regenerate and commit the result:

```bash
python3 tools/gen_conformance_cases.py
cmake --build build --target fixtures
python3 tools/gen_malformed_fixtures.py
```

CI fails if `git diff` shows anything after that. See
`tests/conformance/README.md`.

## Style

C is formatted by `.clang-format`; CI checks it and will tell you to run it.

```bash
find src include tests examples -name '*.c' -o -name '*.h' | xargs clang-format -i
```

`clang-tidy` runs in CI as well. Beyond that:

- **C99.** Not C11, not GNU extensions. `strdup` is not C99 — gcc will
  implicitly declare it as returning `int`, truncate the pointer and segfault
  on Linux while clang on macOS says nothing.
- Comments explain *why*, not *what*. Match the density of the file you are
  in.
- New code needs tests. Branch coverage floors are enforced in CI; see
  `.github/workflows/coverage.yml` for the current numbers.

## Writing tests

Tests are written from the specification, not from the implementation. A test
that reads the code and asserts what it already does will pass forever and
find nothing.

The tests worth writing are the ones that could embarrass you. Every grammar
contradiction and every over-allocation bug found in this repository was
found by a randomised round-trip, a sanitizer or a differential test against
a second implementation — none by inspection, and none by a worked example.

## Pull requests

1. Branch from `main`.
2. Keep it to one thing. A format change and a refactor in one diff cannot be
   reviewed.
3. Make sure the build is warning-free and `ctest` passes.
4. If behaviour changed, add the entry to `CHANGELOG.md` under
   `## [Unreleased]`. `docs/releases.md` is generated from it by
   `./scripts/sync_changelog.sh` — do not edit it by hand.
5. Describe what you measured, if you measured anything. "Faster" is not a
   result; "2,445 → 2,656 MB/s on 15 MB of Wikipedia, `make bench`" is.

Releases are cut by a maintainer with `./scripts/make-release.sh`; see
[RELEASE.md](RELEASE.md).

## terseml

[`terseml/`](terseml/) is a sibling subproject, not part of the TriePack
library. It links nothing from TriePack, TriePack links nothing from it, and
it is not published to any registry. It has its own build:

```bash
make -C terseml check
```

Its grammar is the authority over its implementations — if
[`terseml/GRAMMAR.md`](terseml/GRAMMAR.md) and the code disagree, the code is
the bug. Changing the wire format means regenerating the shared corpus
(`make -C terseml vectors`) and updating all three implementations in the
same pull request.

## Reporting bugs

Open an issue with the version, OS and toolchain, a minimal reproduction, and
what you expected instead. For a decoding problem, the `.trp` buffer as a hex
dump or base64 is worth more than a description of it.

If it looks like a security problem, do not open an issue —
[SECURITY.md](SECURITY.md) has the private path.
