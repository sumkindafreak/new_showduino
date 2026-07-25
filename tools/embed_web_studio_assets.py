#!/usr/bin/env python3
"""Regenerate C3 WebStudioAssets.h from web/showduino-studio."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
WEB_ROOT = ROOT / "web" / "showduino-studio"
OUT_HEADER = ROOT / "firmware" / "c3-supermini-espnow-bridge" / "ShowduinoC3SuperMiniBridge" / "src" / "WebStudioAssets.h"


def mime_for(path: Path) -> str:
    ext = path.suffix.lower()
    if ext == ".html":
      return "text/html"
    if ext == ".css":
      return "text/css"
    if ext == ".js":
      return "application/javascript"
    if ext == ".json":
      return "application/json"
    if ext == ".md":
      return "text/markdown"
    return "application/octet-stream"


def format_bytes(data: bytes) -> list[str]:
    lines: list[str] = []
    chunk: list[str] = []
    for value in data:
        chunk.append(str(value))
        if len(chunk) == 16:
            lines.append("  " + ", ".join(chunk) + ",")
            chunk = []
    if chunk:
        lines.append("  " + ", ".join(chunk))
    return lines


def main() -> None:
    files = sorted(path for path in WEB_ROOT.rglob("*") if path.is_file())
    entries = []
    lines = [
        "#ifndef SHOWDUINO_C3_WEB_STUDIO_ASSETS_H",
        "#define SHOWDUINO_C3_WEB_STUDIO_ASSETS_H",
        "",
        "#include <Arduino.h>",
        "",
        "struct WebStudioAsset {",
        "  const char *mime;",
        "  const char *data;",
        "  size_t length;",
        "};",
        "",
    ]

    for index, file_path in enumerate(files):
        rel = "/" + file_path.relative_to(WEB_ROOT).as_posix()
        data = file_path.read_bytes()
        lines.append(f"static const char kAsset_{index}[] PROGMEM = {{")
        lines.extend(format_bytes(data))
        lines.append("};")
        lines.append("")
        entries.append((index, rel, mime_for(file_path), len(data)))

    lines.extend([
        "struct WebStudioAssetEntry {",
        "  const char *path;",
        "  const char *mime;",
        "  const char *data;",
        "  size_t length;",
        "};",
        "",
        "static const WebStudioAssetEntry kEmbeddedAssets[] = {",
    ])

    for index, rel, mime, length in entries:
        lines.append(f'  {{ "{rel}", "{mime}", kAsset_{index}, {length} }},')

    lines.extend([
        "};",
        "",
        f"static const size_t kEmbeddedAssetCount = {len(entries)};",
        "",
        "inline WebStudioAsset getEmbeddedAsset(const char *path) {",
        "  WebStudioAsset out = { nullptr, nullptr, 0 };",
        "  if (!path) return out;",
        "  for (size_t i = 0; i < kEmbeddedAssetCount; i++) {",
        "    if (strcmp(path, kEmbeddedAssets[i].path) == 0) {",
        "      out.mime = kEmbeddedAssets[i].mime;",
        "      out.data = kEmbeddedAssets[i].data;",
        "      out.length = kEmbeddedAssets[i].length;",
        "      return out;",
        "    }",
        "  }",
        "  return out;",
        "}",
        "",
        "#endif /* SHOWDUINO_C3_WEB_STUDIO_ASSETS_H */",
        "",
    ])

    OUT_HEADER.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"Wrote {OUT_HEADER} ({len(entries)} assets)")


if __name__ == "__main__":
    main()
