#!/usr/bin/env bash
#
# make-release.sh — cut a triepack release.
#
# Builds every target, runs every test suite, and only if all of it is green
# drives the git flow: version bump on a release branch, PR, wait for CI,
# squash-merge, tag. Pushing the tag is what triggers the GitHub Release and
# the npm publish, so nothing reaches a registry that has not passed CI.
#
#   ./scripts/make-release.sh --check              # local gate only, no git actions
#   ./scripts/make-release.sh --version 1.2.0      # full release
#   ./scripts/make-release.sh --version 1.2.0 --dry-run
#
# Options:
#   --version X.Y.Z   Version to release. Written to triepack-version.txt and
#                     propagated by scripts/sync_version.sh.
#   --check           Run the build/test gate and stop. No commits, no PR, no
#                     tag. Use this any time; it is also what --version runs
#                     before touching git.
#   --skip a,b,c      Targets to skip, comma-separated, when a toolchain is
#                     genuinely unavailable. A missing toolchain is otherwise
#                     a failure: a release must be tested on every target.
#                     Targets: c, js, ts, python, go, rust, swift, java, kotlin
#   --dry-run         Print the git/gh commands instead of running them.
#   --yes             Do not prompt before the PR, merge and tag steps.
#
# Requires: cmake, a C/C++ compiler, and the toolchain for each target, plus
# git and the GitHub CLI (gh, authenticated) for anything past --check.
#
# Exit status: 0 on success, 1 on a failed gate or release step, 2 on usage.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

if [[ -t 1 ]]; then
    GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'
    BLUE='\033[0;34m'; BOLD='\033[1m'; NC='\033[0m'
else
    GREEN=''; YELLOW=''; RED=''; BLUE=''; BOLD=''; NC=''
fi

VERSION=""
CHECK_ONLY=0
DRY_RUN=0
ASSUME_YES=0
SKIP_LIST=""
BUILD_DIR="${PROJECT_ROOT}/build-release"

ALL_TARGETS=(c js ts python go rust swift java kotlin)

# --------------------------------------------------------------------------
# Argument parsing
# --------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            VERSION="${2:-}"
            [[ -z "${VERSION}" ]] && { echo "--version needs X.Y.Z" >&2; exit 2; }
            shift 2
            ;;
        --version=*) VERSION="${1#*=}"; shift ;;
        --check)     CHECK_ONLY=1; shift ;;
        --skip)
            SKIP_LIST="${2:-}"
            [[ -z "${SKIP_LIST}" ]] && { echo "--skip needs a target list" >&2; exit 2; }
            shift 2
            ;;
        --skip=*)    SKIP_LIST="${1#*=}"; shift ;;
        --dry-run)   DRY_RUN=1; shift ;;
        --yes|-y)    ASSUME_YES=1; shift ;;
        -h|--help)
            sed -n '3,31p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown argument: $1${NC}" >&2
            echo "Try $0 --help" >&2
            exit 2
            ;;
    esac
done

if [[ ${CHECK_ONLY} -eq 0 && -z "${VERSION}" ]]; then
    echo -e "${RED}Give --version X.Y.Z, or --check to run the gate only.${NC}" >&2
    exit 2
fi

