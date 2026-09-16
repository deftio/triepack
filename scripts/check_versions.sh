#!/usr/bin/env bash
#
# check_versions.sh — reconcile the declared version against everything that
# has already been published.
#
# sync_version.sh checks that the repository agrees with itself. This checks
# the repository against the outside world: git tags, GitHub releases, and the
# package registries. Those can disagree in ways nothing inside the tree can
# see — a version published by hand, a tag that was never pushed, a release cut
# from a different tree.
#
# The rule: the declared version must be strictly newer than everything
# already published. Registries do not let a version be replaced, so releasing
# one that is already out there is not recoverable.
#
#   ./scripts/check_versions.sh            # before tagging: the tag must be free
#   ./scripts/check_versions.sh --tagged   # from the tag: it must exist, at HEAD
#   ./scripts/check_versions.sh --quiet    # verdict only
#   ./scripts/check_versions.sh --offline  # skip the network, check tags only
#
# The tag is the one thing whose expected state flips. Before a release it
# must not exist; a workflow triggered by that tag runs with it necessarily
# present, and should instead confirm it points at the commit being built.
# Registries are checked the same way either way: the version must not be
# published yet.
#
# Exit status: 0 if the declared version is clear to release, 1 if not,
#              2 on usage error.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

if [[ -t 1 ]]; then
    GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; BOLD='\033[1m'; NC='\033[0m'
else
    GREEN=''; YELLOW=''; RED=''; BOLD=''; NC=''
fi

QUIET=0
OFFLINE=0
TAGGED=0
for arg in "$@"; do
    case "$arg" in
        --quiet)   QUIET=1 ;;
        --offline) OFFLINE=1 ;;
        --tagged)  TAGGED=1 ;;
        -h|--help)
            sed -n '3,22p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown argument: ${arg}${NC}" >&2
            exit 2
            ;;
    esac
done

say() { [[ ${QUIET} -eq 1 ]] || echo -e "$*"; }

