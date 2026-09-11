#!/usr/bin/env python3
"""Build the Communications-S3 flash-resident web assets.

Two browser surfaces are embedded:

* `/` — the existing Showduino commissioning/runtime WebUI from this repository.
* `/studio/` — a snapshot of the canonical website Studio V4 from
  `sumkindafreak/showduino.com`.

The authoring Studio is fetched only while regenerating the firmware asset
bundle. The flashed S3 does not require internet access. The exact source commit
is recorded in the generated header and last-build report.
"""

from __future__ import annotations

import gzip
import hashlib
import json
import os
import posixpath
import re
import sys
import urllib.request
from datetime import datetime, timezone
from html.parser import HTMLParser
from urllib.parse import urlparse

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

OVERLAY_DIR = os.path.join(ROOT, "web", "studio-v4-overlay")
AUTHORING_REPO = "sumkindafreak/showduino.com"
AUTHORING_REF = os.environ.get("SHOWDUINO_AUTHORING_REF", "main")
AUTHORING_COMMIT_OVERRIDE = os.environ.get("SHOWDUINO_AUTHORING_COMMIT", "").strip()
AUTHORING_DYNAMIC_FILES = ("css/studio-mobile-v4.css",)

MIME = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".json": "application/json",
    ".svg": "image/svg+xml",
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".webp": "image/webp",
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
CSS_URL_RE = re.compile(r"url\(\s*['\"]?([^)'\"]+)['\"]?\s*\)", re.IGNORECASE)


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


def fetch_bytes(url: str, timeout: int = 25) -> bytes:
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": "Showduino-S3-WebUI-Builder/1.0",
            "Accept": "application/vnd.github+json, text/plain, */*",
        },
    )
    with urllib.request.urlopen(req, timeout=timeout) as response:
        return response.read()


def resolve_authoring_commit() -> str:
    if AUTHORING_COMMIT_OVERRIDE:
        return AUTHORING_COMMIT_OVERRIDE
    api = f"https://api.github.com/repos/{AUTHORING_REPO}/commits/{AUTHORING_REF}"
    data = json.loads(fetch_bytes(api).decode("utf-8"))
    sha = str(data.get("sha", "")).strip()
    if len(sha) < 12:
        raise RuntimeError("Could not resolve canonical website Studio commit")
    return sha


def authoring_raw_url(commit: str, path: str) -> str:
    return f"https://raw.githubusercontent.com/{AUTHORING_REPO}/{commit}/{path.lstrip('/')}"


def list_overlay_files() -> list[str]:
    out: list[str] = []
    if not os.path.isdir(OVERLAY_DIR):
        return out
    for dirpath, _dirnames, filenames in os.walk(OVERLAY_DIR):
        for name in filenames:
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, OVERLAY_DIR).replace("\\", "/")
            out.append(rel)
    return sorted(out)


def overlay_path(path: str) -> str:
    return os.path.join(OVERLAY_DIR, path.replace("/", os.sep))


def load_authoring_file(commit: str, path: str) -> bytes:
    local = overlay_path(path)
    if os.path.isfile(local):
        with open(local, "rb") as fh:
            return fh.read()
    return fetch_authoring_file(commit, path)


def fetch_authoring_file(commit: str, path: str) -> bytes:
    try:
        return fetch_bytes(authoring_raw_url(commit, path))
    except Exception as exc:
        raise RuntimeError(f"Could not fetch canonical Studio file {path} at {commit[:12]}: {exc}") from exc


class StudioReferenceParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.refs: list[str] = []

    def handle_starttag(self, tag: str, attrs) -> None:
        values = dict(attrs)
        if tag == "script" and values.get("src"):
            self.refs.append(values["src"])
        elif tag == "link" and values.get("href"):
            self.refs.append(values["href"])


def is_local_reference(ref: str) -> bool:
    if not ref or ref.startswith("#") or ref.startswith("//"):
        return False
    parsed = urlparse(ref)
    return not parsed.scheme and not parsed.netloc


def strip_external_authoring_dependencies(html: str) -> str:
    # The local S3 copy is intentionally offline/local-first. Supabase and Anime
    # are optional in Studio V4, so remove external CDN requests rather than
    # making the phone wait for internet that is not present on the SoftAP.
    html = re.sub(
        r"\s*<script\b[^>]*\bsrc=['\"]https?://[^'\"]+['\"][^>]*>\s*</script>",
        "",
        html,
        flags=re.IGNORECASE,
    )
    html = re.sub(
        r"\s*<link\b[^>]*\bhref=['\"]https?://[^'\"]+['\"][^>]*>",
        "",
        html,
        flags=re.IGNORECASE,
    )
    html = html.replace('href="index.html" aria-label="Showduino website"', 'href="/" aria-label="Showduino commissioning UI"')
    return html


def offline_runtime_config() -> bytes:
    return (
        "// Generated local Showduino Studio runtime configuration.\n"
        "window.SHOWDUINO_CONFIG = Object.freeze({\n"
        "  environment: 'showduino-local',\n"
        "  features: {\n"
        "    supabase: false, authentication: false, cloudSync: false,\n"
        "    firebase: false, stripe: false, subscriptions: false\n"
        "  },\n"
        "  supabase: { url: '', publishableKey: '' },\n"
        "  firebase: {}, stripe: {}, plans: {}\n"
        "});\n"
    ).encode("utf-8")


