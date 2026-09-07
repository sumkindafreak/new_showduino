$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$proto = Join-Path $here "..\..\protocol"
$src = Join-Path $here "test_e131_parser.cpp"
$out = Join-Path $here "e131_tests.exe"

Write-Host "Compiling E1.31 host parser tests..."
& g++ -std=c++17 -Wall -Wextra "-I$proto" -o $out $src
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Running..."
& $out
exit $LASTEXITCODE