if [[ -n "${VERSION}" && ! "${VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo -e "${RED}Version must be X.Y.Z, got '${VERSION}'${NC}" >&2
    exit 2
fi

# --------------------------------------------------------------------------
# Helpers
# --------------------------------------------------------------------------
step()  { echo -e "\n${BLUE}${BOLD}==>${NC} ${BOLD}$*${NC}"; }
ok()    { echo -e "  ${GREEN}pass${NC}  $*"; }
warn()  { echo -e "  ${YELLOW}skip${NC}  $*"; }
fail()  { echo -e "  ${RED}FAIL${NC}  $*"; }
die()   { echo -e "\n${RED}${BOLD}Release aborted:${NC} $*" >&2; exit 1; }

is_skipped() {
    [[ ",${SKIP_LIST}," == *",$1,"* ]]
}

have() { command -v "$1" >/dev/null 2>&1; }

run_git() {
    if [[ ${DRY_RUN} -eq 1 ]]; then
        echo -e "  ${YELLOW}dry-run${NC} $*"
    else
        "$@"
    fi
}

confirm() {
    [[ ${ASSUME_YES} -eq 1 ]] && return 0
    [[ ${DRY_RUN} -eq 1 ]] && return 0
    local reply
    read -r -p "  $1 [y/N] " reply
    [[ "${reply}" =~ ^[Yy]$ ]]
}

SKIPPED=()
LOG_DIR="$(mktemp -d)"
trap 'rm -rf "${LOG_DIR}"' EXIT

# Run one target's gate. Args: target key, human label, tool to require, command...
# Output goes to a log file, shown only on failure so a green run stays quiet.
run_target() {
    local key="$1" label="$2" tool="$3"; shift 3
    local log="${LOG_DIR}/${key}.log"

    if is_skipped "${key}"; then
        warn "${label} (--skip ${key})"
        SKIPPED+=("${key}")
        return 0
    fi
    if ! have "${tool}"; then
        fail "${label}: ${tool} not found"
        die "${label} has no toolchain here. Install it, or pass --skip ${key} to release without testing that target."
    fi

    if ( "$@" ) > "${log}" 2>&1; then
        ok "${label}"
    else
        fail "${label}"
        echo ""
        tail -40 "${log}"
        die "${label} failed. Full log: ${log}"
    fi
}

# --------------------------------------------------------------------------
# 1. Preflight
# --------------------------------------------------------------------------
step "Preflight"

[[ -f triepack-version.txt ]] || die "run this from a triepack checkout (triepack-version.txt not found)"
ok "repository root"

if [[ ${CHECK_ONLY} -eq 0 ]]; then
    have git || die "git not found"
    have gh  || die "the GitHub CLI (gh) is needed to open and merge the PR"
    if [[ ${DRY_RUN} -eq 0 ]] && ! gh auth status >/dev/null 2>&1; then
        die "gh is not authenticated — run 'gh auth login'"
    fi
    ok "git and gh available"

    if [[ -n "$(git status --porcelain)" ]]; then
        die "working tree is dirty; commit or stash first"
    fi
    ok "working tree is clean"
fi

# --------------------------------------------------------------------------
# 2. Version
# --------------------------------------------------------------------------
if [[ -n "${VERSION}" ]]; then
    step "Setting version to ${VERSION}"
    CURRENT=$(head -1 triepack-version.txt | tr -d '[:space:]')
    if [[ "${CURRENT}" == "${VERSION}" ]]; then
        ok "triepack-version.txt already reads ${VERSION}"
    else
        echo "${VERSION}" > triepack-version.txt
        ok "triepack-version.txt: ${CURRENT} -> ${VERSION}"
    fi
    ./scripts/sync_version.sh | sed 's/^/  /'
else
    step "Checking version consistency"
    VERSION=$(head -1 triepack-version.txt | tr -d '[:space:]')
    ./scripts/sync_version.sh --check | sed 's/^/  /' \
        || die "version strings have drifted; run ./scripts/sync_version.sh"
fi

# --------------------------------------------------------------------------
# 3. Build and test every target
# --------------------------------------------------------------------------
step "Building and testing all targets"

# -- C / C++ ---------------------------------------------------------------
gate_c() {
    rm -rf "${BUILD_DIR}"
    cmake -S . -B "${BUILD_DIR}" -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
    cmake --build "${BUILD_DIR}" -j 2>&1 | tee "${LOG_DIR}/c-build.log"
    # The project holds itself to a zero-warning build.
    if grep -iE "warning:" "${LOG_DIR}/c-build.log" | grep -v "In file included" \
                                                    | grep -v "search path"; then
        echo "build produced warnings"
        return 1
    fi
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
}
run_target c "C / C++ (build, zero warnings, ctest)" cmake gate_c

# -- Conformance corpus ----------------------------------------------------
# The bindings check themselves against checked-in fixtures, so those have to
# be exactly what the generators produce from the current source.
gate_corpus() {
    python3 tools/gen_conformance_cases.py
    cmake --build "${BUILD_DIR}" --target fixtures
    python3 tools/gen_malformed_fixtures.py
    git diff --exit-code -- tests/conformance tests/fixtures
}
if is_skipped c; then
    warn "conformance corpus (needs the C build)"
else
    run_target corpus "Conformance corpus is in sync" python3 gate_corpus
fi

# -- JavaScript ------------------------------------------------------------
gate_js() { cd bindings/javascript && npm ci --silent && npx jest; }
run_target js "JavaScript" npm gate_js

# -- TypeScript ------------------------------------------------------------
gate_ts() { cd bindings/typescript && npm ci --silent && npx tsc --noEmit && npx jest; }
run_target ts "TypeScript" npm gate_ts

# -- Python ----------------------------------------------------------------
gate_python() { cd bindings/python && python3 -m pytest -q; }
run_target python "Python" python3 gate_python

# -- Go --------------------------------------------------------------------
gate_go() {
    cd bindings/go
    local unformatted
    unformatted=$(gofmt -l .)
    if [[ -n "${unformatted}" ]]; then
        echo "gofmt would reformat: ${unformatted}"
        return 1
    fi
    go vet ./... && go test ./...
}
run_target go "Go" go gate_go

# -- Rust ------------------------------------------------------------------
gate_rust() { cd bindings/rust && cargo test --quiet; }
run_target rust "Rust" cargo gate_rust

# -- Swift -----------------------------------------------------------------
gate_swift() { cd bindings/swift && swift test; }
run_target swift "Swift" swift gate_swift

# -- Java ------------------------------------------------------------------
gate_java() { cd bindings/java && gradle --quiet --console=plain test; }
run_target java "Java" gradle gate_java

# -- Kotlin ----------------------------------------------------------------
gate_kotlin() { cd bindings/kotlin && gradle --quiet --console=plain test; }
run_target kotlin "Kotlin" gradle gate_kotlin

# --------------------------------------------------------------------------
# 4. Gate summary
# --------------------------------------------------------------------------
step "Gate summary"
if [[ ${#SKIPPED[@]} -gt 0 ]]; then
    echo -e "  ${YELLOW}Skipped targets: ${SKIPPED[*]}${NC}"
    echo -e "  ${YELLOW}These were NOT tested. CI will still run them on the PR.${NC}"
else
    echo -e "  ${GREEN}Every target built and tested.${NC}"
fi

if [[ ${CHECK_ONLY} -eq 1 ]]; then
    echo -e "\n${GREEN}${BOLD}Gate passed.${NC} (--check: stopping before any git action.)"
    exit 0
fi

# --------------------------------------------------------------------------
# 5. Release branch, PR, CI, squash-merge
# --------------------------------------------------------------------------
step "Preparing the release commit"

DEFAULT_BRANCH=$(gh repo view --json defaultBranchRef --jq .defaultBranchRef.name 2>/dev/null || echo main)
RELEASE_BRANCH="release/v${VERSION}"
TAG="v${VERSION}"

if git rev-parse "${TAG}" >/dev/null 2>&1; then
    die "tag ${TAG} already exists"
fi

if [[ -z "$(git status --porcelain)" ]]; then
    ok "nothing to commit — version files already match ${VERSION}"
    COMMIT_NEEDED=0
else
    COMMIT_NEEDED=1
    git --no-pager diff --stat | sed 's/^/  /'
    confirm "Commit these version changes?" || die "declined"
fi

CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
if [[ "${CURRENT_BRANCH}" == "${DEFAULT_BRANCH}" ]]; then
    run_git git checkout -b "${RELEASE_BRANCH}"
    ok "created ${RELEASE_BRANCH}"
else
    RELEASE_BRANCH="${CURRENT_BRANCH}"
    ok "releasing from the current branch ${RELEASE_BRANCH}"
fi

if [[ ${COMMIT_NEEDED} -eq 1 ]]; then
    run_git git add -A
    run_git git commit -m "Bump version to ${VERSION}"
    ok "committed the version bump"
fi

step "Opening the pull request"
run_git git push -u origin "${RELEASE_BRANCH}"

if [[ ${DRY_RUN} -eq 1 ]]; then
    echo -e "  ${YELLOW}dry-run${NC} gh pr create --base ${DEFAULT_BRANCH} --head ${RELEASE_BRANCH} --title \"Release v${VERSION}\""
else
    if gh pr view --json number >/dev/null 2>&1; then
        ok "a pull request already exists for this branch"
    else
        gh pr create \
            --base "${DEFAULT_BRANCH}" \
            --head "${RELEASE_BRANCH}" \
            --title "Release v${VERSION}" \
            --body "$(cat <<EOF
Release v${VERSION}.

Version propagated from \`triepack-version.txt\` by \`scripts/sync_version.sh\`.

Local gate (\`scripts/make-release.sh\`) passed before this PR was opened.
Merging and tagging \`${TAG}\` publishes the GitHub Release and the npm
package, both gated on CI.
EOF
)"
        ok "pull request opened"
    fi
fi

step "Waiting for CI"
if [[ ${DRY_RUN} -eq 1 ]]; then
    echo -e "  ${YELLOW}dry-run${NC} gh pr checks --watch --fail-fast"
else
    gh pr checks --watch --fail-fast || die "CI failed on the release PR"
    ok "CI is green"
fi

step "Merging"
confirm "Squash-merge the release PR into ${DEFAULT_BRANCH}?" || die "declined"
run_git gh pr merge --squash --delete-branch

# --------------------------------------------------------------------------
# 6. Tag — this is what publishes
# --------------------------------------------------------------------------
step "Tagging ${TAG}"
run_git git checkout "${DEFAULT_BRANCH}"
run_git git pull --ff-only origin "${DEFAULT_BRANCH}"

confirm "Tag ${TAG} and push? This publishes the GitHub Release and npm package." \
    || die "declined — merge is done; tag manually when ready"

run_git git tag -a "${TAG}" -m "triepack ${TAG}"
run_git git push origin "${TAG}"

echo -e "\n${GREEN}${BOLD}Released ${TAG}.${NC}"
echo "  The tag triggers .github/workflows/release.yml, which re-runs the"
echo "  full test matrix and only then creates the GitHub Release and"
echo "  publishes to npm."
echo ""
echo "  Watch it:   gh run watch"
echo "  Verify:     npm view triepack version"
