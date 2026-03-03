---
layout: default
title: Release Process
---

# Release Process

<!-- Copyright (c) 2026 M. A. Chatterjee -->

## Versioning Policy

TriePack follows [Semantic Versioning](https://semver.org/):

- **MAJOR** -- incompatible API or binary format changes
- **MINOR** -- new features, backward-compatible
- **PATCH** -- bug fixes, no API changes

The version is defined in `CMakeLists.txt` via `project(triepack VERSION x.y.z)`.

## Release Checklist

Every dot release **must** update the three version-tracking files. Skipping
any of these causes the releases page and changelog to fall out of sync.

1. **Update version** in the top-level `CMakeLists.txt`:
   ```cmake
   project(triepack VERSION x.y.z LANGUAGES C CXX)
   ```
2. **Update `docs/_config.yml`** site version:
   ```yaml
   version: "x.y.z"
   ```
3. **Update `CHANGELOG.md`** -- add a new section at the top with the version,
   date, and summary of changes. Follow the
   [Keep a Changelog](https://keepachangelog.com/) format.
4. **Update `docs/releases.md`** -- add a matching entry at the top of the
   releases list. This is the public-facing release notes page on the docs site.
5. **Run the full test suite** including coverage:
   ```bash
   cmake -B build -DBUILD_TESTS=ON -DBUILD_JSON=ON -DBUILD_EXAMPLES=ON
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```
6. **Run Python tests** (if Python binding code was touched):
   ```bash
   cd bindings/python && pip install -e ".[test]" && pytest -v
   ```
7. **Run clang-format** on any modified C/C++ files:
   ```bash
   xcrun clang-format -i <modified-files>
   ```
8. **Verify documentation builds** cleanly:
   ```bash
   cmake -B build -DBUILD_DOCS=ON
   cmake --build build --target docs
   ```
9. **Commit** the version bump, changelog, and releases page update together.
10. **Tag** the release:
    ```bash
    git tag -a vx.y.z -m "Release vx.y.z"
    ```
11. **Push** the tag:
    ```bash
    git push origin main --tags
    ```

## Files That Must Be Updated Per Release

| File | What to change |
|------|----------------|
| `CMakeLists.txt` | `project(triepack VERSION x.y.z ...)` |
| `docs/_config.yml` | `version: "x.y.z"` |
| `CHANGELOG.md` | New `## [x.y.z] - YYYY-MM-DD` section at top |
| `docs/releases.md` | New `## vx.y.z -- YYYY-MM-DD` section at top |

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
