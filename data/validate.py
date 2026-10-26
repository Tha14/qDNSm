#!/usr/bin/env python3
"""Validate qDNSm game lists and the managed DNS list."""

import argparse
import ipaddress
import sys
from pathlib import Path


MAX_GAME_FILE = 2 * 1024 * 1024
MAX_DNS_FILE = 64 * 1024
MAX_GAMES_PER_SECTION = 512
MAX_ALIASES = 16
MAX_GAME_LENGTH = 96
MAX_ALIAS_LENGTH = 64
MAX_DNS_ENTRIES = 256


class ValidationError(Exception):
    pass


def read_ascii(path: Path, limit: int) -> list[str]:
    try:
        size = path.stat().st_size
    except OSError as exc:
        raise ValidationError(f"{path}: {exc.strerror or exc}") from exc

    if size > limit:
        raise ValidationError(f"{path}: file is larger than {limit} bytes")

    try:
        data = path.read_bytes()
    except OSError as exc:
        raise ValidationError(f"{path}: {exc.strerror or exc}") from exc

    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]

    try:
        text = data.decode("ascii")
    except UnicodeDecodeError as exc:
        raise ValidationError(f"{path}: non-ASCII byte at offset {exc.start}") from exc

    return text.splitlines()


def trim(text: str) -> str:
    return text.strip(" \t")


def check_text(value: str, what: str) -> None:
    for character in value:
        code = ord(character)
        if code < 0x20 or code > 0x7E:
            raise ValidationError(f"{what}: control or non-ASCII character")


def valid_ipv4(value: str) -> bool:
    try:
        ipaddress.IPv4Address(value)
        return True
    except ipaddress.AddressValueError:
        return False


def parse_game_file(path: Path) -> dict:
    lines = read_ascii(path, MAX_GAME_FILE)
    sections: list[dict] = []
    by_ip: dict[str, dict] = {}
    current: dict | None = None

    for line_number, raw in enumerate(lines, 1):
        line = trim(raw)

        if not line or line.startswith("#") or line.startswith(";"):
            continue

        if line.startswith("["):
            if not line.endswith("]"):
                raise ValidationError(f"{path}:{line_number}: malformed section header")

            ip = trim(line[1:-1])
            if not valid_ipv4(ip):
                raise ValidationError(f"{path}:{line_number}: section key is not an IPv4 address")

            if ip in by_ip:
                raise ValidationError(f"{path}:{line_number}: duplicate section [{ip}]")

            if current is not None:
                finish_game_section(path, current)

            current = {
                "ip": ip,
                "line": line_number,
                "games": [],
                "aliases": [],
                "alias_seen": False,
            }
            sections.append(current)
            by_ip[ip] = current
            continue

        if current is None:
            raise ValidationError(f"{path}:{line_number}: key outside a section")

        if "=" not in line:
            raise ValidationError(f"{path}:{line_number}: expected key=value")

        key, value = line.split("=", 1)
        key = trim(key)
        value = trim(value)

        if not key:
            raise ValidationError(f"{path}:{line_number}: empty key")

        if key == "Game":
            if not value:
                raise ValidationError(f"{path}:{line_number}: empty Game= value")
            if len(value) > MAX_GAME_LENGTH:
                raise ValidationError(f"{path}:{line_number}: game name is longer than {MAX_GAME_LENGTH} characters")
            check_text(value, f"{path}:{line_number}")
            if len(current["games"]) >= MAX_GAMES_PER_SECTION:
                raise ValidationError(f"{path}:{line_number}: more than {MAX_GAMES_PER_SECTION} Game= entries")
            current["games"].append(value)
            continue

        if key == "Aliases":
            if current["alias_seen"]:
                raise ValidationError(f"{path}:{line_number}: duplicate Aliases= key")
            current["alias_seen"] = True

            values = [trim(item) for item in value.split("|")]
            if not values or any(not item for item in values):
                raise ValidationError(f"{path}:{line_number}: empty Aliases= value")
            if len(values) > MAX_ALIASES:
                raise ValidationError(f"{path}:{line_number}: more than {MAX_ALIASES} aliases")
            for item in values:
                if len(item) > MAX_ALIAS_LENGTH:
                    raise ValidationError(f"{path}:{line_number}: alias is longer than {MAX_ALIAS_LENGTH} characters")
                check_text(item, f"{path}:{line_number}")
            current["aliases"].extend(values)
            continue

        if key == "All":
            raise ValidationError(f'{path}:{line_number}: unsupported key "All"')

        raise ValidationError(f'{path}:{line_number}: unsupported key "{key}"')

    if current is not None:
        finish_game_section(path, current)

    total_games = sum(len(section["games"]) for section in sections)
    total_aliases = sum(len(section["aliases"]) for section in sections)
    max_games = max((len(section["games"]) for section in sections), default=0)

    return {
        "sections": sections,
        "games": total_games,
        "aliases": total_aliases,
        "max_games": max_games,
    }


