#!/bin/sh

set -e

DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$DIR/../.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

echo "building..."

g++ -std=c++11 -Wall -Wextra \
    -I "$DIR/.." \
    -I "$ROOT/installer_src" \
    -o "$OUT/test" \
    "$DIR/test.cpp" \
    "$DIR/fake_syscalls.cpp" \
    "$ROOT/installer_src/tests/fakefs.cpp" \
    "$DIR/../LauncherLoader.cpp" \
    "$DIR/../LauncherFiles.cpp"

echo
"$OUT/test"
