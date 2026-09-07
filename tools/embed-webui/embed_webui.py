#!/usr/bin/env python3
"""Convert web/showduino-studio into S3 PROGMEM assets. Do not hand-edit the output.

Source stays ES modules for editing. The embedder inlines the app.js graph into
one /js/app.js so SoftAP does not have to serve 18 parallel module requests.
"""

from __future__ import annotations

import gzip
import hashlib
import json
import os
import posixpath
import re
import sys
from datetime import datetime, timezone

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
WEB_ROOT = os.path.join(ROOT, "web", "showduino-studio")
OUT_H = os.path.join(
    ROOT,
    "firmware",
    "s3-comms-controller",
    "ShowduinoS3CommsController",
    "src",
    "web",
    "WebAssets.generated.h",
)
OUT_JSON = os.path.join(os.path.dirname(__file__), "last-build.json")

MIME = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".json": "application/json",
    ".svg": "image/svg+xml",
    ".png": "image/png",
    ".ico": "image/x-icon",
    ".woff": "font/woff",
    ".woff2": "font/woff2",
}

ENTRY_JS = "/js/app.js"
STATIC_FILES = ("/index.html", "/css/studio.css")
GZIP_MIN = 200

IMPORT_RE = re.compile(
    r"""import\s+(?:type\s+)?(?:[\w*\s{},]+)\s+from\s+['"]([^'"]+)['"]\s*;?""",
    re.MULTILINE,
)
EXPORT_NAME_RE = re.compile(
    r"^export\s+(?:async\s+)?(?:function|const|let|class)\s+([\w$]+)",
    re.MULTILINE,
)


def web_path_to_abs(url: str) -> str:
    rel = url.lstrip("/").replace("/", os.sep)
    return os.path.join(WEB_ROOT, rel)


def read_text(url: str) -> str:
    full = web_path_to_abs(url)
    if not os.path.isfile(full):
        raise FileNotFoundError(f"WebUI import missing: {url}")
    with open(full, "r", encoding="utf-8") as fh:
        return fh.read()


def resolve_import(importer_url: str, spec: str) -> str:
    if spec.startswith("/"):
        return posixpath.normpath(spec)
    base = posixpath.dirname(importer_url)
    resolved = posixpath.normpath(posixpath.join(base, spec))
    if not resolved.startswith("/"):
        resolved = "/" + resolved
    return resolved


def strip_imports_and_exports(src: str) -> str:
    src = IMPORT_RE.sub("", src)
    src = re.sub(r"^export\s+async\s+function\s+", "async function ", src, flags=re.M)
    src = re.sub(r"^export\s+function\s+", "function ", src, flags=re.M)
    src = re.sub(r"^export\s+const\s+", "const ", src, flags=re.M)
    src = re.sub(r"^export\s+let\s+", "let ", src, flags=re.M)
    src = re.sub(r"^export\s+class\s+", "class ", src, flags=re.M)
    src = re.sub(r"^export\s+\{[^}]+\}\s*;?\s*$", "", src, flags=re.M)
    src = re.sub(r"^export\s+default\s+", "", src, flags=re.M)
    if re.search(r"^export\s+", src, re.M):
        raise RuntimeError("Unstripped export remains in WebUI bundle source")
    if re.search(r"^import\s+", src, re.M):
        raise RuntimeError("Unstripped import remains in WebUI bundle source")
    return src.strip() + "\n"


def bundle_js(entry: str) -> tuple[str, list[str]]:
    order: list[str] = []
    seen: set[str] = set()
    exported: dict[str, str] = {}

    def walk(url: str) -> None:
        if url in seen:
            return
        seen.add(url)
        src = read_text(url)
        for spec in IMPORT_RE.findall(src):
            walk(resolve_import(url, spec))
        for name in EXPORT_NAME_RE.findall(src):
            if name in exported and exported[name] != url:
                raise RuntimeError(
                    f"Duplicate export {name} in {url} and {exported[name]}"
                )
            exported[name] = url
        order.append(url)

    walk(entry)
    chunks = ["/* Generated WebUI bundle - do not edit. Source: web/showduino-studio */\n"]
    for url in order:
        chunks.append(f"/* {url} */\n")
        chunks.append(strip_imports_and_exports(read_text(url)))
        chunks.append("\n")
    return "".join(chunks), order


def gzip_bytes(data: bytes) -> bytes:
    return gzip.compress(data, compresslevel=9, mtime=0)


