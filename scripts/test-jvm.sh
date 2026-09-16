#!/usr/bin/env bash
#
# test-jvm.sh — build and test the Java and Kotlin bindings without Gradle.
#
# Both bindings are a single package with no dependencies beyond JUnit, so a
# build system is not needed to compile and test them: javac and kotlinc with
# the JUnit console launcher is enough. CI uses Gradle because it is already
# there; this is for a laptop that does not have it.
#
# Anything missing is fetched into a cache directory (gitignored) rather than
# installed system-wide, so nothing here touches the rest of the machine:
#
#   a JDK        only if there is no working javac. macOS ships a javac *stub*
#                that is not a compiler, so presence on PATH is not enough.
#   JUnit        junit-platform-console-standalone, ~2.6 MB
#   kotlinc      the Kotlin compiler, ~80 MB, only for the Kotlin target
#
#   ./scripts/test-jvm.sh            # both bindings
#   ./scripts/test-jvm.sh java
#   ./scripts/test-jvm.sh kotlin
#
# Set JAVA_HOME to use a JDK you already have. Set TRIEPACK_JVM_CACHE to put
# the downloads somewhere other than .jvm-toolchain/ in the repository.
#
# Exit status: 0 if the selected suites pass, 1 otherwise, 2 on usage error.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

if [[ -t 1 ]]; then
    GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; BOLD='\033[1m'; NC='\033[0m'
else
    GREEN=''; YELLOW=''; RED=''; BOLD=''; NC=''
fi

CACHE="${TRIEPACK_JVM_CACHE:-${PROJECT_ROOT}/.jvm-toolchain}"
JDK_FEATURE=21
JUNIT_VERSION="1.10.0"
KOTLIN_VERSION="1.9.24"

TARGET="${1:-both}"
case "${TARGET}" in
    java|kotlin|both) ;;
    -h|--help)
        sed -n '3,26p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
        exit 0
        ;;
    *)
        echo "Usage: $0 [java|kotlin|both]" >&2
        exit 2
        ;;
esac

info() { echo -e "  ${YELLOW}$*${NC}"; }
ok()   { echo -e "  ${GREEN}pass${NC}  $*"; }
die()  { echo -e "${RED}${BOLD}error:${NC} $*" >&2; exit 1; }

mkdir -p "${CACHE}"

# --------------------------------------------------------------------------
# JDK
#
# A working javac, not merely a javac on PATH: /usr/bin/javac on macOS is a
# stub that errors out when no JDK is installed.
# --------------------------------------------------------------------------
find_jdk() {
    local candidate

    if [[ -n "${JAVA_HOME:-}" && -x "${JAVA_HOME}/bin/javac" ]]; then
        if "${JAVA_HOME}/bin/javac" -version >/dev/null 2>&1; then
            echo "${JAVA_HOME}"
            return 0
        fi
    fi

    if command -v javac >/dev/null 2>&1 && javac -version >/dev/null 2>&1; then
        candidate="$(dirname "$(dirname "$(command -v javac)")")"
        echo "${candidate}"
        return 0
    fi

    # Anything already downloaded here.
    for candidate in "${CACHE}"/jdk-*/Contents/Home "${CACHE}"/jdk-*; do
        if [[ -x "${candidate}/bin/javac" ]] && "${candidate}/bin/javac" -version >/dev/null 2>&1; then
            echo "${candidate}"
            return 0
        fi
    done

    return 1
}

download_jdk() {
    local os arch url archive
    case "$(uname -s)" in
        Darwin) os=mac ;;
        Linux)  os=linux ;;
        *)      die "unsupported OS $(uname -s); set JAVA_HOME to a JDK" ;;
    esac
    case "$(uname -m)" in
        arm64|aarch64) arch=aarch64 ;;
        x86_64|amd64)  arch=x64 ;;
        *)             die "unsupported architecture $(uname -m); set JAVA_HOME to a JDK" ;;
    esac

    url="https://api.adoptium.net/v3/binary/latest/${JDK_FEATURE}/ga/${os}/${arch}/jdk/hotspot/normal/eclipse"
    archive="${CACHE}/jdk.tar.gz"

    info "no working JDK found; fetching Temurin ${JDK_FEATURE} for ${os}/${arch} (~200 MB, once)"
    curl -fsSL "${url}" -o "${archive}" || die "JDK download failed"
    tar xzf "${archive}" -C "${CACHE}"
    rm -f "${archive}"

    find_jdk || die "downloaded a JDK but cannot find javac in it"
}

JAVA_HOME_RESOLVED="$(find_jdk || true)"
if [[ -z "${JAVA_HOME_RESOLVED}" ]]; then
    JAVA_HOME_RESOLVED="$(download_jdk)"
fi
JAVAC="${JAVA_HOME_RESOLVED}/bin/javac"
JAVA="${JAVA_HOME_RESOLVED}/bin/java"
ok "JDK $("${JAVAC}" -version 2>&1 | awk '{print $2}')  (${JAVA_HOME_RESOLVED})"

