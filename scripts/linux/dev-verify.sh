#!/usr/bin/env bash
# CHML Linux verification gate.
#
# Verifies the production implementation on a POSIX host. It does not install
# anything, does not modify the toolchain, and does not claim crash durability or
# concurrency safety. Every step records its own exit status so one failure does
# not hide the state of the others.
#
# Usage: scripts/linux/dev-verify.sh
# Exit:  0 = every required step passed
#        1 = a step failed
#        2 = required toolchain is missing (nothing was installed)

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_ROOT="${REPO_ROOT}/build/verify"
LOG_DIR="${BUILD_ROOT}/logs"
STRICT_FLAGS="-std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wstrict-prototypes"

FAILED=0
ROWS=()

record() { # name status detail
    local line
    line="$(printf '%-38s %-8s %s' "$1" "$2" "$3")"
    ROWS+=("$line")
    printf '  %s\n' "$line"
    if [[ "$2" != "PASS" ]]; then FAILED=1; fi
}

section() { printf '\n== %s ==\n' "$1"; }

# ---------------------------------------------------------------- toolchain ---

section "toolchain"

missing=0
for tool in cmake ctest gcc clang; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "MISSING: $tool" >&2
        missing=1
    fi
done
if [[ "$missing" -ne 0 ]]; then
    cat >&2 <<'EOF'

This script does not install dependencies or change the toolchain.
Install the missing tools, then re-run. Required: cmake, ctest, gcc, clang.
EOF
    exit 2
fi

echo "kernel   : $(uname -srm)"
echo "cmake    : $(cmake --version | head -1)"
echo "ctest    : $(ctest --version | head -1)"
echo "gcc      : $(gcc --version | head -1)"
echo "clang    : $(clang --version | head -1)"
echo "build dir: ${BUILD_ROOT}"

mkdir -p "$LOG_DIR"

# --------------------------------------------------------------- build/test ---

# configure_build_test <label> <compiler> <extra flags> [ctest env prefix]
configure_build_test() {
    local label="$1" cc="$2" extra="$3" envprefix="${4:-}"
    local dir="${BUILD_ROOT}/${label}"
    local cfg="${LOG_DIR}/${label}.configure.log"
    local bld="${LOG_DIR}/${label}.build.log"
    local tst="${LOG_DIR}/${label}.ctest.log"

    rm -rf "$dir"
    if ! cmake -S "$REPO_ROOT" -B "$dir" \
            -DCMAKE_C_COMPILER="$cc" \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_C_FLAGS="${STRICT_FLAGS} ${extra}" \
            -DCMAKE_EXE_LINKER_FLAGS="${extra}" >"$cfg" 2>&1; then
        record "$label configure" FAIL "see ${cfg}"
        return 1
    fi

    if ! cmake --build "$dir" --parallel --verbose >"$bld" 2>&1; then
        record "$label build" FAIL "see ${bld}"
        return 1
    fi
    record "$label build" PASS "strict warnings clean"

    if ! eval "${envprefix} ctest --test-dir \"$dir\" --output-on-failure" >"$tst" 2>&1; then
        record "$label ctest" FAIL "see ${tst}"
        return 1
    fi
    local summary
    summary="$(grep -E 'tests passed' "$tst" | tail -1 | sed 's/^ *//')"
    record "$label ctest" PASS "${summary:-see ${tst}}"
    return 0
}

section "GCC strict"
configure_build_test "gcc" gcc ""

section "Clang strict"
configure_build_test "clang" clang ""

# ------------------------------------------------- POSIX path actually ran? ---

section "POSIX atomic replacement"

ID_BIN="${BUILD_ROOT}/gcc/chml_atomic_identity_test"
NEG_BIN="${BUILD_ROOT}/gcc/chml_atomic_identity_negative"

if [[ -x "$ID_BIN" ]]; then
    id_out="$("$ID_BIN" 2>&1)"
    id_rc=$?
    printf '%s\n' "$id_out" | sed 's/^/    /'
    if [[ $id_rc -eq 0 && "$id_out" == *"replace backend: posix-rename"* ]]; then
        record "posix branch executed" PASS "identity changed, backend=posix-rename"
    else
        record "posix branch executed" FAIL "rc=${id_rc}, posix backend line not observed"
    fi
else
    record "posix branch executed" FAIL "identity test binary missing"
fi

# The negative control is the whole point: a test that passes on an in-place
# writer proves nothing. It must fail, and it must fail for the intended reason.
if [[ -x "$NEG_BIN" ]]; then
    neg_out="$("$NEG_BIN" 2>&1)"
    neg_rc=$?
    if [[ $neg_rc -eq 1 && "$neg_out" == *"NEGATIVE_CONTROL_REASON=identity_unchanged"* ]]; then
        record "negative control (in-place write)" PASS "discriminated for the expected reason"
    elif [[ $neg_rc -ne 0 ]]; then
        printf '%s\n' "$neg_out" | sed 's/^/    /'
        record "negative control (in-place write)" FAIL "exited ${neg_rc} for the wrong reason"
    else
        record "negative control (in-place write)" FAIL "in-place writer was not detected"
    fi
