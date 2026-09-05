$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $here "test_production_format.cpp"
$format = Join-Path $here "..\..\firmware\stage-engine-p4\ShowduinoStageEngineP4\src\ProductionFormat.cpp"
$include = Join-Path $here "..\..\firmware\stage-engine-p4\ShowduinoStageEngineP4\src"
$out = Join-Path $here "production_format_tests.exe"
$runtimeOut = Join-Path $here "timeline_runtime_tests.exe"
$storeOut = Join-Path $here "production_store_tests.exe"
$sketch = Join-Path $here "..\..\firmware\stage-engine-p4\ShowduinoStageEngineP4"
$stubs = Join-Path $here "stubs"

Write-Host "Compiling production format host tests..."
& g++ -std=c++17 -Wall -Wextra "-I$include" -o $out $src $format
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Running..."
& $out
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Compiling production store host tests..."
& g++ -std=c++17 -Wall -Wextra "-I$stubs" "-I$include" -o $storeOut `
  (Join-Path $here "test_production_store.cpp") `
  (Join-Path $include "ProductionFormat.cpp") `
  (Join-Path $include "ProductionStore.cpp")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Running..."
& $storeOut
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Compiling timeline/runtime host tests..."
& g++ -std=c++17 -Wall -Wextra "-I$stubs" "-I$sketch" -o $runtimeOut `
  (Join-Path $here "test_timeline_runtime.cpp")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Running..."
& $runtimeOut
exit $LASTEXITCODE
