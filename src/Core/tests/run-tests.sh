#!/bin/sh
set -e

DIR=$(cd "$(dirname "$0")" && pwd)
CORE=$(cd "$DIR/.." && pwd)
LANG_DIR=$(cd "$DIR/../../../data/lang" && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

echo "building..."
g++ -std=c++11 -Wall -Wextra -DQDNSM_HOST_TEST \
    -DQDNSM_TEST_LANG_DIR="\"$LANG_DIR\"" \
    -I "$CORE" \
    -o "$OUT/test" \
    "$DIR/test.cpp" \
    "$CORE/GameList.cpp" \
    "$CORE/GameDirectory.cpp" \
    "$CORE/../Net/DnsList.cpp" \
    "$CORE/Language.cpp" \
    "$CORE/LanguageCatalog.cpp" \
    "$CORE/LanguageCodec.cpp" \
    "$CORE/LanguageCompression.cpp" \
    "$CORE/LanguageTable.generated.cpp" \
    "$CORE/../OpenSpy/OpenSpyPatchPlan.cpp" \
    "$CORE/../OpenSpy/OpenSpySupportedTitles.cpp" \
    "$CORE/../Memory/AobScan.cpp"

echo
"$OUT/test"
