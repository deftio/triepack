#!/bin/bash
set -euo pipefail

# check-coverage.sh — build with coverage, run the tests, generate an lcov
# report, and check the totals against a floor.
#
#   ./tools/check-coverage.sh              # default floors
#   ./tools/check-coverage.sh 99           # line floor only
#   ./tools/check-coverage.sh 99 82        # line and branch floors
#
# The floors match .github/workflows/coverage.yml. They are below 100 on
# purpose: what is left uncovered is error handling that needs a dictionary
# corrupted in one specific way. See docs/guide/testing.md.

LINE_THRESHOLD="${1:-97}"
BRANCH_THRESHOLD="${2:-80}"
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build-coverage"

echo "=== triepack coverage check ==="
echo "Project root:    $PROJECT_ROOT"
echo "Line floor:      ${LINE_THRESHOLD}%"
echo "Branch floor:    ${BRANCH_THRESHOLD}%"
echo ""

# lcov drives gcov. Apple ships an llvm-cov shim that needs a wrapper to be
# called the way lcov expects.
GCOV_ARG=()
if [ "$(uname -s)" = "Darwin" ] && xcrun --find llvm-cov >/dev/null 2>&1; then
    GCOV_WRAPPER="$(mktemp -t llvm-gcov)"
    printf '#!/bin/sh\nexec xcrun llvm-cov gcov "$@"\n' > "$GCOV_WRAPPER"
    chmod +x "$GCOV_WRAPPER"
    GCOV_ARG=(--gcov-tool "$GCOV_WRAPPER")
    trap 'rm -f "$GCOV_WRAPPER"' EXIT
fi

echo "--- Configuring with coverage enabled ---"
cmake -B "$BUILD_DIR" -S "$PROJECT_ROOT" -DENABLE_COVERAGE=ON -DBUILD_TESTS=ON

echo ""
echo "--- Building ---"
cmake --build "$BUILD_DIR" -j

echo ""
echo "--- Clearing stale counters ---"
# gcov accumulates across runs. Without this, a second run reports the union
# of both, and a re-run of the same tree gives a different number from the
# first — which makes the floors below meaningless.
find "$BUILD_DIR" -name '*.gcda' -delete

echo ""
echo "--- Running tests ---"
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo ""
echo "--- Generating coverage report ---"
cd "$BUILD_DIR"

LCOV_FLAGS=(--rc branch_coverage=1 --ignore-errors empty,unused,inconsistent,gcov,mismatch)

lcov --capture --directory . --output-file lcov_raw.info \
     "${GCOV_ARG[@]}" "${LCOV_FLAGS[@]}"

# Tests, vendored Unity (FetchContent puts it under _deps) and the example
# programs are not the code under test.
lcov --remove lcov_raw.info \
     "*/tests/*" "*/_deps/*" "/usr/*" "*/examples/*" \
     --output-file lcov.info "${LCOV_FLAGS[@]}"

genhtml lcov.info --output-directory coverage --branch-coverage \
        --ignore-errors inconsistent,category,unmapped >/dev/null

lcov --list lcov.info --rc branch_coverage=1 --ignore-errors inconsistent

echo ""
echo "Coverage report: $BUILD_DIR/coverage/index.html"
echo ""

SUMMARY="$(lcov --summary lcov.info --rc branch_coverage=1 --ignore-errors inconsistent 2>&1)"
LINES=$(echo "$SUMMARY"    | awk '/lines\.*:/    {gsub("%","",$2); print $2}')
BRANCHES=$(echo "$SUMMARY" | awk '/branches\.*:/ {gsub("%","",$2); print $2}')

if [ -z "$LINES" ] || [ -z "$BRANCHES" ]; then
    echo "FAIL: could not parse coverage totals from lcov output"
    exit 1
fi

echo "Line coverage:   ${LINES}%"
echo "Branch coverage: ${BRANCHES}%"
echo ""

status=0
if awk "BEGIN { exit !($LINES < $LINE_THRESHOLD) }"; then
    echo "FAIL: line coverage ${LINES}% is below the ${LINE_THRESHOLD}% floor"
    status=1
fi
if awk "BEGIN { exit !($BRANCHES < $BRANCH_THRESHOLD) }"; then
    echo "FAIL: branch coverage ${BRANCHES}% is below the ${BRANCH_THRESHOLD}% floor"
    status=1
fi
[ "$status" -eq 0 ] && echo "PASS: coverage meets both floors"
exit "$status"
