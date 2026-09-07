$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $here "test_plugin_roles.cpp"
$roles = Join-Path $here "..\..\firmware\stage-engine-p4\ShowduinoStageEngineP4\src\plugin\PluginRoles.cpp"
$include = Join-Path $here "..\..\firmware\stage-engine-p4\ShowduinoStageEngineP4\src"
$out = Join-Path $here "plugin_role_tests.exe"

Write-Host "Compiling Plug-in Bus role host tests..."
& g++ -std=c++17 -Wall -Wextra "-I$include" -o $out $src $roles
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Running..."
& $out
exit $LASTEXITCODE