def finish_game_section(path: Path, section: dict) -> None:
    if not section["games"]:
        if section["aliases"]:
            raise ValidationError(f'{path}:{section["line"]}: section has Aliases= but no Game=')
        raise ValidationError(f'{path}:{section["line"]}: empty section [{section["ip"]}]')


def parse_dns_file(path: Path) -> dict:
    lines = read_ascii(path, MAX_DNS_FILE)
    entries: list[dict] = []
    current: dict | None = None
    pairs: set[tuple[str, str]] = set()

    for line_number, raw in enumerate(lines, 1):
        line = trim(raw)

        if not line or line.startswith("#") or line.startswith(";"):
            continue

        if line.startswith("["):
            if not line.endswith("]"):
                raise ValidationError(f"{path}:{line_number}: malformed section header")

            section = trim(line[1:-1])
            if section != "DNS":
                raise ValidationError(f'{path}:{line_number}: unsupported section [{section}]')

            if current is not None:
                finish_dns_section(path, current, entries)

            current = {
                "line": line_number,
                "name": None,
                "primary": None,
                "managed": None,
                "name_seen": False,
                "primary_seen": False,
                "managed_seen": False,
            }
            continue

        if current is None:
            raise ValidationError(f"{path}:{line_number}: key outside a [DNS] section")

        if "=" not in line:
            raise ValidationError(f"{path}:{line_number}: expected key=value")

        key, value = line.split("=", 1)
        key = trim(key)
        value = trim(value)

        if not key:
            raise ValidationError(f"{path}:{line_number}: empty key")

        if key == "Name":
            if current["name_seen"]:
                raise ValidationError(f"{path}:{line_number}: duplicate Name= key")
            current["name_seen"] = True
            if not value:
                raise ValidationError(f"{path}:{line_number}: empty Name= value")
            check_text(value, f"{path}:{line_number}")
            current["name"] = value
            continue

        if key == "Primary":
            if current["primary_seen"]:
                raise ValidationError(f"{path}:{line_number}: duplicate Primary= key")
            current["primary_seen"] = True
            if not valid_ipv4(value):
                raise ValidationError(f"{path}:{line_number}: Primary is not a valid IPv4 address")
            current["primary"] = value
            continue

        if key == "Managed":
            if current["managed_seen"]:
                raise ValidationError(f"{path}:{line_number}: duplicate Managed= key")
            current["managed_seen"] = True
            if value != "1":
                raise ValidationError(f"{path}:{line_number}: Managed must be 1 in the repository list")
            current["managed"] = 1
            continue

        raise ValidationError(f'{path}:{line_number}: unsupported key "{key}"')

    if current is not None:
        finish_dns_section(path, current, entries)

    if not entries:
        raise ValidationError(f"{path}: no DNS entries")

    if len(entries) > MAX_DNS_ENTRIES:
        raise ValidationError(f"{path}: more than {MAX_DNS_ENTRIES} entries")

    for entry in entries:
        pair = (entry["name"].lower(), entry["primary"])
        if pair in pairs:
            raise ValidationError(f'{path}:{entry["line"]}: duplicate entry ({entry["name"]}, {entry["primary"]})')
        pairs.add(pair)

    shared = {}
    for entry in entries:
        shared[entry["primary"]] = shared.get(entry["primary"], 0) + 1

    return {
        "entries": entries,
        "managed": sum(1 for entry in entries if entry["managed"] == 1),
        "shared": sum(1 for count in shared.values() if count > 1),
    }


def finish_dns_section(path: Path, entry: dict, entries: list[dict]) -> None:
    if entry["name"] is None:
        raise ValidationError(f'{path}:{entry["line"]}: missing Name=')
    if entry["primary"] is None:
        raise ValidationError(f'{path}:{entry["line"]}: missing Primary=')
    if entry["managed"] is None:
        raise ValidationError(f'{path}:{entry["line"]}: missing Managed=')

    entries.append(entry)


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate qDNSm data files.")
    parser.add_argument("--game-file", default="data/qDNSm-server-games.ini")
    parser.add_argument("--dns-file", default=None)
    args = parser.parse_args()

    game_path = Path(args.game_file)
    dns_path = Path(args.dns_file) if args.dns_file else None

    try:
        game = parse_game_file(game_path)
    except ValidationError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print(f"OK: {game_path}")
    print(f"  sections: {len(game['sections'])}")
    print(f"  games: {game['games']}")
    print(f"  aliases: {game['aliases']}")
    print(f"  max games in a section: {game['max_games']}")

    if dns_path is None:
        return 0

    try:
        dns = parse_dns_file(dns_path)
    except ValidationError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print(f"OK: {dns_path}")
    print(f"  entries: {len(dns['entries'])}")
    print(f"  managed: {dns['managed']}")
    print(f"  shared primary IPs: {dns['shared']}")

    dns_ips = {entry["primary"] for entry in dns["entries"]}
    for section in game["sections"]:
        if section["ip"] not in dns_ips:
            print(f"WARNING: game section [{section['ip']}] is not in {dns_path}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
