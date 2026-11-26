param([string]$Workspace = "")
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
if (!$Workspace) { $Workspace = Join-Path $root "_build" }
$template = Join-Path $Workspace "union-plugin-template"
$templateCommit = "42a8a0cb61a5390696cdbcfe1a918c9e1b25b6f8"
$build = Join-Path $Workspace "compiled"
$release = Join-Path $root "release"
New-Item -ItemType Directory -Force -Path $Workspace, $release | Out-Null
function Check($msg) { if ($LASTEXITCODE -ne 0) { throw "$msg (code $LASTEXITCODE)" } }
if (!(Test-Path (Join-Path $template ".git"))) {
  git clone --no-checkout --depth 1 https://github.com/Patrix9999/union-plugin-template.git $template
  Check "Failed to fetch Union template"
}
git -C $template cat-file -e "$templateCommit^{commit}" 2>$null
if ($LASTEXITCODE -ne 0) {
  git -C $template fetch --depth 1 origin $templateCommit
  Check "Failed to fetch pinned Union template"
}
git -C $template checkout --detach --force $templateCommit
Check "Failed to select pinned Union template"
git -C $template submodule update --init --recursive
Check "Failed to fetch Union dependencies"
$sources = @("Plugin.ipp", "inventory_layout.hpp", "inventory_history.hpp", "inventory_tracking.hpp", "inventory_hotbar.hpp", "inventory_trade.hpp", "inventory_trade_rules.hpp", "mouse_win32.hpp", "inventory_cursor.rc", "inventory_cursor.cur")
foreach ($source in $sources) {
  Copy-Item (Join-Path $root "src/$source") (Join-Path $template "src/$source") -Force
}
$textureDir = Join-Path $template "vdf/_Work/Data/Textures/_Compiled"
New-Item -ItemType Directory -Force -Path $textureDir | Out-Null
foreach ($texture in @("G2NRI_PANEL-C.TEX", "G2NRI_SOLID-C.TEX", "G2NRI_CURSOR-C.TEX", "G2NRI_LOCKED-C.TEX", "G2NRI_UNREAD-C.TEX", "G2NRI_EQUIPPED-C.TEX")) {
  Copy-Item (Join-Path $root "textures/$texture") $textureDir -Force
}
$cmake = Join-Path $template "CMakeLists.txt"
$body = [IO.File]::ReadAllText($cmake)
$body = $body.Replace("project(UnionPlugin", "project(G2NRinventory")
$body = $body.Replace('VERSION ${PROJECT_VERSION}', 'VERSION 0.1.11.0')
if ($body -notmatch "project\(G2NRinventory") { throw "Upstream CMake file changed" }
$body += "`r`ntarget_sources(G2NRinventory PRIVATE src/inventory_cursor.rc)`r`n"
$body += 'target_compile_options(G2NRinventory PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")' + "`r`n"
[IO.File]::WriteAllText($cmake, $body)
cmake -S $template -B $build -G "Visual Studio 17 2022" -A Win32 -DGOTHIC_API_G2A=ON
Check "CMake configure failed"
cmake --build $build --config Release --parallel 2
Check "C++ or VDF build failed"
$vdf = Join-Path $build "vdf/G2NRinventory.vdf"
if (!(Test-Path $vdf)) { throw "VDF not found at $vdf" }
Copy-Item $vdf (Join-Path $release "G2NRinventory.vdf") -Force
Write-Host "Build succeeded, VDF in release/"
