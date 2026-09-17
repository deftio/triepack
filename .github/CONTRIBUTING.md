# Contributing to TriePack

Thanks for your interest. TriePack is a binary format with a C reference
library and eight native bindings, so the thing that matters most here is that
every implementation agrees byte for byte. Most of this document is about how
that is kept true.

## Getting started

```bash
git clone https://github.com/<your-username>/triepack.git
cd triepack
git checkout -b fix/short-description
```

Branches are named `feature/<short-description>` or `fix/<short-description>`.
`main` is the production branch and is only ever updated by squash-merge from
a reviewed PR.

## Building and testing

The C library and the C++ wrapper:

```bash
cmake -B build -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Each binding has its own suite:

| Binding | Command (from `bindings/<name>`) |
|---|---|
| JavaScript | `npm ci && npm test` |
| TypeScript | `npm ci && npx tsc --noEmit && npm test` |
| Python | `python -m pytest` |
| Go | `go vet ./... && go test ./...` |
| Rust | `cargo test` |
| Swift | `swift test` |
| Java | `gradle test` |
| Kotlin | `gradle test` |

To run everything at once, exactly as a release would:

```bash
./scripts/make-release.sh --check
```

That builds every target, runs every suite, and checks the conformance corpus
and version strings. It makes no git changes. If a toolchain is missing it
fails rather than skipping silently; `--skip go,rust` opts out explicitly.

## The conformance corpus

`tests/conformance/` holds one case list that all nine implementations run.
For every case, each implementation must decode the C-generated fixture to the
same values *and* re-encode it to the same bytes.

If you change anything about the format, the encoder or a decoder, this is the
suite that decides whether you got it right everywhere. Adding a case means
editing the generator, not the generated files:

```bash
python3 tools/gen_conformance_cases.py       # rewrite cases.txt
cmake --build build --target fixtures        # rewrite fixtures/
python3 tools/gen_malformed_fixtures.py      # rewrite malformed/
```

Commit the regenerated files; CI fails if they are not what the generators
produce. See [`tests/conformance/README.md`](../tests/conformance/README.md).

## Changing behaviour in one binding

Don't, unless the behaviour is genuinely language-specific. A fix to the trie
walk or the value encoding belongs in all nine implementations, with the same
reasoning in the comments. If a binding cannot match the others — JavaScript
holds integers in a double, for instance — say so in the code and give the
corpus case a flag rather than quietly diverging.

## Code style

**C and C++** are formatted with clang-format using the project's
`.clang-format`, and the build is expected to be warning-free:

```bash
find include src wrapper/src wrapper/include \
  -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i
```

CI also runs `clang-tidy` over `src/`.

**Go** must satisfy `gofmt -l .` (empty output) and `go vet`. Other bindings
follow the conventions already visible in their sources.

Across all languages: match the file you are editing. Comments should explain
why something is the way it is, not restate the code.

## Tests

- Every bug fix comes with a regression test that fails before the fix.
- Python and JavaScript hold 100% line coverage; C/C++ holds 99.5% of lines
  and 100% of functions. Do not reduce either — CI fails below 97% lines or
  80% branches. `./tools/check-coverage.sh` checks the C side.
- Anything that parses a buffer needs to survive corruption, not just reject
  it. Build with `-DENABLE_SANITIZERS=ON` and run the suite before sending a
  change that touches the decoder or the bitstream.
- A format-level change needs a conformance case, not just a unit test.

## Versioning

`triepack-version.txt` is the single source of truth. Never edit a version in a
package manifest by hand:

```bash
./scripts/sync_version.sh           # propagate
./scripts/sync_version.sh --check   # verify; CI runs this
```

The on-disk *format* version (`TP_VERSION_MAJOR` / `TP_VERSION_MINOR`) is
separate and changes only when the bytes change.

## Pull requests

1. Rebase on `main` (`git fetch origin && git rebase origin/main`).
2. Make sure the suites above pass, and coverage has not dropped.
3. Push and open a PR against `main` with a title under 70 characters.
4. Fill in the PR template: what changed, why, and how to verify it.
5. Respond to review with new commits rather than force-pushes, so the
   discussion stays readable. The maintainer squash-merges.

Commits that land on `main` carry the maintainer's sign-off and no one else's.
When triepack breaks, that is who gets called, and the history should say so.
Do not add `Co-Authored-By:` trailers for tools or assistants; the release
script strips them from the squash message if any slip through.

## What gets accepted

- Bug fixes with a regression test
- Features on the roadmap in [README.md](../README.md)
- Documentation fixes, including small ones
- Performance work backed by a benchmark

## What does not

- Code without tests
- Changes that drop coverage
- Breaking changes to a public API without discussion first
- A fix applied to one binding that silently leaves the others wrong

## Reporting bugs

Open an issue with a minimal reproduction, the affected language and version,
and your OS and toolchain versions. For the C library, the compiler and its
version matter. If a `.trp` buffer is involved, a hex dump or the key set that
produces it is the fastest route to a fix.

Security issues go through [SECURITY.md](../SECURITY.md), not the public
tracker.

## License

By contributing you agree that your contributions are licensed under the
project's BSD-2-Clause license (see [LICENSE.txt](../LICENSE.txt)).
