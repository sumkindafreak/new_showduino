# Regenerates C3 WebStudioAssets.h from web/showduino-studio
$ErrorActionPreference = "Stop"
$utf8 = New-Object System.Text.UTF8Encoding $false
$root = Split-Path $PSScriptRoot -Parent
$webRoot = Join-Path $root "web\showduino-studio"
$outHeader = Join-Path $root "firmware\c3-supermini-espnow-bridge\ShowduinoC3SuperMiniBridge\src\WebStudioAssets.h"

function Get-Mime($rel) {
  switch -Regex ($rel.ToLower()) {
    '\.html$' { return 'text/html' }
    '\.css$'  { return 'text/css' }
    '\.js$'   { return 'application/javascript' }
    '\.json$' { return 'application/json' }
    '\.md$'   { return 'text/markdown' }
    default   { return 'application/octet-stream' }
  }
}

$files = Get-ChildItem -Path $webRoot -Recurse -File | Sort-Object FullName
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('#ifndef SHOWDUINO_C3_WEB_STUDIO_ASSETS_H')
[void]$sb.AppendLine('#define SHOWDUINO_C3_WEB_STUDIO_ASSETS_H')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('#include <Arduino.h>')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('struct WebStudioAsset {')
[void]$sb.AppendLine('  const char *mime;')
[void]$sb.AppendLine('  const char *data;')
[void]$sb.AppendLine('  size_t length;')
[void]$sb.AppendLine('};')
[void]$sb.AppendLine('')
$entries = @()
$idx = 0
foreach ($f in $files) {
  $rel = '/' + ($f.FullName.Substring($webRoot.Length + 1) -replace '\\','/')
  $bytes = [System.IO.File]::ReadAllBytes($f.FullName)
  [void]$sb.AppendLine("static const char kAsset_$idx[] PROGMEM = {")
  $line = '  '
  for ($i = 0; $i -lt $bytes.Length; $i++) {
    $line += ([int]$bytes[$i]).ToString() + ', '
    if ((($i + 1) % 16) -eq 0) { [void]$sb.AppendLine($line.TrimEnd()); $line = '  ' }
  }
  if ($line.Trim().Length -gt 0) { [void]$sb.AppendLine($line.TrimEnd().TrimEnd(',')) }
  [void]$sb.AppendLine('};')
  [void]$sb.AppendLine('')
  $entries += [pscustomobject]@{ Index=$idx; Path=$rel; Mime=(Get-Mime $rel); Length=$bytes.Length }
  $idx++
}
[void]$sb.AppendLine('struct WebStudioAssetEntry {')
[void]$sb.AppendLine('  const char *path;')
[void]$sb.AppendLine('  const char *mime;')
[void]$sb.AppendLine('  const char *data;')
[void]$sb.AppendLine('  size_t length;')
[void]$sb.AppendLine('};')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('static const WebStudioAssetEntry kEmbeddedAssets[] = {')
foreach ($e in $entries) {
  [void]$sb.AppendLine("  { `"$($e.Path)`", `"$($e.Mime)`", kAsset_$($e.Index), $($e.Length) },")
}
[void]$sb.AppendLine('};')
[void]$sb.AppendLine('')
[void]$sb.AppendLine("static const size_t kEmbeddedAssetCount = $($entries.Count);")
[void]$sb.AppendLine('')
[void]$sb.AppendLine('inline WebStudioAsset getEmbeddedAsset(const char *path) {')
[void]$sb.AppendLine('  WebStudioAsset out = { nullptr, nullptr, 0 };')
[void]$sb.AppendLine('  if (!path) return out;')
[void]$sb.AppendLine('  for (size_t i = 0; i < kEmbeddedAssetCount; i++) {')
[void]$sb.AppendLine('    if (strcmp(path, kEmbeddedAssets[i].path) == 0) {')
[void]$sb.AppendLine('      out.mime = kEmbeddedAssets[i].mime;')
[void]$sb.AppendLine('      out.data = kEmbeddedAssets[i].data;')
[void]$sb.AppendLine('      out.length = kEmbeddedAssets[i].length;')
[void]$sb.AppendLine('      return out;')
[void]$sb.AppendLine('    }')
[void]$sb.AppendLine('  }')
[void]$sb.AppendLine('  return out;')
[void]$sb.AppendLine('}')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('#endif /* SHOWDUINO_C3_WEB_STUDIO_ASSETS_H */')
[System.IO.File]::WriteAllText($outHeader, $sb.ToString(), $utf8)
Write-Host "Wrote $OutHeader ($($entries.Count) assets)"