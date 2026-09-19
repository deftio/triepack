#!/usr/bin/env bash
#
# test-ci-linux.sh — run the CI jobs that only exist on Linux, locally.
#
# scripts/make-release.sh covers every target on this machine, but CI runs on
# ubuntu, and some things differ enough to matter:
#
#   gcc     the zero-warning gate is stricter than Apple clang. gcc 13 warns
#           about format truncation that clang says nothing about, and ci.yml
#           fails the build on any warning.
#   Swift   Linux uses swift-corelibs-foundation, not Apple's Foundation.
#   asan    LeakSanitizer only exists on Linux, so the leak half of the
#           sanitizer job cannot run on this machine at all.
#   cov     lcov on Linux reads real gcov data; on macOS it needs an llvm-cov
#           shim, and the two disagree often enough to be worth checking.
#
# Needs a running container runtime. Nothing is installed on the host and the
# images are the only thing cached.
#
#   ./scripts/test-ci-linux.sh             # everything
#   ./scripts/test-ci-linux.sh c           # the gcc build, ctest, zero-warning gate
#   ./scripts/test-ci-linux.sh swift
#   ./scripts/test-ci-linux.sh sanitizers  # ASan + UBSan + LeakSanitizer
#   ./scripts/test-ci-linux.sh coverage    # lcov totals against the CI floors
#
# Exit status: 0 if the selected jobs pass, 1 otherwise, 2 on usage error.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

if [[ -t 1 ]]; then
    GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; BOLD='\033[1m'; NC='\033[0m'
else
    GREEN=''; YELLOW=''; RED=''; BOLD=''; NC=''
fi

TARGET="${1:-all}"
case "${TARGET}" in
    all|c|swift|sanitizers|coverage) ;;
    -h|--help)
        sed -n '3,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
        exit 0
        ;;
    *)
        echo "Usage: $0 [all|c|swift|sanitizers|coverage]" >&2
        exit 2
        ;;
esac

ok()   { echo -e "  ${GREEN}pass${NC}  $*"; }
die()  { echo -e "${RED}${BOLD}error:${NC} $*" >&2; exit 1; }
step() { echo -e "\n${BOLD}$*${NC}"; }

RUNTIME=""
for candidate in docker podman; do
    if command -v "${candidate}" >/dev/null 2>&1 && "${candidate}" info >/dev/null 2>&1; then
        RUNTIME="${candidate}"
        break
    fi
done
[[ -n "${RUNTIME}" ]] || die "no running container runtime (tried docker, podman). Start one, or rely on CI for these jobs."
ok "using ${RUNTIME}"

# ---------------------------------------------------------------------------
# C / C++ under gcc, including the zero-warning gate ci.yml enforces
# ---------------------------------------------------------------------------
if [[ "${TARGET}" == "c" || "${TARGET}" == "all" ]]; then
    step "C / C++ on ubuntu (gcc)"
    "${RUNTIME}" run --rm -v "${PROJECT_ROOT}":/src -w /src ubuntu:24.04 bash -c '
        set -eo pipefail
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq > /dev/null 2>&1
        # git is needed: the test suite fetches Unity through FetchContent.
        apt-get install -y -qq gcc g++ cmake python3 git ca-certificates > /dev/null 2>&1
        echo "  $(gcc --version | head -1)"
        rm -rf /tmp/ci-build
        cmake -S . -B /tmp/ci-build -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON > /dev/null 2>&1
        cmake --build /tmp/ci-build -j"$(nproc)" > /tmp/ci-build.log 2>&1 || {
            tail -30 /tmp/ci-build.log; exit 1; }
        if grep -iE "warning:" /tmp/ci-build.log | grep -v "In file included"; then
            echo "  zero-warning gate FAILED"
            exit 1
        fi
        echo "  zero-warning gate passed"
        ctest --test-dir /tmp/ci-build 2>&1 | tail -3
    ' || die "the gcc job failed"
    ok "C / C++ on gcc"
fi

# ---------------------------------------------------------------------------
# Swift on Linux (swift-corelibs-foundation, not Apple Foundation)
# ---------------------------------------------------------------------------
if [[ "${TARGET}" == "swift" || "${TARGET}" == "all" ]]; then
    step "Swift on Linux"
    # The container mounts the repository, so it writes the same
    # bindings/swift/.build the host uses. Leaving Linux artifacts there makes
    # the next `swift test` on macOS fail with "command ... not registered",
    # which looks like a broken binding and is not one. Build somewhere else.
    "${RUNTIME}" run --rm -v "${PROJECT_ROOT}":/src -w /src/bindings/swift swift:5.10-jammy bash -c '
        set -eo pipefail
        swift --version 2>&1 | head -1 | sed "s/^/  /"
        swift test --scratch-path /tmp/swift-build 2>&1 \
            | grep -E "Executed [0-9]+ tests" | tail -1 | sed "s/^/  /"
    ' || die "the Swift Linux job failed"
    ok "Swift on Linux"
