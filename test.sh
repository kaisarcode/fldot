#!/bin/sh
# test.sh
# Summary: Validation suite for fldot functionality.
# Author:  KaisarCode
# Website: https://kaisarcode.com
# License: https://www.gnu.org/licenses/gpl-3.0.html

# Prints one failure line.
# @param $1 Failure message.
# @return 1 on failure.
kc_test_fail() {
    printf '\033[31m[FAIL]\033[0m %s\n' "$1"
    return 1
}

# Prints one success line.
# @param $1 Success message.
# @return 0 on success.
kc_test_pass() {
    printf '\033[32m[PASS]\033[0m %s\n' "$1"
}

# Detects the artifact architecture for the current machine.
# @return Architecture name on stdout.
kc_test_arch() {
    case "$(uname -m)" in
        x86_64 | amd64)
            printf '%s\n' "x86_64"
            ;;
        aarch64 | arm64)
            printf '%s\n' "aarch64"
            ;;
        armv7l | armv7)
            printf '%s\n' "armv7"
            ;;
        i386 | i486 | i586 | i686)
            printf '%s\n' "i686"
            ;;
        ppc64le | powerpc64le)
            printf '%s\n' "powerpc64le"
            ;;
        *)
            uname -m
            ;;
    esac
}

# Detects the artifact platform for the current machine.
# @return Platform name on stdout.
kc_test_platform() {
    case "$(uname -s)" in
        Linux)
            printf '%s\n' "linux"
            ;;
        *)
            uname -s | tr '[:upper:]' '[:lower:]'
            ;;
    esac
}

# Returns the CLI path for the current architecture and platform.
# @return CLI path on stdout.
kc_test_binary_path() {
    printf './bin/%s/%s/fldot\n' "$(kc_test_arch)" "$(kc_test_platform)"
}

# Verifies the binary exists and is executable.
# @return 0 on success, 1 on failure.
kc_test_check_binary() {
    if [ ! -x "$BIN" ]; then
        kc_test_fail "binary not found: $BIN"
        return 1
    fi
    return 0
}

# Tests that the binary exits 0 and creates a .dot file on valid input.
# @return 0 on success, 1 on failure.
kc_test_basic_exec() {
    tmp_dir=$(mktemp -d)
    tmp_flow="$tmp_dir/test.flow"
    printf "flow.id=test\nnode.a.link=b\nnode.b.exec=ls\n" > "$tmp_flow"
    
    if ! "$BIN" -i "$tmp_flow" > /dev/null 2>&1; then
        kc_test_fail "basic file execution"
        rm -rf "$tmp_dir"
        return 1
    fi

    if [ ! -f "$tmp_dir/test.dot" ]; then
        kc_test_fail "derived .dot file creation"
        rm -rf "$tmp_dir"
        return 1
    fi

    if ! grep -q "digraph" "$tmp_dir/test.dot"; then
        kc_test_fail "DOT output validation"
        rm -rf "$tmp_dir"
        return 1
    fi
    
    kc_test_pass "basic file execution"
    rm -rf "$tmp_dir"
    return 0
}

# Tests the explicit -o output path functionality.
# @return 0 on success, 1 on failure.
kc_test_explicit_output() {
    tmp_dir=$(mktemp -d)
    tmp_flow="$tmp_dir/test.flow"
    tmp_out="$tmp_dir/custom.dot"
    printf "flow.id=test\n" > "$tmp_flow"
    
    if ! "$BIN" -i "$tmp_flow" -o "$tmp_out" > /dev/null 2>&1; then
        kc_test_fail "explicit output execution"
        rm -rf "$tmp_dir"
        return 1
    fi

    if [ ! -f "$tmp_out" ]; then
        kc_test_fail "explicit .dot file creation"
        rm -rf "$tmp_dir"
        return 1
    fi
    
    kc_test_pass "explicit output execution"
    rm -rf "$tmp_dir"
    return 0
}

# Runs the full validation suite.
# @return 0 on success, 1 on failure.
kc_test_main() {
    failed=0
    BIN=$(kc_test_binary_path)
    kc_test_check_binary || exit 1
    kc_test_basic_exec      || failed=$((failed + 1))
    kc_test_explicit_output || failed=$((failed + 1))
    return $failed
}

kc_test_main
