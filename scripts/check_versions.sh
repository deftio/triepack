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
#   ./scripts/check_versions.sh --released # after releasing: everything must
#                                          # be AT this version, including the
#                                          # default branch
#
# The tag is the one thing whose expected state flips. Before a release it
# must not exist; a workflow triggered by that tag runs with it necessarily
# present, and should instead confirm it points at the commit being built.
# Registries are checked the same way either way: the version must not be
# published yet.
#
# --released inverts every check. Instead of "this version is free", each
# place must already be at it, and origin/<default branch> must contain the
# tag's commit. That last one is the check that was missing when v2.0.0 was
# tagged from a local main two commits ahead of the remote: npm and PyPI
# published 2.0.0 while the branch, and therefore the README and the docs
# site, still said 1.3.2.
#
# Exit status: 0 if the declared version is clear to release (or, with
#              --released, fully released), 1 if not, 2 on usage error.

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
RELEASED=0
for arg in "$@"; do
    case "$arg" in
        --quiet)   QUIET=1 ;;
        --offline) OFFLINE=1 ;;
        --tagged)  TAGGED=1 ;;
        --released) RELEASED=1 ;;
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
if [[ ${RELEASED} -eq 1 ]]; then
    say "  ${BOLD}--released: every row must already be at ${DECLARED}${NC}\n"
fi
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

if [[ ${RELEASED} -eq 1 ]]; then
    # The tag must exist, and the default branch must actually contain it.
    DEFAULT_BRANCH=$(git symbolic-ref --quiet --short refs/remotes/origin/HEAD 2>/dev/null \
                     | sed 's|^origin/||')
    DEFAULT_BRANCH=${DEFAULT_BRANCH:-main}
    git fetch --quiet origin "${DEFAULT_BRANCH}" 2>/dev/null || true

    if ! git rev-parse "v${DECLARED}" >/dev/null 2>&1; then
        report "git tag" "v${LATEST_TAG}" "v${DECLARED} does not exist" "${RED}"
        note_problem
    else
        report "git tag" "v${LATEST_TAG}" "v${DECLARED} exists" "${GREEN}"

        # The check that was missing. A tag the branch does not contain means
        # the registries were published from a commit nobody can see.
        TAG_COMMIT=$(git rev-parse "v${DECLARED}^{commit}")
        if ! git rev-parse "origin/${DEFAULT_BRANCH}" >/dev/null 2>&1; then
            report "origin/${DEFAULT_BRANCH}" "?" "no remote branch" "${YELLOW}"
        elif git merge-base --is-ancestor "${TAG_COMMIT}" "origin/${DEFAULT_BRANCH}" 2>/dev/null; then
            report "origin/${DEFAULT_BRANCH}" "has v${DECLARED}" "contains the tag" "${GREEN}"
        else
            BEHIND=$(git rev-list --count "origin/${DEFAULT_BRANCH}..${TAG_COMMIT}" 2>/dev/null || echo "?")
            report "origin/${DEFAULT_BRANCH}" "behind" "missing the tagged commit (${BEHIND} ahead)" "${RED}"
            note_problem
        fi
    fi
elif [[ ${TAGGED} -eq 1 ]]; then
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
        if [[ ${RELEASED} -eq 1 ]]; then
            if gh release view "v${DECLARED}" >/dev/null 2>&1; then
                report "GitHub release" "${GH_LATEST}" "v${DECLARED} released" "${GREEN}"
            else
                report "GitHub release" "${GH_LATEST}" "v${DECLARED} MISSING" "${RED}"
                note_problem
            fi
        elif gh release view "v${DECLARED}" >/dev/null 2>&1; then
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
        if [[ ${RELEASED} -eq 1 ]]; then
            if npm view "triepack@${DECLARED}" version >/dev/null 2>&1; then
                report "npm triepack" "${NPM_LATEST:-none}" "${DECLARED} published" "${GREEN}"
            else
                report "npm triepack" "${NPM_LATEST:-none}" "${DECLARED} MISSING" "${RED}"
                note_problem
            fi
        elif [[ -z "${NPM_LATEST}" ]]; then
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
    if [[ ${RELEASED} -eq 1 ]]; then
        if echo "${PYPI_JSON}" | python3 -c "
import json, sys
d = json.load(sys.stdin)
sys.exit(0 if '${DECLARED}' in d.get('releases', {}) else 1)
" 2>/dev/null; then
            report "PyPI triepack" "${DECLARED}" "${DECLARED} published" "${GREEN}"
        else
            report "PyPI triepack" "?" "${DECLARED} MISSING" "${RED}"
            note_problem
        fi
    elif [[ -z "${PYPI_JSON}" ]]; then
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
if [[ ${RELEASED} -eq 1 ]]; then
    if [[ ${PROBLEMS} -ne 0 ]]; then
        echo -e "${RED}${BOLD}${DECLARED} is not fully released.${NC}" >&2
        echo -e "${RED}Each row marked above is out of step. A branch missing the" >&2
        echo -e "tagged commit is fixed with:  git push origin <default branch>${NC}" >&2
        exit 1
    fi
    say "${GREEN}${BOLD}${DECLARED} is released consistently everywhere.${NC}"
    exit 0
fi
if [[ ${PROBLEMS} -ne 0 ]]; then
    echo -e "${RED}${BOLD}${DECLARED} cannot be released.${NC}" >&2
    echo -e "${RED}Bump triepack-version.txt past everything above, run" >&2
    echo -e "./scripts/sync_version.sh, and land the result.${NC}" >&2
    exit 1
fi
say "${GREEN}${BOLD}${DECLARED} is ahead of everything published.${NC}"
