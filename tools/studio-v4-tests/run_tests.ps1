$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Write-Host "Running Studio V4 pixel-authoring host tests..."
$node = Get-Command node -ErrorAction SilentlyContinue
if ($node) {
  & node (Join-Path $here "test_pixel_authoring.js")
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  Write-Host "Studio V4 pixel-authoring tests passed."
  exit 0
}

Write-Host "node is not on PATH; open tools/studio-v4-tests/test_pixel_authoring.html in a browser instead."
Write-Host "Layout fixture: tools/studio-v4-tests/picker-fixture.html"
exit 0