def discover_css_assets(commit: str, source_path: str, css: bytes) -> list[str]:
    text = css.decode("utf-8")
    out: list[str] = []
    base = posixpath.dirname(source_path)
    for ref in CSS_URL_RE.findall(text):
        ref = ref.strip()
        if not is_local_reference(ref) or ref.startswith("data:"):
            continue
        clean = ref.split("?", 1)[0].split("#", 1)[0]
        if not clean:
            continue
        resolved = posixpath.normpath(posixpath.join(base, clean))
        if resolved.startswith("../"):
            continue
        out.append(resolved)
    return out


def build_authoring_records() -> tuple[list[tuple[str, str, bytes]], str, list[str]]:
    commit = resolve_authoring_commit()
    raw_html = load_authoring_file(commit, "studio.html").decode("utf-8")
    parser = StudioReferenceParser()
    parser.feed(raw_html)

    source_paths: list[str] = []
    for ref in parser.refs:
        if not is_local_reference(ref):
            continue
        clean = ref.split("?", 1)[0].split("#", 1)[0].lstrip("/")
        if clean and clean not in source_paths:
            source_paths.append(clean)
    for path in AUTHORING_DYNAMIC_FILES:
        if path not in source_paths:
            source_paths.append(path)
    for path in list_overlay_files():
        if path == "studio.html":
            continue
        if path not in source_paths:
            source_paths.append(path)

    records: list[tuple[str, str, bytes]] = []
    transformed_html = strip_external_authoring_dependencies(raw_html).encode("utf-8")
    records.append(("/studio/index.html", MIME[".html"], transformed_html))

    fetched: dict[str, bytes] = {}
    pending = list(source_paths)
    while pending:
        path = pending.pop(0)
        if path in fetched:
            continue
        if path == "config/runtime-config.js":
            data = offline_runtime_config()
        else:
            data = load_authoring_file(commit, path)
        fetched[path] = data

        ext = os.path.splitext(path)[1].lower()
        if ext == ".css":
            for extra in discover_css_assets(commit, path, data):
                if extra not in fetched and extra not in pending:
                    pending.append(extra)

    for path, data in fetched.items():
        ext = os.path.splitext(path)[1].lower()
        mime = MIME.get(ext, "application/octet-stream")
        records.append((f"/studio/{path}", mime, data))

    return records, commit, list(fetched.keys())


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

    authoring_records, authoring_commit, authoring_files = build_authoring_records()
    records.extend(authoring_records)

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

    authoring_asset_count = sum(1 for path, *_rest in rewritten if path.startswith("/studio/"))
    authoring_raw_bytes = sum(len(raw) for path, _mime, raw, _payload, _use_gz in rewritten if path.startswith("/studio/"))
    authoring_embedded_bytes = sum(len(payload) for path, _mime, _raw, payload, _use_gz in rewritten if path.startswith("/studio/"))

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
        f'#define SHOWDUINO_AUTHORING_SOURCE_REPO "{AUTHORING_REPO}"',
        f'#define SHOWDUINO_AUTHORING_SOURCE_COMMIT "{authoring_commit}"',
        f"#define SHOWDUINO_AUTHORING_ASSET_COUNT {authoring_asset_count}",
        f"#define SHOWDUINO_AUTHORING_RAW_BYTES {authoring_raw_bytes}",
        f"#define SHOWDUINO_AUTHORING_EMBEDDED_BYTES {authoring_embedded_bytes}",
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

    for i, (_path, _mime, _raw, payload, _use_gz) in enumerate(rewritten):
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
        "source": "web/showduino-studio + canonical website Studio V4",
        "output": os.path.relpath(OUT_H, ROOT).replace("\\", "/"),
        "assetCount": len(rewritten),
        "rawBytes": raw_total,
        "embeddedBytes": gz_total,
        "bundledModules": module_order,
        "authoringStudio": {
            "repo": AUTHORING_REPO,
            "ref": AUTHORING_REF,
            "commit": authoring_commit,
            "route": "/studio/",
            "assetCount": authoring_asset_count,
            "rawBytes": authoring_raw_bytes,
            "embeddedBytes": authoring_embedded_bytes,
            "sourceFiles": authoring_files,
            "overlayDir": os.path.relpath(OVERLAY_DIR, ROOT).replace("\\", "/"),
            "overlayFiles": list_overlay_files(),
            "cloudEnabled": False,
            "externalCdnDependencies": False,
        },
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
    print(f"Bundled {len(module_order)} commissioning modules into /js/app.js")
    print(
        f"Authoring Studio {authoring_commit[:12]}  assets={authoring_asset_count} "
        f"raw={authoring_raw_bytes} embedded={authoring_embedded_bytes}"
    )
    for path, _mime, raw, payload, use_gz in rewritten:
        flag = "gzip" if use_gz else "raw"
        print(f"  {path}  {len(raw)} -> {len(payload)} ({flag})")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"embed-webui failed: {exc}", file=sys.stderr)
        raise