def c_array(name: str, data: bytes) -> list[str]:
    lines = [f"static const uint8_t {name}[] PROGMEM = {{"]
    chunk = []
    for b in data:
        chunk.append(f"0x{b:02x}")
        if len(chunk) >= 16:
            lines.append("  " + ", ".join(chunk) + ",")
            chunk = []
    if chunk:
        lines.append("  " + ", ".join(chunk) + ",")
    lines.append("};")
    lines.append("")
    return lines


def main() -> None:
    bundle_text, module_order = bundle_js(ENTRY_JS)
    bundle = bundle_text.encode("utf-8")

    records: list[tuple[str, str, bytes]] = []
    for path in STATIC_FILES:
        ext = os.path.splitext(path)[1].lower()
        with open(web_path_to_abs(path), "rb") as fh:
            records.append((path, MIME[ext], fh.read()))
    records.append((ENTRY_JS, MIME[".js"], bundle))

    raw_blob = b"".join(data for _path, _mime, data in records)
    digest = hashlib.sha256(raw_blob).hexdigest()
    short = digest[:12]
    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    rewritten = []
    raw_total = 0
    gz_total = 0
    for path, mime, data in records:
        if path == "/index.html":
            text = data.decode("utf-8")
            text = text.replace(
                '<script type="module" src="/js/app.js"></script>',
                f'<script src="/js/app.js?v={short}"></script>',
            )
            text = text.replace("/css/studio.css", f"/css/studio.css?v={short}")
            data = text.encode("utf-8")
        raw_total += len(data)
        gz = gzip_bytes(data) if len(data) >= GZIP_MIN else data
        use_gz = len(data) >= GZIP_MIN and len(gz) < len(data)
        payload = gz if use_gz else data
        gz_total += len(payload)
        rewritten.append((path, mime, data, payload, use_gz))

    lines = [
        "/* Generated by tools/embed-webui/embed_webui.py — do not hand-edit. */",
        "#ifndef SHOWDUINO_S3_WEB_ASSETS_GENERATED_H",
        "#define SHOWDUINO_S3_WEB_ASSETS_GENERATED_H",
        "",
        "#include <Arduino.h>",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        f'#define SHOWDUINO_WEBUI_BUILD_HASH "{short}"',
        f'#define SHOWDUINO_WEBUI_BUILD_UTC "{stamp}"',
        f"#define SHOWDUINO_WEBUI_ASSET_COUNT {len(rewritten)}",
        f"#define SHOWDUINO_WEBUI_RAW_BYTES {raw_total}",
        f"#define SHOWDUINO_WEBUI_EMBEDDED_BYTES {gz_total}",
        "",
        "struct ShowduinoWebAsset {",
        "  const char *path;",
        "  const char *mime;",
        "  const uint8_t *data;",
        "  size_t length;",
        "  size_t rawLength;",
        "  bool gzip;",
        "};",
        "",
    ]

    for i, (path, mime, raw, payload, use_gz) in enumerate(rewritten):
        lines.extend(c_array(f"kWebAsset_{i}", payload))

    lines.append("static const ShowduinoWebAsset kShowduinoWebAssets[] = {")
    for i, (path, mime, raw, payload, use_gz) in enumerate(rewritten):
        gz_lit = "true" if use_gz else "false"
        lines.append(
            f'  {{ "{path}", "{mime}", kWebAsset_{i}, {len(payload)}, {len(raw)}, {gz_lit} }},'
        )
    lines += [
        "};",
        "",
        "#endif",
        "",
    ]

    os.makedirs(os.path.dirname(OUT_H), exist_ok=True)
    with open(OUT_H, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lines))

    report = {
        "buildHash": short,
        "buildUtc": stamp,
        "source": "web/showduino-studio",
        "output": os.path.relpath(OUT_H, ROOT).replace("\\", "/"),
        "assetCount": len(rewritten),
        "rawBytes": raw_total,
        "embeddedBytes": gz_total,
        "bundledModules": module_order,
        "assets": [
            {
                "path": path,
                "mime": mime,
                "raw": len(raw),
                "embedded": len(payload),
                "gzip": use_gz,
            }
            for path, mime, raw, payload, use_gz in rewritten
        ],
    }
    with open(OUT_JSON, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(report, fh, indent=2)
        fh.write("\n")

    print(f"Generated {OUT_H}")
    print(f"Build {short}  assets={len(rewritten)}  raw={raw_total}  embedded={gz_total}")
    print(f"Bundled {len(module_order)} modules into /js/app.js")
    for path, _mime, raw, payload, use_gz in rewritten:
        flag = "gzip" if use_gz else "raw"
        print(f"  {path}  {len(raw)} -> {len(payload)} ({flag})")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"embed-webui failed: {exc}", file=sys.stderr)
        raise
