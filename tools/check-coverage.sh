#!/bin/bash
set -euo pipefail

# check-coverage.sh — Build with coverage, run tests, generate lcov report,
# and check against the coverage threshold.

THRESHOLD="${1:-100}"
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "=== TXZ Coverage Check ==="
echo "Project root: $PROJECT_ROOT"
echo "Coverage threshold: ${THRESHOLD}%"
echo ""

# Build with coverage
echo "--- Configuring with coverage enabled ---"
cmake -B "$PROJECT_ROOT/build" -S "$PROJECT_ROOT" \
  -DENABLE_COVERAGE=ON \
  -DBUILD_TESTS=ON

echo ""
echo "--- Building ---"
cmake --build "$PROJECT_ROOT/build"

echo ""
echo "--- Running tests ---"
ctest --test-dir "$PROJECT_ROOT/build" --output-on-failure

echo ""
echo "--- Generating coverage report ---"
cd "$PROJECT_ROOT/build"

lcov --capture \
  --directory . \
  --output-file lcov.info \
  --rc lcov_branch_coverage=1

lcov --remove lcov.info \
  "*/tests/*" "*/Unity/*" "/usr/*" \
  --output-file lcov.info \
  --rc lcov_branch_coverage=1

genhtml lcov.info \
  --output-directory coverage \
  --branch-coverage

echo ""
echo "Coverage report: $PROJECT_ROOT/build/coverage/index.html"

# Check threshold
COVERAGE=$(lcov --summary lcov.info 2>&1 | grep "lines" | grep -oP '[0-9.]+%' | head -1)
COVERAGE_NUM="${COVERAGE%\%}"

echo "Line coverage: $COVERAGE"

if [ "$(echo "$COVERAGE_NUM < $THRESHOLD" | bc -l)" -eq 1 ]; then
  echo "FAIL: Coverage ${COVERAGE} is below threshold ${THRESHOLD}%"
  exit 1
else
  echo "PASS: Coverage ${COVERAGE} meets threshold ${THRESHOLD}%"
fi
