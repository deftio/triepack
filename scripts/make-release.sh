#!/usr/bin/env bash
#
# make-release.sh — cut a triepack release.
#
# Builds every target, runs every test suite, and only if all of it is green
# drives the git flow: version bump on a release branch, PR, wait for CI,
# squash-merge, tag. Pushing the tag is what triggers the GitHub Release and
# the npm publish, so nothing reaches a registry that has not passed CI.
#
#   ./scripts/make-release.sh --check      # local gate only, no git actions
#   ./scripts/make-release.sh              # release whatever the file declares
#   ./scripts/make-release.sh --dry-run
#
# The version comes from triepack-version.txt and nowhere else. To release a
# new version, edit that file, run scripts/sync_version.sh, and land the
# result through the normal review flow — then run this. This script reads the
# version; it never decides it.
#
# Options:
#   --check           Run the build/test gate and stop. No commits, no PR, no
#                     tag. Use this any time; it is also what a real release
#                     runs before touching git.
#   --skip a,b,c      Targets to skip, comma-separated, when a toolchain is
#                     genuinely unavailable. A missing toolchain is otherwise
#                     a failure: a release must be tested on every target.
#                     Targets: c, js, ts, python, go, rust, swift, java, kotlin
#                     java and kotlin need no toolchain installed — see
#                     scripts/test-jvm.sh.
#   --merge STRATEGY  How to land the PR: squash (default), merge, or rebase.
#                     Squash is right for a release PR that is only a version
#                     bump. It is wrong for a branch carrying real work, which
#                     it would flatten into one commit — the script warns
#                     before doing that.
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
MERGE_STRATEGY="squash"
BUILD_DIR="${PROJECT_ROOT}/build-release"

ALL_TARGETS=(c js ts python go rust swift java kotlin)

# --------------------------------------------------------------------------
# Argument parsing
# --------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --version|--version=*)
            echo -e "${RED}make-release.sh does not set the version.${NC}" >&2
            echo "The version lives in triepack-version.txt. To release a new one:" >&2
            echo "  1. edit triepack-version.txt" >&2
            echo "  2. ./scripts/sync_version.sh" >&2
            echo "  3. commit and land it through review" >&2
            echo "  4. ./scripts/make-release.sh" >&2
            exit 2
            ;;
        --check)     CHECK_ONLY=1; shift ;;
        --skip)
            SKIP_LIST="${2:-}"
            [[ -z "${SKIP_LIST}" ]] && { echo "--skip needs a target list" >&2; exit 2; }
            shift 2
            ;;
        --skip=*)    SKIP_LIST="${1#*=}"; shift ;;
        --merge)
            MERGE_STRATEGY="${2:-}"
            case "${MERGE_STRATEGY}" in
                squash|merge|rebase) ;;
                *) echo "--merge takes squash, merge or rebase" >&2; exit 2 ;;
            esac
            shift 2
            ;;
        --merge=*)
            MERGE_STRATEGY="${1#*=}"
            case "${MERGE_STRATEGY}" in
                squash|merge|rebase) ;;
                *) echo "--merge takes squash, merge or rebase" >&2; exit 2 ;;
            esac
            shift
            ;;
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
step "Reading the version"

