#!/usr/bin/env bash
# CHML final-newline check.
#
# Every tracked file whose name ends in .c, .h, .md or .sh must end with a line
# feed (0x0A). A single rule covers both repository conventions: an LF file ends
# with 0x0A and a CRLF file also ends with 0x0A. A file ending in a bare CR, in
# a space, or in any other byte is a violation. Empty files are accepted: there
# is no last line to terminate.
#
# The inventory is Git's, not the filesystem's, so build output, editor backup
# files and any other untracked material cannot influence the result. Paths are
# read NUL-separated from `git ls-files -z` and `find -print0`, so names that
# contain spaces, quotes or non-ASCII bytes are handled verbatim.
#
# Modes:
#   check-final-newline.sh tracked [repo-root]   check the tracked tree
#   check-final-newline.sh dir <path>            check a directory (fixtures)
#   check-final-newline.sh controls              self-test, retained in the gate
#
# Exit: 0 = every checked file ends with LF
#       1 = at least one violation
#       2 = usage, environment or internal error, including a skipped file
#
# scripts/check-final-newline.ps1 is the Windows counterpart. The two must
# produce the same verdicts; both `controls` modes assert the same fixture table
# and print the same case identifiers so the two outputs can be compared.

set -uo pipefail

PROG=${0##*/}
SELF_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SELF="$SELF_DIR/${BASH_SOURCE[0]##*/}"

usage() {
    printf 'usage: %s tracked [repo-root] | dir <path> | controls\n' "$PROG" >&2
}

# Only these extensions are in scope. Keep this list in step with the
# PowerShell counterpart.
is_target() {
    case $1 in
        *.c | *.h | *.md | *.sh) return 0 ;;
        *) return 1 ;;
    esac
}

