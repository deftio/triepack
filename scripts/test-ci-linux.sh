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
#
# Needs a running container runtime. Nothing is installed on the host and the
# images are the only thing cached.
#
#   ./scripts/test-ci-linux.sh          # everything
#   ./scripts/test-ci-linux.sh c        # the gcc build, ctest, zero-warning gate
#   ./scripts/test-ci-linux.sh swift
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
    all|c|swift) ;;
    -h|--help)
        sed -n '3,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
        exit 0
        ;;
    *)
        echo "Usage: $0 [all|c|swift]" >&2
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
        set -e
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
    "${RUNTIME}" run --rm -v "${PROJECT_ROOT}":/src -w /src/bindings/swift swift:5.10-jammy bash -c '
        set -e
        swift --version 2>&1 | head -1 | sed "s/^/  /"
        swift test 2>&1 | grep -E "Executed [0-9]+ tests" | tail -1 | sed "s/^/  /"
    ' || die "the Swift Linux job failed"
    ok "Swift on Linux"
fi

echo -e "\n${GREEN}${BOLD}Linux CI jobs pass.${NC}"
