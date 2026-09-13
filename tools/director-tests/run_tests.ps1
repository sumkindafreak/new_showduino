$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$proto = Join-Path $here "..\..\protocol"
function Run-One($name, $srcName, $exeName) {
  $src = Join-Path $here $srcName
  $out = Join-Path $here $exeName
  Write-Host "Compiling $name..."
  & g++ -std=c++17 -Wall -Wextra "-I$proto" -o $out $src
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  Write-Host "Running $name..."
  & $out
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Run-One "Director lamp sheet" "test_lamp_sheet.cpp" "lamp_sheet_tests.exe"
Run-One "Director emergency sheet" "test_emergency_sheet.cpp" "emergency_sheet_tests.exe"
exit 0
