#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TEST_SRC="$SCRIPT_DIR/tests/test_mpi.c"
TEST_BIN="$SCRIPT_DIR/tests/test_mpi"

echo "Compiling test_mpi.c ..."
gcc -Wall -Wextra -o "$TEST_BIN" "$TEST_SRC"
echo "Compilation successful."
echo ""

echo "Running tests ..."
echo "----------------------------------------"
"$TEST_BIN"
EXIT_CODE=$?
echo "----------------------------------------"

if [ $EXIT_CODE -eq 0 ]; then
    echo "All tests passed."
else
    echo "Some tests failed (exit code $EXIT_CODE)."
fi

exit $EXIT_CODE
