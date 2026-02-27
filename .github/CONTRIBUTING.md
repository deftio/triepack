# Contributing to TXZ

Thank you for your interest in contributing to TXZ! This document outlines the
process and guidelines for contributing.

## Getting Started

1. **Fork** the repository on GitHub.
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/<your-username>/txz.git
   cd txz
   ```
3. **Create a branch** for your work:
   ```bash
   git checkout -b feature/my-feature
   ```

## Building

```bash
cmake -B build -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Code Style

- All C/C++ code must be formatted with **clang-format** using the project's
  `.clang-format` configuration.
- Run before committing:
  ```bash
  clang-format -i src/*.c include/txz/*.h
  ```
- Keep functions short and focused.
- Use clear, descriptive names. Prefer `txz_` prefixed names for public API.

## Testing

- **100% test coverage is mandatory.** Every new feature or bug fix must include
  tests that cover all code paths.
- Run the coverage check locally:
  ```bash
  ./tools/check-coverage.sh
  ```
- Tests use the Unity test framework (included in the project).

## Pull Request Process

1. Ensure your branch is up to date with `main`:
   ```bash
   git fetch origin
   git rebase origin/main
   ```
2. Verify all tests pass and coverage is at 100%.
3. Run `clang-format` on all modified files.
4. Push your branch and open a Pull Request against `main`.
5. Fill in the PR template with:
   - A clear description of what changed and why.
   - How to test the changes.
6. Address any review feedback promptly.

## What We Accept

- Bug fixes with regression tests.
- New features that align with the project roadmap.
- Documentation improvements.
- Performance improvements with benchmarks.

## What We Do Not Accept

- Breaking changes to the public API without prior discussion.
- Code without tests.
- Changes that reduce test coverage below 100%.

## Reporting Issues

- Use GitHub Issues for bug reports and feature requests.
- Include a minimal reproduction case for bugs.
- Specify your OS, compiler, and compiler version.

## License

By contributing, you agree that your contributions will be licensed under the
same license as the project (see [LICENSE.txt](../LICENSE.txt)).
