$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Write-Host "Running Network gateway draft host tests..."
$py = Get-Command python -ErrorAction SilentlyContinue
if ($py) {
  & python (Join-Path $here "test_network_connect_order.py")
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
$node = Get-Command node -ErrorAction SilentlyContinue
if (-not $node) {
  $portable = Join-Path $env:TEMP "showduino-node-runtime\node.exe"
  if (Test-Path $portable) {
    $node = $portable
  }
}
if (-not $node) {
  throw "node is required for tools/webui-tests (ESM draft tests)"
}
$nodeExe = if ($node -is [string]) { $node } else { $node.Source }
& $nodeExe (Join-Path $here "test_network_gateway_draft.mjs")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Network gateway draft tests passed."
