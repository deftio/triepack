## What and why

<!-- What changed, and the problem it solves. Link the issue: Fixes #123 -->

## How to verify

<!-- The command a reviewer runs to see it work, and what they should see. -->

```bash

```

## Checklist

- [ ] Tests added — a bug fix has a regression test that fails without the fix
- [ ] `./scripts/make-release.sh --check` passes, or the affected suites do
- [ ] Coverage has not dropped (C/C++, Python and JavaScript are at 100%)
- [ ] C/C++ formatted with clang-format; Go passes `gofmt -l .` and `go vet`
- [ ] Versions untouched by hand (`triepack-version.txt` is the source of truth)

### If this touches the format, the encoder or a decoder

- [ ] The change is applied to **every** implementation, not just one
- [ ] A conformance case covers it — generators rerun and the regenerated
      `tests/conformance/` files committed
- [ ] Any deliberate difference between languages is explained in the code

### If this is a breaking change

- [ ] Called out here, with the migration path
- [ ] `CHANGELOG.md` updated
