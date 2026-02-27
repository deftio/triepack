# Release Process

<!-- Copyright (c) 2026 M. A. Chatterjee -->

## Versioning Policy

triepack follows [Semantic Versioning](https://semver.org/):

- **MAJOR** -- incompatible API or binary format changes
- **MINOR** -- new features, backward-compatible
- **PATCH** -- bug fixes, no API changes

The version is defined in `CMakeLists.txt` via `project(triepack VERSION x.y.z)`.

## Release Checklist

1. **Update version** in the top-level `CMakeLists.txt`.
2. **Update CHANGELOG.md** with a summary of changes under the new version heading.
3. **Run the full test suite** including coverage:
   ```bash
   cmake -B build -DENABLE_COVERAGE=ON
   cmake --build build
   ctest --test-dir build
   ```
4. **Verify documentation builds** cleanly:
   ```bash
   cmake -B build -DBUILD_DOCS=ON
   cmake --build build --target docs
   ```
5. **Commit** the version bump and changelog update.
6. **Tag** the release:
   ```bash
   git tag -a v1.2.3 -m "Release v1.2.3"
   ```
7. **Push** the tag:
   ```bash
   git push origin main --tags
   ```

## CI/CD Automation

The CI pipeline runs on every push and pull request:

- **Build matrix:** Linux (gcc, clang), macOS (AppleClang), 32-bit and 64-bit
- **Test:** `ctest` with `--output-on-failure`
- **Coverage:** uploaded on tagged releases and main branch builds
- **Docs:** Doxygen HTML generated and deployed on tagged releases

When a version tag (`v*`) is pushed, the pipeline additionally:

- Creates a GitHub Release with auto-generated notes
- Attaches source tarballs
- Publishes Doxygen docs to GitHub Pages
