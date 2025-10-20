#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: run_memory_checks.sh [--build-dir <path>] [-- [extra test args]]

Runs the ancestrytree test suite under Valgrind to surface memory leaks and
invalid accesses. Provide an alternate build directory if the tests were built
outside of the default ./build tree.
EOF
}

BUILD_DIR="build"
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            shift
            if [[ $# -eq 0 ]]; then
                echo "Missing value for --build-dir" >&2
                exit 2
            fi
            BUILD_DIR="$1"
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        --)
            shift
            EXTRA_ARGS=("$@")
            break
            ;;
        *)
            EXTRA_ARGS+=("$1")
            ;;
    esac
    shift || break
done

if ! command -v valgrind >/dev/null 2>&1; then
    echo "Valgrind is required for this check. Install it via your package manager." >&2
    exit 127
fi

TEST_BINARY="${BUILD_DIR}/bin/ancestrytree_tests"
if [[ ! -x "${TEST_BINARY}" ]]; then
    echo "Test binary not found at ${TEST_BINARY}. Run a build first." >&2
    exit 3
fi

echo "Executing Valgrind memory checks against ${TEST_BINARY}" >&2
valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --error-exitcode=1 \
    "${TEST_BINARY}" "${EXTRA_ARGS[@]}"
