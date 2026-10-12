#!/bin/sh
# Runs the boot_plugins.txt editor tests on a normal host machine.
#
# BootPlugins.cpp is compiled unmodified - the only thing faked is the cellFs
# layer underneath it, so these tests exercise the code that actually ships
# rather than a reimplementation of it. No PS3, no SDK and no console needed.
#
# Usage:  sh installer_src/tests/run-tests.sh
# Exits non-zero if any test fails.

set -e

DIR=$(cd "$(dirname "$0")" && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

echo "building..."
g++ -std=c++11 -Wall -Wextra \
    -I "$DIR/.." \
    -o "$OUT/test" \
    "$DIR/test.cpp" \
    "$DIR/fakefs.cpp" \
    "$DIR/../BootPlugins.cpp"

echo
"$OUT/test"