# --------------------------------------------------------------------------
# JUnit console launcher
# --------------------------------------------------------------------------
JUNIT_JAR="${CACHE}/junit-platform-console-standalone-${JUNIT_VERSION}.jar"
if [[ ! -f "${JUNIT_JAR}" ]]; then
    info "fetching JUnit ${JUNIT_VERSION} console launcher"
    curl -fsSL \
        "https://repo1.maven.org/maven2/org/junit/platform/junit-platform-console-standalone/${JUNIT_VERSION}/junit-platform-console-standalone-${JUNIT_VERSION}.jar" \
        -o "${JUNIT_JAR}" || die "JUnit download failed"
fi
ok "JUnit ${JUNIT_VERSION}"

# --------------------------------------------------------------------------
# Run one binding's suite. Args: label, classes dir, extra classpath
# --------------------------------------------------------------------------
run_suite() {
    local label="$1" classes="$2" extra_cp="$3"
    local cp="${classes}"
    [[ -n "${extra_cp}" ]] && cp="${cp}:${extra_cp}"

    local out
    if ! out=$("${JAVA}" -jar "${JUNIT_JAR}" execute \
                  --class-path "${cp}" \
                  --select-package com.deftio.triepack \
                  --details=summary 2>&1); then
        echo "${out}" | tail -30
        die "${label} tests failed"
    fi

    local passed failed
    passed=$(echo "${out}" | grep -oE '[0-9]+ tests successful' | grep -oE '^[0-9]+' || echo 0)
    failed=$(echo "${out}" | grep -oE '[0-9]+ tests failed'     | grep -oE '^[0-9]+' || echo 0)
    if [[ "${failed}" != "0" ]]; then
        echo "${out}" | tail -30
        die "${label}: ${failed} test(s) failed"
    fi
    ok "${label}: ${passed} tests"
}

# --------------------------------------------------------------------------
# Java
# --------------------------------------------------------------------------
if [[ "${TARGET}" == "java" || "${TARGET}" == "both" ]]; then
    echo -e "\n${BOLD}Java${NC}"
    CLASSES="${CACHE}/out-java"
    rm -rf "${CLASSES}"; mkdir -p "${CLASSES}"
    "${JAVAC}" -d "${CLASSES}" -cp "${JUNIT_JAR}" \
        bindings/java/src/main/java/com/deftio/triepack/*.java \
        bindings/java/src/test/java/com/deftio/triepack/*.java \
        || die "Java compilation failed"
    ok "compiled"
    # The conformance harness walks up from the working directory to find
    # tests/conformance, so run from the binding directory as Gradle would.
    (cd bindings/java && run_suite "Java" "${CLASSES}" "")
fi

# --------------------------------------------------------------------------
# Kotlin
# --------------------------------------------------------------------------
if [[ "${TARGET}" == "kotlin" || "${TARGET}" == "both" ]]; then
    echo -e "\n${BOLD}Kotlin${NC}"

    KOTLINC_DIR="${CACHE}/kotlinc"
    if [[ ! -x "${KOTLINC_DIR}/bin/kotlinc" ]]; then
        info "fetching Kotlin ${KOTLIN_VERSION} compiler (~80 MB, once)"
        curl -fsSL \
            "https://github.com/JetBrains/kotlin/releases/download/v${KOTLIN_VERSION}/kotlin-compiler-${KOTLIN_VERSION}.zip" \
            -o "${CACHE}/kotlinc.zip" || die "Kotlin compiler download failed"
        (cd "${CACHE}" && unzip -q -o kotlinc.zip && rm -f kotlinc.zip)
    fi
    ok "kotlinc ${KOTLIN_VERSION}"

    KT_LIB="${KOTLINC_DIR}/lib"
    KT_CP="${KT_LIB}/kotlin-test.jar:${KT_LIB}/kotlin-test-junit5.jar:${JUNIT_JAR}"
    CLASSES="${CACHE}/out-kotlin"
    rm -rf "${CLASSES}"

    JAVA_HOME="${JAVA_HOME_RESOLVED}" "${KOTLINC_DIR}/bin/kotlinc" \
        -cp "${KT_CP}" -d "${CLASSES}" -nowarn \
        bindings/kotlin/src/main/kotlin/com/deftio/triepack/*.kt \
        bindings/kotlin/src/test/kotlin/com/deftio/triepack/*.kt \
        2>&1 | grep -E "error:" && die "Kotlin compilation failed"
    ok "compiled"

    (cd bindings/kotlin && run_suite "Kotlin" "${CLASSES}" \
        "${KT_LIB}/kotlin-stdlib.jar:${KT_LIB}/kotlin-test.jar:${KT_LIB}/kotlin-test-junit5.jar")
fi

echo -e "\n${GREEN}${BOLD}JVM bindings pass.${NC}"
