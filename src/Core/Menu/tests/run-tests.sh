#!/bin/sh
# Runs the menu list tests on a normal host machine.
#
# Scrolling.hpp has no UI or SDK dependency, so nothing has to be faked.
#
# Usage:  sh src/Core/Menu/tests/run-tests.sh
# Exits non-zero if any test fails.

set -e

DIR=$(cd "$(dirname "$0")" && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

echo "building..."
g++ -std=c++11 -Wall -Wextra \
    -o "$OUT/test" \
    "$DIR/test.cpp"

echo
"$OUT/test"