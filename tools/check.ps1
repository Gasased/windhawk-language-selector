# Compile-only check of the mod with the host's Windhawk clang (same flags Windhawk uses).
# Usage: .\tools\check.ps1 [mods\x.wh.cpp]
param([string]$Mod = "$PSScriptRoot\..\mods\mac-style-language-popup.wh.cpp")
$c = 'C:\Program Files\Windhawk\Compiler'
$out = "$env:TEMP\whcheck.o"
# compile only: the Windhawk runtime (InternalWh_*) is only linkable inside Windhawk
$args = @('-c', '-std=c++23', '-O2', '-DUNICODE', '-D_UNICODE', '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00',
    '-DNTDDI_VERSION=0x0A000008', '-D__USE_MINGW_ANSI_STDIO=0', '-DWH_MOD', '-DWH_MOD_ID=L"check"', '-DWH_MOD_VERSION=L"0"',
    '-include', "$c\include\windhawk_api.h", '-target', 'x86_64-w64-mingw32',
    '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-missing-field-initializers',
    (Resolve-Path $Mod).Path, '-o', $out)
# Settings YAML: an unquoted value containing ": " breaks the whole block (Windhawk refuses the install)
$block = [regex]::Match((Get-Content -Raw $Mod), '(?s)// ==WindhawkModSettings==(.*?)// ==/WindhawkModSettings==').Groups[1].Value
$n = 0; foreach ($l in $block -split "`r?`n") { $n++
    if ($l -match '^\s*(?:-\s+)?[$\w]+:\s+(.*)$' -and ($v = $Matches[1]) -match ': ' -and $v -notmatch '^["'']') { "settings line ${n}: unquoted ': ' in value -> $($l.Trim())" } }
& "$c\bin\clang++.exe" @args 2>&1
"exit $LASTEXITCODE"
