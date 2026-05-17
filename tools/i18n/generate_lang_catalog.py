from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
from typing import Any


PLACEHOLDER_RE = re.compile(r"\{[^{}]*\}")
LANG_STRING_RE = re.compile(r'^LANG_STRING\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*"([^"]+)"\s*\)$')
CATALOG_PREFIX = "overlay_"


def load_string_entries(path: Path) -> tuple[tuple[str, str], ...]:
    entries: list[tuple[str, str]] = []

    for lineno, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("//"):
            continue

        match = LANG_STRING_RE.fullmatch(line)
        if not match:
            raise RuntimeError(f"Unsupported entry in {path}:{lineno}: {raw_line}")

        entries.append((match.group(1), match.group(2)))

    if not entries:
        raise RuntimeError(f"No string entries found in {path}")

    return tuple(entries)


def load_catalog(path: Path) -> dict[str, str]:
    raw: dict[str, Any] = json.loads(path.read_text(encoding="utf-8"))
    catalog: dict[str, str] = {}

    for key, value in raw.items():
        if isinstance(value, str):
            catalog[key] = value
        elif isinstance(value, dict) and isinstance(value.get("message"), str):
            catalog[key] = value["message"]
        else:
            raise RuntimeError(f"Unsupported catalog entry in {path}: {key}")

    return catalog


def locale_tag_from_path(path: Path) -> str:
    stem = path.stem
    if not stem.startswith(CATALOG_PREFIX):
        raise RuntimeError(f"Catalog filename must start with {CATALOG_PREFIX!r}: {path.name}")

    locale_tag = stem[len(CATALOG_PREFIX):]
    if not locale_tag:
        raise RuntimeError(f"Catalog filename is missing a locale tag: {path.name}")

    return locale_tag


def placeholder_set(text: str) -> tuple[str, ...]:
    return tuple(sorted(PLACEHOLDER_RE.findall(text)))


def escape_cpp(text: str) -> str:
    return (
        text.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )


def build_locale(
    locale_tag: str,
    english: dict[str, str],
    translated: dict[str, str],
    string_entries: tuple[tuple[str, str], ...],
) -> dict[str, Any]:
    unknown = sorted(key for key in translated if key not in english)
    if unknown:
        raise RuntimeError(f"{locale_tag} has unknown keys: {unknown}")

    strings: list[str] = []
    for _, key in string_entries:
        source = english[key]
        value = translated.get(key, source)
        if placeholder_set(value) != placeholder_set(source):
            raise RuntimeError(
                f"Placeholder mismatch in {locale_tag}:{key} "
                f"{placeholder_set(source)} != {placeholder_set(value)}"
            )
        strings.append(value)

    return {
        "tag": locale_tag,
        "strings": strings,
    }


def render_string_array(values: list[str], indent: str) -> str:
    body = ",\n".join(f'{indent}"{escape_cpp(value)}"' for value in values)
    return "{\n" + body + "\n" + indent[:-4] + "}"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True)
    parser.add_argument("--strings-def", required=True)
    parser.add_argument("--output-header", required=True)
    parser.add_argument("--output-source", required=True)
    args = parser.parse_args()

    input_dir = Path(args.input_dir)
    strings_def = Path(args.strings_def)
    output_header = Path(args.output_header)
    output_source = Path(args.output_source)
    output_header.parent.mkdir(parents=True, exist_ok=True)
    output_source.parent.mkdir(parents=True, exist_ok=True)

    string_entries = load_string_entries(strings_def)
    expected_keys = tuple(key for _, key in string_entries)
    english_path = input_dir / f"{CATALOG_PREFIX}en.json"
    if not english_path.is_file():
        raise RuntimeError(f"Missing required English catalog: {english_path}")

    english = load_catalog(english_path)

    missing_in_english = [key for key in expected_keys if key not in english]
    if missing_in_english:
        raise RuntimeError(f"English catalog is missing keys: {missing_in_english}")

    catalog_paths = sorted(input_dir.glob(f"{CATALOG_PREFIX}*.json"))
    if not catalog_paths:
        raise RuntimeError(f"No catalogs found in {input_dir} matching {CATALOG_PREFIX}*.json")

    locale_tags = [locale_tag_from_path(path) for path in catalog_paths]
    duplicate_tags = sorted({tag for tag in locale_tags if locale_tags.count(tag) > 1})
    if duplicate_tags:
        raise RuntimeError(f"Duplicate locale tags found: {duplicate_tags}")

    locales = [
        build_locale(locale_tag_from_path(path), english, load_catalog(path), string_entries)
        for path in catalog_paths
    ]
    locales.sort(key=lambda locale: (locale["tag"] != "en", locale["tag"].lower()))

    string_count = len(string_entries)

    header = f"""// Generated by tools/i18n/generate_lang_catalog.py. Do not edit by hand.
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace lang::generated {{

inline constexpr std::size_t k_string_count = {string_count};

struct LocaleData {{
    std::string_view tag;
    std::array<std::string_view, k_string_count> strings;
}};

const LocaleData &english_locale();
const LocaleData *find_locale(std::string_view tag);

}} // namespace lang::generated
"""
    output_header.write_text(header, encoding="utf-8")

    locale_blocks: list[str] = []
    for locale in locales:
        locale_blocks.append(
            "    LocaleData{\n"
            f'        "{escape_cpp(locale["tag"])}",\n'
            f'        {render_string_array(locale["strings"], "            ")}\n'
            "    }"
        )

    source = f"""// Generated by tools/i18n/generate_lang_catalog.py. Do not edit by hand.
#include "generated_catalog.h"

#include <algorithm>
#include <cctype>
#include <util/log.h>

namespace lang::generated {{

namespace {{

constexpr LocaleData k_locales[] = {{
{",\n".join(locale_blocks)}
}};

bool locale_equals(std::string_view lhs, std::string_view rhs) {{
    if (lhs.size() != rhs.size())
        return false;

    for (std::size_t index = 0; index < lhs.size(); ++index) {{
        const char left = lhs[index] == '_' ? '-' : static_cast<char>(std::tolower(static_cast<unsigned char>(lhs[index])));
        const char right = rhs[index] == '_' ? '-' : static_cast<char>(std::tolower(static_cast<unsigned char>(rhs[index])));
        if (left != right)
            return false;
    }}

    return true;
}}

}} // namespace

const LocaleData &english_locale() {{
    const auto *english = find_locale("en");
    if (!english) {{
        LOG_ERROR_ONCE("English locale is missing from the generated catalog. Falling back to the first available locale.");
        return k_locales[0];
    }}

    return *english;
}}

const LocaleData *find_locale(std::string_view tag) {{
    const auto *match = std::find_if(std::begin(k_locales), std::end(k_locales), [tag](const LocaleData &locale) {{
        return locale_equals(locale.tag, tag);
    }});

    return (match == std::end(k_locales)) ? nullptr : match;
}}

}} // namespace lang::generated
"""
    output_source.write_text(source, encoding="utf-8")


if __name__ == "__main__":
    main()
