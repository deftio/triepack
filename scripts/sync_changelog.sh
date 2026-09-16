#!/usr/bin/env bash
#
# sync_changelog.sh — generate docs/releases.md from CHANGELOG.md.
#
# What changed in each version is recorded once, in CHANGELOG.md. The docs site
# used to carry a hand-written second copy that said the same things in
# different words, which is two sources of truth and one of them is always out
# of date.
#
#   ./scripts/sync_changelog.sh              # regenerate docs/releases.md
#   ./scripts/sync_changelog.sh --check      # verify only, exit 1 on drift
#
# Exit status: 0 in sync (or successfully synced), 1 on drift in --check mode,
#              2 on usage error.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

if [[ -t 1 ]]; then
    GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
else
    GREEN=''; YELLOW=''; RED=''; NC=''
fi

MODE="sync"
for arg in "$@"; do
    case "$arg" in
        --check) MODE="check" ;;
        -h|--help)
            sed -n '3,15p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown argument: ${arg}${NC}" >&2
            exit 2
            ;;
    esac
done

SOURCE="CHANGELOG.md"
TARGET="docs/releases.md"
[[ -f "${SOURCE}" ]] || { echo -e "${RED}${SOURCE} not found${NC}" >&2; exit 2; }

TMP="$(mktemp)"
trap 'rm -f "${TMP}"' EXIT

python3 - "${SOURCE}" "${TMP}" <<'PY'
import re
import sys

source, target = sys.argv[1], sys.argv[2]
with open(source, encoding="utf-8") as f:
    body = f.read()

# Everything from the first version heading onward; the preamble about Keep a
# Changelog belongs in the repository, not on the site.
match = re.search(r"^## \[", body, re.M)
entries = body[match.start():] if match else ""

# "## [1.2.0] - 2026-09-16" reads better on the site as "## v1.2.0 — 2026-09-16".
entries = re.sub(
    r"^## \[([^\]]+)\](?:\s*-\s*(.+))?$",
    lambda m: "## v%s%s" % (m.group(1), " — %s" % m.group(2).strip() if m.group(2) else ""),
    entries,
    flags=re.M,
)

with open(target, "w", encoding="utf-8") as f:
    f.write(
        "---\n"
        "layout: default\n"
        "title: Releases\n"
        "---\n"
        "\n"
        "<!-- GENERATED from CHANGELOG.md by scripts/sync_changelog.sh.\n"
        "     Do not edit: edit CHANGELOG.md and rerun the script. -->\n"
        "\n"
        "# Releases\n"
        "\n"
        "What changed in each version. Downloads are on the\n"
        "[GitHub Releases page](https://github.com/deftio/triepack/releases);\n"
        "the full history lives in\n"
        "[CHANGELOG.md](https://github.com/deftio/triepack/blob/main/CHANGELOG.md).\n"
        "\n"
        "---\n"
        "\n"
    )
    f.write(entries.rstrip() + "\n")
PY

if [[ -f "${TARGET}" ]] && cmp -s "${TMP}" "${TARGET}"; then
    echo -e "  ${GREEN}ok${NC}      ${TARGET} matches ${SOURCE}"
    exit 0
fi

if [[ "${MODE}" == "check" ]]; then
    echo -e "  ${RED}DRIFT${NC}   ${TARGET} does not match ${SOURCE}" >&2
    echo -e "${RED}Run ./scripts/sync_changelog.sh and commit the result.${NC}" >&2
    exit 1
fi

cp "${TMP}" "${TARGET}"
echo -e "  ${YELLOW}updated${NC} ${TARGET} from ${SOURCE}"