fi

# ---------------------------------------------------------------------------
# Sanitizers — the job in ci.yml. LeakSanitizer is Linux-only, so this is the
# only place the leak half actually runs.
# ---------------------------------------------------------------------------
if [[ "${TARGET}" == "sanitizers" || "${TARGET}" == "all" ]]; then
    step "AddressSanitizer + UndefinedBehaviorSanitizer"
    "${RUNTIME}" run --rm -v "${PROJECT_ROOT}":/src -w /src ubuntu:24.04 bash -c '
        set -eo pipefail
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq > /dev/null 2>&1
        apt-get install -y -qq gcc g++ cmake python3 git ca-certificates > /dev/null 2>&1
        rm -rf /tmp/ci-asan
        cmake -S . -B /tmp/ci-asan -DENABLE_SANITIZERS=ON -DBUILD_TESTS=ON               -DBUILD_EXAMPLES=ON > /dev/null 2>&1
        cmake --build /tmp/ci-asan -j"$(nproc)" > /tmp/ci-asan.log 2>&1 || {
            tail -30 /tmp/ci-asan.log; exit 1; }
        export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
        export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
        # See ci.yml: the compaction benchmark under ASan is ~9 minutes of
        # sanitizer overhead, not a test.
        ctest --test-dir /tmp/ci-asan --output-on-failure -E compaction_benchmark 2>&1 | tail -3
    ' || die "the sanitizer job failed"
    ok "sanitizers (with leak detection)"
fi

# ---------------------------------------------------------------------------
# Coverage — the job in coverage.yml, including its floors
# ---------------------------------------------------------------------------
if [[ "${TARGET}" == "coverage" || "${TARGET}" == "all" ]]; then
    step "Coverage"
    "${RUNTIME}" run --rm -v "${PROJECT_ROOT}":/src -w /src ubuntu:24.04 bash -c '
        set -eo pipefail
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq > /dev/null 2>&1
        apt-get install -y -qq gcc g++ cmake python3 git ca-certificates lcov > /dev/null 2>&1
        rm -rf /tmp/ci-cov
        cmake -S . -B /tmp/ci-cov -DENABLE_COVERAGE=ON -DBUILD_TESTS=ON > /dev/null 2>&1
        cmake --build /tmp/ci-cov -j"$(nproc)" > /dev/null 2>&1
        ctest --test-dir /tmp/ci-cov > /dev/null 2>&1
        cd /tmp/ci-cov
        FLAGS="--rc branch_coverage=1 --ignore-errors empty,unused,inconsistent,mismatch"
        lcov --capture --directory . --output-file raw.info $FLAGS > /dev/null 2>&1
        # Same scope as coverage.yml: the library, not terseml or tools.
        lcov --remove raw.info "*/tests/*" "*/_deps/*" "/usr/*" "*/examples/*" \
             "*/terseml/*" "*/tools/*" --output-file lcov.info $FLAGS > /dev/null 2>&1
        SUMMARY=$(lcov --summary lcov.info $FLAGS 2>&1)
        LINES=$(echo "$SUMMARY" | awk "/lines\.*:/ {gsub(\"%\",\"\",\$2); print \$2}")
        BRANCHES=$(echo "$SUMMARY" | awk "/branches\.*:/ {gsub(\"%\",\"\",\$2); print \$2}")
        [ -n "$LINES" ] && [ -n "$BRANCHES" ] || { echo "  could not parse lcov totals"; exit 1; }
        echo "  lines ${LINES}%  branches ${BRANCHES}%  (floors 97 / 80)"
        awk "BEGIN { exit !($LINES < 97) }"    && { echo "  below the line floor"; exit 1; }
        awk "BEGIN { exit !($BRANCHES < 80) }" && { echo "  below the branch floor"; exit 1; }
        exit 0
    ' || die "the coverage job failed"
    ok "coverage meets the CI floors"
fi

echo -e "\n${GREEN}${BOLD}Linux CI jobs pass.${NC}"