# check_inventory <label> <root> <nul-separated list of paths relative to root>
check_inventory() {
    local label=$1 root=$2 list=$3
    local path display size checked=0 violations=0 empty=0 skipped=0

    while IFS= read -r -d '' path; do
        is_target "$path" || continue
        if [ ! -f "$root/$path" ]; then
            skipped=$((skipped + 1))
            printf '  not a regular file: %s\n' "${path#./}"
            continue
        fi
        size=$(wc -c <"$root/$path" 2>/dev/null)
        size=${size//[[:space:]]/}
        if [ -z "$size" ]; then
            skipped=$((skipped + 1))
            printf '  unreadable: %s\n' "${path#./}"
            continue
        fi
        checked=$((checked + 1))
        if [ "$size" -eq 0 ]; then
            empty=$((empty + 1))
            continue
        fi
        if [ "$(tail -c 1 -- "$root/$path" | od -An -tu1 | tr -d ' \n')" = "10" ]; then
            continue
        fi
        violations=$((violations + 1))
        display=${path#./}
        printf '  missing final newline: %s\n' "$display"
    done <"$list"

    printf '%s: %d checked (%d empty), %d violation(s), %d skipped\n' \
        "$label" "$checked" "$empty" "$violations" "$skipped"

    # A file the check could not read is not a file the check passed.
    if [ "$skipped" -ne 0 ]; then
        return 2
    fi
    if [ "$violations" -ne 0 ]; then
        return 1
    fi
    return 0
}

new_list() {
    mktemp "${TMPDIR:-/tmp}/chml-newline.XXXXXX"
}

check_tracked() {
    local root=${1:-} list rc
    if [ -z "$root" ]; then
        root=$(git rev-parse --show-toplevel 2>/dev/null) || {
            printf 'error: not inside a Git work tree\n' >&2
            return 2
        }
    fi
    if [ ! -d "$root" ]; then
        printf 'error: no such directory: %s\n' "$root" >&2
        return 2
    fi
    list=$(new_list) || return 2
    if ! git -C "$root" ls-files -z >"$list"; then
        printf 'error: git ls-files failed in %s\n' "$root" >&2
        rm -f "$list"
        return 2
    fi
    check_inventory "final newline (tracked)" "$root" "$list"
    rc=$?
    rm -f "$list"
    return "$rc"
}

check_dir() {
    local root=${1:-} list rc
    if [ -z "$root" ] || [ ! -d "$root" ]; then
        printf 'error: dir mode needs an existing directory\n' >&2
        return 2
    fi
    root=${root%/}
    list=$(new_list) || return 2
    # Inventory relative to root, so `find` and `git ls-files` reach the same
    # code path.
    if ! (cd "$root" && find . -type f -print0) >"$list" 2>/dev/null; then
        rm -f "$list"
        return 2
    fi
    check_inventory "final newline (fixture)" "$root" "$list"
    rc=$?
    rm -f "$list"
    return "$rc"
}

# ---------------------------------------------------------------- controls ---

CONTROL_FAILURES=0
CONTROL_TMP=

cleanup_controls() {
    if [ -n "$CONTROL_TMP" ] && [ -d "$CONTROL_TMP" ]; then
        rm -rf "$CONTROL_TMP"
    fi
}

control_ok() { printf '  ok   %s\n' "$1"; }
control_bad() {
    CONTROL_FAILURES=$((CONTROL_FAILURES + 1))
    printf '  FAIL %s\n' "$1"
}

run_controls() {
    local pass fail out rc case

    CONTROL_TMP=$(mktemp -d "${TMPDIR:-/tmp}/chml-newline-ctl.XXXXXX") || return 2
    trap cleanup_controls EXIT
    pass="$CONTROL_TMP/pass"
    fail="$CONTROL_TMP/fail"
    mkdir -p "$pass/sub dir" "$fail/sub"

    # Accepted: LF, CRLF, empty, an out-of-scope extension, and a space in a
    # directory name.
    printf 'line\n' >"$pass/lf.md"
    printf 'line\r\n' >"$pass/crlf.c"
    : >"$pass/empty.h"
    printf 'no newline here' >"$pass/ignore.txt"
    printf 'x\n' >"$pass/sub dir/nested.sh"

    # Refused: missing LF, bare CR, and names containing a space or a quote.
    printf 'no newline' >"$fail/missing.md"
    printf 'line\r' >"$fail/bare-cr.c"
    printf 'no newline' >"$fail/space name.md"
    printf 'no newline' >"$fail/quote'name.md"
    printf 'no newline' >"$fail/sub/missing.h"
    printf 'no newline' >"$fail/ignore.txt"

    # Invoked through bash explicitly: the executable bit is not reliable across
    # checkouts (a Windows checkout stores no mode), and this same script is
    # run by the Linux gate as `bash check-final-newline.sh`.
    out=$(bash "$SELF" dir "$pass" 2>&1)
    rc=$?
    if [ "$rc" -eq 0 ]; then
        control_ok "accept-lf+crlf+empty: positive fixture passes"
    else
        control_bad "accept-lf+crlf+empty: refused with rc=$rc: $out"
    fi
    case $out in
        *"0 violation"*) control_ok "positive-report: zero violations" ;;
        *) control_bad "positive-report: $out" ;;
    esac
    case $out in
        *"1 empty"*) control_ok "accept-empty: empty file counted, not refused" ;;
        *) control_bad "accept-empty: $out" ;;
    esac

    out=$(bash "$SELF" dir "$fail" 2>&1)
    rc=$?
    if [ "$rc" -eq 1 ]; then
        control_ok "reject-missing-lf: nonzero exit status"
    else
        control_bad "reject-missing-lf: expected exit 1, got $rc"
    fi
    for case in "missing.md" "bare-cr.c" "space name.md" "quote'name.md" "sub/missing.h"; do
        case $out in
            *"$case"*) control_ok "reject-$case: reported" ;;
            *) control_bad "reject-$case: not reported" ;;
        esac
    done
    case $out in
        *"ignore.txt"*) control_bad "ignore-foreign-extension: reported an out-of-scope file" ;;
        *) control_ok "ignore-foreign-extension: not reported" ;;
    esac
    case $out in
        *"5 violation"*) control_ok "reject-count: exactly five violations" ;;
        *) control_bad "reject-count: $out" ;;
    esac

    if [ "$CONTROL_FAILURES" -ne 0 ]; then
        printf '%d final-newline control check(s) failed\n' "$CONTROL_FAILURES" >&2
        return 1
    fi
    printf 'final-newline controls passed\n'
    return 0
}

case ${1:-} in
    tracked) check_tracked "${2:-}" ;;
    dir) check_dir "${2:-}" ;;
    controls) run_controls ;;
    *)
        usage
        exit 2
        ;;
esac
