#!/bin/sh
set -e

DIR=$(cd "$(dirname "$0")" && pwd)
CORE=$(cd "$DIR/.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

echo "building..."
g++ -std=c++11 -Wall -Wextra -DQDNSM_HOST_TEST \
    -I "$CORE" \
    -o "$OUT/test" \
    "$DIR/test.cpp" \
    "$CORE/GameList.cpp" \
    "$CORE/DnsList.cpp"

echo
"$OUT/test"