DECLARED=$(head -1 triepack-version.txt | tr -d '[:space:]')
[[ "${DECLARED}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] \
    || { echo -e "${RED}triepack-version.txt is not X.Y.Z: '${DECLARED}'${NC}" >&2; exit 2; }

# Is $1 strictly greater than $2? sort -V orders versions correctly.
newer_than() {
    [[ "$1" != "$2" ]] && [[ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -1)" == "$1" ]]
}

PROBLEMS=0
note_problem() { PROBLEMS=$((PROBLEMS + 1)); }

say "\n${BOLD}Declared version${NC}  ${GREEN}${DECLARED}${NC}  (triepack-version.txt)\n"
say "  $(printf '%-22s %-16s %s' 'where' 'latest published' 'verdict')"
say "  $(printf '%.60s' '------------------------------------------------------------')"

report() {
    local where="$1" latest="$2" verdict="$3" colour="$4"
    say "  $(printf '%-22s %-16s ' "${where}" "${latest}")${colour}${verdict}${NC}"
}

# --------------------------------------------------------------------------
# git tags
# --------------------------------------------------------------------------
git fetch --tags --quiet origin 2>/dev/null || true
LATEST_TAG=$(git tag -l 'v*' | sed 's/^v//' | sort -V | tail -1)
LATEST_TAG=${LATEST_TAG:-none}

if [[ ${TAGGED} -eq 1 ]]; then
    # Running from the tag: it has to exist, and has to be this commit.
    if ! git rev-parse "v${DECLARED}" >/dev/null 2>&1; then
        report "git tag" "v${LATEST_TAG}" "v${DECLARED} does not exist" "${RED}"
        note_problem
    elif [[ "$(git rev-parse "v${DECLARED}^{commit}")" != "$(git rev-parse HEAD)" ]]; then
        report "git tag" "v${LATEST_TAG}" "v${DECLARED} is not this commit" "${RED}"
        note_problem
    else
        report "git tag" "v${LATEST_TAG}" "v${DECLARED} is HEAD" "${GREEN}"
    fi
elif git rev-parse "v${DECLARED}" >/dev/null 2>&1; then
    report "git tag" "v${LATEST_TAG}" "v${DECLARED} already exists" "${RED}"
    note_problem
elif [[ "${LATEST_TAG}" != "none" ]] && ! newer_than "${DECLARED}" "${LATEST_TAG}"; then
    report "git tag" "v${LATEST_TAG}" "not newer than v${LATEST_TAG}" "${RED}"
    note_problem
else
    report "git tag" "v${LATEST_TAG}" "free" "${GREEN}"
fi

if [[ ${OFFLINE} -eq 1 ]]; then
    say "\n  ${YELLOW}--offline: registries and releases not checked${NC}"
else
    # ----------------------------------------------------------------------
    # GitHub releases
    # ----------------------------------------------------------------------
    if command -v gh >/dev/null 2>&1 && gh auth status >/dev/null 2>&1; then
        GH_LATEST=$(gh release list --limit 1 --json tagName --jq '.[0].tagName' 2>/dev/null || echo "")
        GH_LATEST=${GH_LATEST:-none}
        if gh release view "v${DECLARED}" >/dev/null 2>&1; then
            report "GitHub release" "${GH_LATEST}" "v${DECLARED} already released" "${RED}"
            note_problem
        else
            report "GitHub release" "${GH_LATEST}" "free" "${GREEN}"
        fi
    else
        report "GitHub release" "?" "gh unavailable, skipped" "${YELLOW}"
    fi

    # ----------------------------------------------------------------------
    # npm — the package the release actually publishes
    # ----------------------------------------------------------------------
    if command -v npm >/dev/null 2>&1; then
        NPM_LATEST=$(npm view triepack version 2>/dev/null || echo "")
        if [[ -z "${NPM_LATEST}" ]]; then
            report "npm triepack" "none" "unpublished" "${GREEN}"
        elif npm view "triepack@${DECLARED}" version >/dev/null 2>&1; then
            report "npm triepack" "${NPM_LATEST}" "${DECLARED} already published" "${RED}"
            note_problem
        elif ! newer_than "${DECLARED}" "${NPM_LATEST}"; then
            report "npm triepack" "${NPM_LATEST}" "not newer than ${NPM_LATEST}" "${RED}"
            note_problem
        else
            report "npm triepack" "${NPM_LATEST}" "free" "${GREEN}"
        fi
    else
        report "npm triepack" "?" "npm unavailable, skipped" "${YELLOW}"
    fi

    # ----------------------------------------------------------------------
    # PyPI — published by pypi.yml, so held to the same rule as npm
    # ----------------------------------------------------------------------
    PYPI_JSON=$(curl -fsS "https://pypi.org/pypi/triepack/json" 2>/dev/null || echo "")
    if [[ -z "${PYPI_JSON}" ]]; then
        report "PyPI triepack" "none" "unpublished" "${GREEN}"
    else
        PYPI_LATEST=$(echo "${PYPI_JSON}" \
            | python3 -c "import json,sys; print(json.load(sys.stdin)['info']['version'])" 2>/dev/null || echo "")
        # A version is "on PyPI" if it appears in releases, even yanked.
        if echo "${PYPI_JSON}" | python3 -c "
import json, sys
d = json.load(sys.stdin)
sys.exit(0 if '${DECLARED}' in d.get('releases', {}) else 1)
" 2>/dev/null; then
            report "PyPI triepack" "${PYPI_LATEST:-?}" "${DECLARED} already published" "${RED}"
            note_problem
        elif [[ -n "${PYPI_LATEST}" ]] && ! newer_than "${DECLARED}" "${PYPI_LATEST}"; then
            report "PyPI triepack" "${PYPI_LATEST}" "not newer than ${PYPI_LATEST}" "${RED}"
            note_problem
        else
            report "PyPI triepack" "${PYPI_LATEST:-none}" "free" "${GREEN}"
        fi
    fi

    # crates.io is not published to yet. Informational: if the name gets
    # claimed elsewhere, this is where it shows up.
    CRATE=$(curl -fsS -H "User-Agent: triepack-release-check" \
            "https://crates.io/api/v1/crates/triepack" 2>/dev/null \
            | python3 -c "import json,sys; print(json.load(sys.stdin)['crate']['max_version'])" 2>/dev/null || echo "")
    report "crates.io triepack" "${CRATE:-none}" "$([[ -z "${CRATE}" ]] && echo 'not published' || echo 'published')" "${YELLOW}"
fi

say ""
if [[ ${PROBLEMS} -ne 0 ]]; then
    echo -e "${RED}${BOLD}${DECLARED} cannot be released.${NC}" >&2
    echo -e "${RED}Bump triepack-version.txt past everything above, run" >&2
    echo -e "./scripts/sync_version.sh, and land the result.${NC}" >&2
    exit 1
fi
say "${GREEN}${BOLD}${DECLARED} is ahead of everything published.${NC}"
