#!/usr/bin/env python3
"""Generate the compiled qDNSm language tables from data/lang/*.ini.

The generated files are checked in. The build does not run Python; this script is
run by hand whenever a catalog changes.

Each catalog is UTF-8. Values support \\n, \\t, \\r, \\\\, \\", \\uXXXX and
\\UXXXXXXXX escapes. English is LZSS-compressed and loaded when the menu opens;
every other catalog ships as a raw .ini file in qDNSm_lang. Startup and worker
notifications use a small compressed notification table embedded for every
language.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LANG_DIR = ROOT / "data" / "lang"
HEADER_PATH = ROOT / "src" / "Core" / "LanguageIds.generated.hpp"
SOURCE_PATH = ROOT / "src" / "Core" / "LanguageTable.generated.cpp"
INSTALLER_HEADER_PATH = ROOT / "installer_src" / "LanguageFiles.generated.hpp"

LANGUAGES = ("en", "de", "es", "fr", "it", "pt_BR", "pt_PT", "ru", "pl", "ja", "zh_CN")
NOTIFICATION_PREFIXES = ("TEXT_NOTIFY_", "TEXT_UPDATE_")
PLACEHOLDER_RE = re.compile(r"\{([0-9]+)\}")
PUA_START = 0xE000
PUA_END = 0xF8FF

LZSS_WINDOW = 4096
LZSS_MAX_MATCH = 18
LZSS_MIN_MATCH = 3
LZSS_MAX_CHAIN = 64


class CatalogError(Exception):
    pass


def decode_escapes(value, path, line_number):
    out = []
    i = 0
    while i < len(value):
        character = value[i]
        if character != "\\":
            out.append(character)
            i += 1
            continue

        i += 1
        if i >= len(value):
            raise CatalogError(f"{path}:{line_number}: trailing backslash")

        escape = value[i]
        i += 1

        if escape == "n":
            out.append("\n")
        elif escape == "r":
            out.append("\r")
        elif escape == "t":
            out.append("\t")
        elif escape == "\\":
            out.append("\\")
        elif escape == '"':
            out.append('"')
        elif escape in ("u", "U"):
            digits = 4 if escape == "u" else 8
            if i + digits > len(value):
                raise CatalogError(f"{path}:{line_number}: short \\{escape} escape")
            text = value[i:i + digits]
            if not re.fullmatch(r"[0-9A-Fa-f]{" + str(digits) + r"}", text):
                raise CatalogError(f"{path}:{line_number}: invalid \\{escape} escape")
            codepoint = int(text, 16)
            i += digits

            if escape == "u" and 0xD800 <= codepoint <= 0xDBFF:
                if value[i:i + 2] != "\\u" or i + 6 > len(value):
                    raise CatalogError(f"{path}:{line_number}: unpaired high surrogate")
                low_text = value[i + 2:i + 6]
                if not re.fullmatch(r"[0-9A-Fa-f]{4}", low_text):
                    raise CatalogError(f"{path}:{line_number}: invalid low surrogate")
                low = int(low_text, 16)
                if not 0xDC00 <= low <= 0xDFFF:
                    raise CatalogError(f"{path}:{line_number}: invalid low surrogate")
                codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00)
                i += 6

            if codepoint > 0x10FFFF or 0xD800 <= codepoint <= 0xDFFF:
                raise CatalogError(f"{path}:{line_number}: invalid codepoint")
            out.append(chr(codepoint))
        else:
            raise CatalogError(f"{path}:{line_number}: unknown escape \\{escape}")

    return "".join(out)


def parse_catalog(path):
    entries = {}
    order = []

    with path.open("r", encoding="utf-8", newline="") as handle:
        for line_number, raw in enumerate(handle, 1):
            line = raw.rstrip("\r\n")
            stripped = line.strip()
            if not stripped or stripped.startswith("#") or stripped.startswith(";"):
                continue

            if "=" not in line:
                raise CatalogError(f"{path}:{line_number}: expected KEY=VALUE")

            key, value = line.split("=", 1)
            key = key.strip()
            if not key:
                raise CatalogError(f"{path}:{line_number}: empty key")
            if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", key):
                raise CatalogError(f"{path}:{line_number}: invalid key {key!r}")
            if key in entries:
                raise CatalogError(f"{path}:{line_number}: duplicate key {key}")

            entries[key] = decode_escapes(value.strip(), path, line_number)
            order.append(key)

    if not entries:
        raise CatalogError(f"{path}: empty catalog")

    return entries, order


def validate(english, translated, path):
    english_keys = set(english)
    translated_keys = set(translated)

    missing = sorted(english_keys - translated_keys)
    extra = sorted(translated_keys - english_keys)
    if missing:
        raise CatalogError(f"{path}: missing keys: {', '.join(missing)}")
    if extra:
        raise CatalogError(f"{path}: unknown keys: {', '.join(extra)}")

    for key, source in english.items():
        text = translated[key]
        if not text:
            raise CatalogError(f"{path}: empty translation for {key}")

        if set(PLACEHOLDER_RE.findall(source)) != set(PLACEHOLDER_RE.findall(text)):
            raise CatalogError(f"{path}: placeholder mismatch for {key}")

        source_glyphs = {c for c in source if PUA_START <= ord(c) <= PUA_END}
        text_glyphs = {c for c in text if PUA_START <= ord(c) <= PUA_END}
        if source_glyphs != text_glyphs:
            raise CatalogError(f"{path}: glyph mismatch for {key}")


def catalog_bytes(catalog, order):
    return b"\x00".join(catalog[key].encode("utf-8") for key in order) + b"\x00"


def lzss_compress(data):
    size = len(data)
    position = 0
    output = bytearray()
    flags = 0
    flag_position = 0
    bit = 8
    chains = {}

    while position < size:
        if bit == 8:
            flag_position = len(output)
            output.append(0)
            flags = 0
            bit = 0

        best_length = 0
        best_distance = 0

        if position + LZSS_MIN_MATCH <= size:
            key = data[position:position + LZSS_MIN_MATCH]
            candidates = chains.get(key, [])

            for candidate in reversed(candidates[-LZSS_MAX_CHAIN:]):
                distance = position - candidate
                if distance > LZSS_WINDOW:
                    break

                length = 0
                while (length < LZSS_MAX_MATCH and
                       position + length < size and
                       data[candidate + length] == data[position + length]):
                    length += 1

                if length > best_length:
                    best_length = length
                    best_distance = distance
                    if length == LZSS_MAX_MATCH:
                        break

        if best_length >= LZSS_MIN_MATCH:
            output.append((best_distance - 1) & 0xFF)
            output.append((((best_distance - 1) >> 8) & 0x0F) |
                          ((best_length - LZSS_MIN_MATCH) << 4))
            flags |= 1 << bit

            for index in range(position, min(position + best_length, size - 2)):
                chains.setdefault(data[index:index + LZSS_MIN_MATCH], []).append(index)

            position += best_length
        else:
            output.append(data[position])

            if position + LZSS_MIN_MATCH <= size:
                chains.setdefault(data[position:position + LZSS_MIN_MATCH], []).append(position)

            position += 1

        bit += 1
        if bit == 8:
            output[flag_position] = flags

    if bit:
        output[flag_position] = flags

    return bytes(output)


def lzss_decompress(data, raw_size):
    output = bytearray()
    position = 0

    while len(output) < raw_size:
        if position >= len(data):
            raise CatalogError("compressed stream ended before raw data")

        flags = data[position]
        position += 1

        for bit in range(8):
            if len(output) >= raw_size:
                break

            if (flags & (1 << bit)) == 0:
                if position >= len(data):
                    raise CatalogError("compressed literal truncated")
                output.append(data[position])
                position += 1
                continue

            if position + 1 >= len(data):
                raise CatalogError("compressed match truncated")

            low = data[position]
            high = data[position + 1]
            position += 2
            distance = ((high & 0x0F) << 8) + low + 1
            length = (high >> 4) + 3

            if distance > len(output):
                raise CatalogError("compressed match before output start")

            for _ in range(length):
                if len(output) >= raw_size:
                    break
                output.append(output[-distance])

    return bytes(output)


def write_blob(source, name, data):
    source.append(f"static const unsigned char {name}[{len(data)}] =")
    source.append("{")
    for index in range(0, len(data), 20):
        values = ", ".join(str(value) for value in data[index:index + 20])
        source.append(f"\t{values},")
    source.append("};")
    source.append("")


def compressed_catalog(code, raw):
    packed = lzss_compress(raw)

    if len(raw) > 0xFFFF or len(packed) > 0xFFFF:
        raise CatalogError(f"{code}: catalog is too large for its size fields")

    if lzss_decompress(packed, len(raw)) != raw:
        raise CatalogError(f"{code}: LZSS round-trip failed")

    return packed, len(raw)


def write_outputs(english, order, catalogs):
    notification_keys = [
        key for key in order if key.startswith(NOTIFICATION_PREFIXES)
    ]

    header = []
    header.append("// Generated by tools/gen-language.py. Do not edit.")
    header.append("#pragma once")
    header.append("")
    header.append("enum TextId")
    header.append("{")
    for key in order:
        header.append(f"\t{key},")
    header.append("\tTEXT_COUNT")
    header.append("};")
    header.append("")
    header.append("extern const char* const kLanguageKeys[TEXT_COUNT];")
    header.append("")

    source = []
    source.append("// Generated by tools/gen-language.py. Do not edit.")
    source.append('#include "LanguageIds.generated.hpp"')
    source.append('#include "LanguageCompression.hpp"')
    source.append("")
    source.append("const char* const kLanguageKeys[TEXT_COUNT] =")
    source.append("{")
    for key in order:
        source.append(f'\t"{key}",')
    source.append("};")
    source.append("")

    english_raw = catalog_bytes(english, order)
    english_packed, english_raw_size = compressed_catalog("en", english_raw)
    write_blob(source, "kLanguageBlob_en", english_packed)
    source.append("const LanguageCatalogEntry kLanguageCatalogs[] =")
    source.append("{")
    source.append(f'\t{{ "en", kLanguageBlob_en, {len(english_packed)}, {english_raw_size} }},')
    source.append("};")
    source.append("const int kLanguageCatalogCount = sizeof(kLanguageCatalogs) / sizeof(kLanguageCatalogs[0]);")
    source.append("")

    max_notification_raw_size = 0
    notification_catalogs = []

    for code in LANGUAGES:
        raw = catalog_bytes(catalogs[code], notification_keys)
        packed, raw_size = compressed_catalog(code, raw)
        max_notification_raw_size = max(max_notification_raw_size, raw_size)
        notification_catalogs.append((code, packed, raw_size))
        write_blob(source, f"kLanguageNotifyBlob_{code}", packed)

    source.append("const int kLanguageNotificationIds[] =")
    source.append("{")
    for key in notification_keys:
        source.append(f"\t{key},")
    source.append("};")
    source.append("const int kLanguageNotificationCount = sizeof(kLanguageNotificationIds) / sizeof(kLanguageNotificationIds[0]);")
    source.append(f"const unsigned int kLanguageNotificationMaxRawSize = {max_notification_raw_size};")
    source.append("")
    source.append("const LanguageCatalogEntry kLanguageNotificationCatalogs[] =")
    source.append("{")
    for code, packed, raw_size in notification_catalogs:
        source.append(f'\t{{ "{code}", kLanguageNotifyBlob_{code}, {len(packed)}, {raw_size} }},')
    source.append("};")
    source.append("const int kLanguageNotificationCatalogCount = sizeof(kLanguageNotificationCatalogs) / sizeof(kLanguageNotificationCatalogs[0]);")
    source.append("")

    installer_header = []
    installer_header.append("// Generated by tools/gen-language.py. Do not edit.")
    installer_header.append("#pragma once")
    installer_header.append("")
    installer_header.append("static const char* const kLanguageFiles[] =")
    installer_header.append("{")
    for code in LANGUAGES:
        if code == "en":
            continue
        installer_header.append(f'\t"{code}.ini",')
    installer_header.append("};")
    installer_header.append("static const int kLanguageFileCount = sizeof(kLanguageFiles) / sizeof(kLanguageFiles[0]);")
    installer_header.append("")

    HEADER_PATH.write_text("\n".join(header), encoding="utf-8", newline="\n")
    SOURCE_PATH.write_text("\n".join(source), encoding="utf-8", newline="\n")
    INSTALLER_HEADER_PATH.write_text(
        "\n".join(installer_header), encoding="utf-8", newline="\n")


def main():
    english_path = LANG_DIR / "en.ini"
    if not english_path.exists():
        print(f"error: missing {english_path}", file=sys.stderr)
        return 1

    try:
        english, order = parse_catalog(english_path)
        catalogs = {}
        for code in LANGUAGES:
            path = LANG_DIR / f"{code}.ini"
            if not path.exists():
                raise CatalogError(f"missing catalog {path}")
            catalog, catalog_order = parse_catalog(path)
            validate(english, catalog, path)
            if catalog_order != order:
                raise CatalogError(f"{path}: key order differs from en.ini")
            catalogs[code] = catalog

        write_outputs(english, order, catalogs)
    except CatalogError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(
        f"generated {HEADER_PATH.relative_to(ROOT)}, "
        f"{SOURCE_PATH.relative_to(ROOT)} and "
        f"{INSTALLER_HEADER_PATH.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