else
    record "negative control (in-place write)" FAIL "negative binary missing"
fi

# Temporary files must not survive either outcome.
residue="$(find "$BUILD_ROOT" -type f -name '*.tmp[0-9]*' 2>/dev/null | head -5)"
if [[ -z "$residue" ]]; then
    record "temporary file cleanup" PASS "no *.tmpN residue in the build tree"
else
    record "temporary file cleanup" FAIL "residue: ${residue}"
fi

# ------------------------------------------------------------- sanitizers ----

sanitizer_supported() { # <compiler> <sanitizer>
    echo 'int main(void){return 0;}' |
        "$1" -x c -fsanitize="$2" -o /dev/null - >/dev/null 2>&1
}

# A sanitizer build that silently dropped its flag would still compile and still
# pass its tests. Prove instrumentation instead of inferring it: the flag must
# appear in the compiler command, and the runtime must be visible in the binary.
assert_sanitizer_instrumentation() { # <label> <build dir> <flag> <lib pattern> <symbol pattern>
    local label="$1" dir="$2" flag="$3" lib="$4" sym="$5"
    local bin="${dir}/chml_document_test"

    if ! grep -q -- "$flag" "${LOG_DIR}/${label}.build.log"; then
        record "${label} instrumentation" FAIL "compiler command does not contain ${flag}"
        return 1
    fi
    if ldd "$bin" 2>/dev/null | grep -qi "$lib"; then
        record "${label} instrumentation" PASS "flag applied; runtime ${lib} linked"
        return 0
    fi
    if nm -C "$bin" 2>/dev/null | grep -q "$sym"; then
        record "${label} instrumentation" PASS "flag applied; ${sym} present (static runtime)"
        return 0
    fi
    record "${label} instrumentation" FAIL "flag applied but ${bin} shows no runtime"
    return 1
}

section "AddressSanitizer"
if sanitizer_supported clang address; then
    # Leak detection stays on. Some allocators and container runtimes interfere
    # with ASan; that limitation is documented rather than silently disabled.
    if configure_build_test "asan" clang \
        "-fsanitize=address -fno-omit-frame-pointer -g" \
        "env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1"; then
        assert_sanitizer_instrumentation "asan" "${BUILD_ROOT}/asan" \
            "-fsanitize=address" "libasan" "__asan_"
    fi
else
    record "asan supported" FAIL "clang cannot link -fsanitize=address"
fi

section "UndefinedBehaviorSanitizer"
if sanitizer_supported clang undefined; then
    if configure_build_test "ubsan" clang \
        "-fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g" \
        "env UBSAN_OPTIONS=print_stacktrace=1"; then
        assert_sanitizer_instrumentation "ubsan" "${BUILD_ROOT}/ubsan" \
            "-fsanitize=undefined" "libubsan" "__ubsan_"
    fi
else
    record "ubsan supported" FAIL "clang cannot link -fsanitize=undefined"
fi

# ---------------------------------------------------------------- example ----

section "example output"

EX_DIR="${BUILD_ROOT}/example"
rm -rf "$EX_DIR"
mkdir -p "$EX_DIR"
if [[ -x "${BUILD_ROOT}/gcc/chml_hello" ]]; then
    if ( cd "$EX_DIR" && "${BUILD_ROOT}/gcc/chml_hello" >"${LOG_DIR}/example.log" 2>&1 ); then
        if [[ -s "${EX_DIR}/index.html" && -s "${EX_DIR}/style.css" ]]; then
            charsets="$(grep -o 'charset' "${EX_DIR}/index.html" | wc -l | tr -d ' ')"
            example_tmp="$(find "$EX_DIR" -type f -name '*.tmp[0-9]*' | wc -l | tr -d ' ')"
            if [[ "$charsets" == "1" && "$example_tmp" == "0" ]]; then
                record "example ran" PASS "index.html + style.css, 1 charset, no temp"
                head -c 200 "${EX_DIR}/index.html" | sed 's/^/    /'
                echo
            else
                record "example ran" FAIL "charset=${charsets} temp=${example_tmp}"
            fi
        else
            record "example ran" FAIL "output files missing"
        fi
    else
        record "example ran" FAIL "see ${LOG_DIR}/example.log"
    fi
else
    record "example ran" FAIL "chml_hello binary missing"
fi

# ---------------------------------------------------------------- summary ----

section "summary"
printf '%s\n' "${ROWS[@]}"
echo
echo "Not claimed by this gate: crash durability, power-loss durability,"
echo "concurrent-writer safety, browser conformance, or performance."
if [[ "$FAILED" -ne 0 ]]; then
    echo "CHML verification: FAILED"
    exit 1
fi
echo "CHML verification: PASSED on this host"
exit 0
