$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$proto = Join-Path $here "..\..\protocol"

function Run-LampTest([string]$srcName, [string]$exeName) {
  $src = Join-Path $here $srcName
  $out = Join-Path $here $exeName
  Write-Host "Compiling $srcName..."
  & g++ -std=c++17 -Wall -Wextra "-I$proto" -o $out $src
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  Write-Host "Running $exeName..."
  & $out
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Run-LampTest "test_lamp_node.cpp" "lamp_node_tests.exe"
Run-LampTest "test_carbide_lamp.cpp" "carbide_lamp_tests.exe"
Run-LampTest "test_lamp_motion.cpp" "lamp_motion_tests.exe"
Write-Host "ALL LAMP HOST TESTS PASS"
exit 0