VERSION=$(head -1 triepack-version.txt | tr -d '[:space:]')
[[ "${VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] \
    || die "triepack-version.txt must hold a bare X.Y.Z version, got '${VERSION}'"
ok "triepack-version.txt declares ${VERSION}"

# Every manifest, constant and page has to already agree. This script does not
# fix drift: a version bump is a reviewed change like any other.
./scripts/sync_version.sh --check | sed 's/^/  /' \
    || die "version strings have drifted; run ./scripts/sync_version.sh and commit the result"

# CHANGELOG.md is the single source of truth for what changed in each version.
./scripts/sync_changelog.sh --check | sed 's/^/  /' \
    || die "docs/releases.md is stale; run ./scripts/sync_changelog.sh and commit the result"

grep -q "^## \[${VERSION}\]" CHANGELOG.md \
    || die "CHANGELOG.md has no '## [${VERSION}]' section — write the release notes first"

# The repository agreeing with itself is not enough. A version already on a
# registry cannot be replaced, so check the outside world before building
# anything: git tags, GitHub releases, npm.
./scripts/check_versions.sh || die "the declared version is not clear to release"

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

# -- Java and Kotlin -------------------------------------------------------
# Gradle when it is installed, matching CI. Otherwise scripts/test-jvm.sh,
# which compiles with javac/kotlinc directly and fetches whatever is missing
# into .jvm-toolchain/ — so neither target has to be skipped for want of a
# build system.
# --gradle runs the real build.gradle files, which is what CI does. The direct
# javac/kotlinc path compiles the same sources without touching those files,
# so it cannot catch a problem in them — and it did not catch the Kotlin
# plugin refusing JVM target 21.
gate_java() { ./scripts/test-jvm.sh --gradle java; }
run_target java "Java (Gradle)" curl gate_java

gate_kotlin() { ./scripts/test-jvm.sh --gradle kotlin; }
run_target kotlin "Kotlin (Gradle)" curl gate_kotlin

# --------------------------------------------------------------------------
# 4. Gate summary
# --------------------------------------------------------------------------
step "Gate summary"
echo -e "  ${YELLOW}Not covered here: the ubuntu-only CI jobs (gcc zero-warning gate,${NC}"
echo -e "  ${YELLOW}Swift on Linux). Run ./scripts/test-ci-linux.sh for those.${NC}"
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
step "Checking the release state"

DEFAULT_BRANCH=$(gh repo view --json defaultBranchRef --jq .defaultBranchRef.name 2>/dev/null || echo main)
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
TAG="v${VERSION}"

if git rev-parse "${TAG}" >/dev/null 2>&1; then
    die "tag ${TAG} already exists — bump triepack-version.txt for a new release"
fi
ok "tag ${TAG} is free"

# The version bump is an ordinary reviewed change, so by release time it is
# usually already on the default branch and there is nothing to do but tag.
# Running from a branch that still carries it is also fine: that branch is
# taken through review first.
if [[ "${CURRENT_BRANCH}" == "${DEFAULT_BRANCH}" ]]; then
    NEEDS_PR=0
    ok "on ${DEFAULT_BRANCH}; ${VERSION} is already landed"
else
    NEEDS_PR=1
    ok "on ${CURRENT_BRANCH}, which still has to land"
fi

if [[ ${NEEDS_PR} -eq 1 ]]; then
step "Opening the pull request"
RELEASE_BRANCH="${CURRENT_BRANCH}"
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

# Squashing a branch that carries real work throws away its history. A release
# PR is usually a single version-bump commit, where squash is exactly right.
BRANCH_COMMITS=$(git rev-list --count "origin/${DEFAULT_BRANCH}..HEAD" 2>/dev/null || echo 0)
if [[ "${MERGE_STRATEGY}" == "squash" && "${BRANCH_COMMITS}" -gt 3 ]]; then
    echo -e "  ${YELLOW}This branch has ${BRANCH_COMMITS} commits.${NC}"
    echo -e "  ${YELLOW}--merge squash lands them on ${DEFAULT_BRANCH} as one commit and${NC}"
    echo -e "  ${YELLOW}discards the rest. Use --merge merge or --merge rebase to keep them.${NC}"
    confirm "Squash ${BRANCH_COMMITS} commits into one anyway?" \
        || die "declined — rerun with --merge merge or --merge rebase"
else
    confirm "Merge the release PR into ${DEFAULT_BRANCH} (--${MERGE_STRATEGY})?" || die "declined"
fi

# GitHub composes a squash message by concatenating every commit on the
# branch, trailers included. Commits landing on the default branch carry the
# maintainer's name alone — when triepack breaks, that is who gets called — so
# the message is supplied explicitly with any Co-Authored-By trailers removed.
if [[ "${MERGE_STRATEGY}" == "squash" && ${DRY_RUN} -eq 0 ]]; then
    SQUASH_BODY="$(git log --reverse --format='%s%n%n%b' "origin/${DEFAULT_BRANCH}..HEAD" \
                   | grep -v '^Co-Authored-By:' | cat -s)"
    gh pr merge --squash --delete-branch \
        --subject "$(gh pr view --json title --jq .title)" \
        --body "${SQUASH_BODY}"
else
    run_git gh pr merge "--${MERGE_STRATEGY}" --delete-branch
fi

run_git git checkout "${DEFAULT_BRANCH}"
run_git git pull --ff-only origin "${DEFAULT_BRANCH}"

fi  # NEEDS_PR

# --------------------------------------------------------------------------
# 6. Tag — this is what publishes
# --------------------------------------------------------------------------
step "Tagging ${TAG}"

confirm "Tag ${TAG} and push? This publishes the GitHub Release and npm package." \
    || die "declined — merge is done; tag manually when ready"

run_git git tag -a "${TAG}" -m "triepack ${TAG}"
run_git git push origin "${TAG}"

echo -e "\n${GREEN}${BOLD}Released ${TAG}.${NC}"
echo "  The tag triggers .github/workflows/publish.yml, which re-runs the"
echo "  full test matrix and only then creates the GitHub Release and"
echo "  publishes to npm."
echo ""
echo "  Watch it:   gh run watch"
echo "  Verify:     npm view triepack version"
