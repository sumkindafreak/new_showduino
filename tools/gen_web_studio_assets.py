#!/usr/bin/env python3
import os

WEB_ROOT = os.path.join(os.path.dirname(__file__), "..", "web", "showduino-studio")
OUT = os.path.join(
    os.path.dirname(__file__),
    "..",
    "firmware",
    "director-esp32-8048s050",
    "ShowduinoDirector8048S050",
    "src",
    "WebStudioAssets.h",
)

MIME = {
    ".html": "text/html",
    ".css": "text/css",
    ".js": "application/javascript",
    ".json": "application/json",
    ".svg": "image/svg+xml",
    ".md": "text/plain",
}


def main():
    assets = []
    for root, _dirs, files in os.walk(WEB_ROOT):
        for fn in sorted(files):
            ext = os.path.splitext(fn)[1].lower()
            if ext not in MIME:
                continue
            full = os.path.join(root, fn)
            rel = os.path.relpath(full, WEB_ROOT).replace("\\", "/")
            web_path = "/" + rel
            with open(full, "rb") as f:
                data = f.read()
            assets.append((web_path, MIME[ext], data))

    lines = [
        "#ifndef SHOWDUINO_WEB_STUDIO_ASSETS_H",
        "#define SHOWDUINO_WEB_STUDIO_ASSETS_H",
        "",
        "#include <Arduino.h>",
        "#include <SD.h>",
        '#include "StorageConfig.h"',
        '#include "FileUtil.h"',
        "",
        "struct WebStudioAsset {",
        "  const char *mime;",
        "  const char *data;",
        "  size_t length;",
        "};",
        "",
    ]

    var_names = []
    for i, (path, mime, data) in enumerate(assets):
        vname = f"kAsset_{i}"
        var_names.append((path, mime, vname, len(data)))
        lines.append(f"static const char {vname}[] PROGMEM = {{")
        chunk = []
        for b in data:
            chunk.append(str(b))
            if len(chunk) >= 16:
                lines.append("  " + ", ".join(chunk) + ",")
                chunk = []
        if chunk:
            lines.append("  " + ", ".join(chunk) + ",")
        lines.append("};")
        lines.append("")

    lines += [
        "struct WebStudioAssetEntry {",
        "  const char *path;",
        "  const char *mime;",
        "  const char *data;",
        "  size_t length;",
        "};",
        "",
        "static const WebStudioAssetEntry kEmbeddedAssets[] = {",
    ]
    for path, mime, vname, length in var_names:
        lines.append(f'  {{ "{path}", "{mime}", {vname}, {length} }},')
    lines += [
        "};",
        "",
        "static const size_t kEmbeddedAssetCount = sizeof(kEmbeddedAssets) / sizeof(kEmbeddedAssets[0]);",
        "",
        "inline WebStudioAsset getEmbeddedAsset(const char *path) {",
        "  WebStudioAsset out = { nullptr, nullptr, 0 };",
        "  if (!path) return out;",
        "  String p = String(path);",
        '  if (p.length() == 0) p = "/index.html";',
        "  for (size_t i = 0; i < kEmbeddedAssetCount; i++) {",
        "    if (p.equals(kEmbeddedAssets[i].path)) {",
        "      out.mime = kEmbeddedAssets[i].mime;",
        "      out.data = kEmbeddedAssets[i].data;",
        "      out.length = kEmbeddedAssets[i].length;",
        "      return out;",
        "    }",
        "  }",
        "  return out;",
        "}",
        "",
        "inline bool ensureWwwOnSd() {",
        "  if (!SD.exists(PATH_WEBUI_WWW)) {",
        "    if (!ShowduinoFileUtil::ensureDir(PATH_WEBUI_WWW)) return false;",
        "  }",
        "  for (size_t i = 0; i < kEmbeddedAssetCount; i++) {",
        "    const WebStudioAssetEntry &e = kEmbeddedAssets[i];",
        "    String sdPath = String(PATH_WEBUI_WWW) + String(e.path);",
        "    if (SD.exists(sdPath.c_str())) continue;",
        "    if (!ShowduinoFileUtil::ensureParentDirs(sdPath.c_str())) continue;",
        "    File f = SD.open(sdPath.c_str(), FILE_WRITE);",
        "    if (!f) continue;",
        "    for (size_t j = 0; j < e.length; j++) {",
        "      f.write(pgm_read_byte(e.data + j));",
        "    }",
        "    f.close();",
        "  }",
        "  return true;",
        "}",
        "",
        "#endif",
        "",
    ]

    out_path = os.path.normpath(OUT)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))

    print(f"Generated {out_path} with {len(assets)} assets")
    for path, _mime, _vname, length in var_names:
        print(f"  {path} ({length} bytes)")


if __name__ == "__main__":
    main()
