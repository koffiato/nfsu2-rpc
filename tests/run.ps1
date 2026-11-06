param([string] $Cxx = 'cl.exe')

$ErrorActionPreference = 'Stop'
$repoDir = Split-Path -Parent $PSScriptRoot
$projectDir = Join-Path $repoDir 'nfsu2-rpc'
$outputDir = Join-Path $projectDir 'bin\tests'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$sources = @((Join-Path $PSScriptRoot 'discord_presence_test.cpp'),
  (Join-Path $projectDir 'src\discord_presence.cpp'))
$sources += Get-ChildItem (Join-Path $projectDir 'lib\discord\*.cpp') |
  Select-Object -ExpandProperty FullName

Push-Location $outputDir
try {
  if ((Split-Path -Leaf $Cxx) -match '^cl(\.exe)?$') {
    & $Cxx /nologo /EHsc /std:c++14 /W3 /MT "/I$projectDir\lib" "/I$projectDir\src" @sources /Fe:discord_presence_test.exe
  } else {
    & $Cxx -std=c++14 -Wall -Wextra -static "-I$projectDir\lib" "-I$projectDir\src" @sources -o discord_presence_test.exe
  }
  if ($LASTEXITCODE -ne 0) { throw 'Test build failed.' }
  & .\discord_presence_test.exe
  if ($LASTEXITCODE -ne 0) { throw 'Lifecycle tests failed.' }
} finally {
  Pop-Location
}
