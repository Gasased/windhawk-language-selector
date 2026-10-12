# Screenshots of display modes, run one playground job after another (never in parallel: one VM).
#   .\tools\modes.ps1                -> shots\modes-light.png, shots\modes-dark.png (+ no-text-cursor cases)
#   .\tools\modes.ps1 -Set parts     -> shots\parts.png: parts on/off and custom spacing
param([ValidateSet('modes', 'parts')][string]$Set = 'modes', [switch]$NoDeploy)
$ErrorActionPreference = 'Stop'
$pg = "$PSScriptRoot\playground.ps1"
$deploy = -not $NoDeploy
$prefix = @{ modes = '^(m|nc)-'; parts = '^p-' }[$Set]
# reruns write into the same case folders: start clean so old frames don't mix in
Get-ChildItem "$PSScriptRoot\..\shots\pg" -Directory | ? Name -match $prefix | Remove-Item -Recurse -Force
function Pg([hashtable]$a) {
    if (-not $script:deploy) { $a.NoDeploy = $true }
    $script:deploy = $false
    & $pg @a | Select-Object -Last 4 | Out-Host
}
$codes = @{ showLanguageName = 0; showLayoutName = 0 }   # code only = compact cells
$full = @{ showLanguageName = 1; showLayoutName = 1 }
$defaults = @{ theme = 'system'; noCursorClassic = 1; noCursorMouse = 0; showAbbreviation = 1; showLanguageName = 1; showLayoutName = 1
    panelPadding = 4; itemPaddingX = 16; itemPaddingY = 8; columnGap = 12; itemSpacing = 2 }
$c = 'single-3l-100dpi'

if ($Set -eq 'modes') {
    foreach ($t in 'light', 'dark') {
        Pg @{ Layouts = 'vertical', 'horizontal'; Modes = 'all'; Scenarios = 'single'; Settings = $full + @{ theme = $t }; Run = "m-$t-full" }
        Pg @{ Layouts = 'vertical', 'horizontal'; Modes = 'all'; Scenarios = 'single', 'repeat'; Settings = $codes + @{ theme = $t }; Run = "m-$t-codes" }
        Pg @{ Layouts = 'vertical'; Modes = 'only'; Scenarios = 'single'; Settings = $full + @{ theme = $t }; Run = "m-$t-mac" }
        Pg @{ Layouts = 'vertical'; Modes = 'only'; Scenarios = 'single', 'repeat'; Settings = $codes + @{ theme = $t }; Run = "m-$t-maccodes" }
    }
    foreach ($nc in @{ n = 'classic'; c = 1; m = 0 }, @{ n = 'mouse'; c = 1; m = 1 }, @{ n = 'none'; c = 0; m = 0 }) {
        Pg @{ Layouts = 'vertical'; Modes = 'all'; Scenarios = 'single', 'winspace'; NoCaret = $true; Run = "nc-$($nc.n)"
            Settings = $full + @{ theme = 'light'; noCursorClassic = $nc.c; noCursorMouse = $nc.m } }
    }
}
else {
    # each case: label -> settings on top of the defaults (horizontal row unless it says vertical)
    $parts = [ordered]@{
        'All parts (default)'           = @{}
        'Code + name, no layout'        = @{ showLayoutName = 0 }
        'Name only'                     = @{ showAbbreviation = 0; showLayoutName = 0 }
        'Code + layout'                 = @{ showLanguageName = 0 }
        'Code only (compact)'           = $codes
        'Compact, tight spacing'        = $codes + @{ itemPaddingX = 8; itemPaddingY = 4; itemSpacing = 0; panelPadding = 2 }
        'Roomy: padding 24/14, gap 6'   = @{ itemPaddingX = 24; itemPaddingY = 14; itemSpacing = 6; panelPadding = 8; columnGap = 16 }
        'Vertical, roomy'               = @{ itemPaddingX = 24; itemPaddingY = 14; itemSpacing = 6; panelPadding = 8; columnGap = 16; vertical = 1 }
    }
    $i = 0; $cases = [ordered]@{}
    foreach ($label in $parts.Keys) {
        $s = $defaults.Clone(); $s.theme = 'dark'
        $v = $parts[$label]
        foreach ($k in $v.Keys) { if ($k -ne 'vertical') { $s[$k] = $v[$k] } }
        $layout = if ($v.vertical) { 'vertical' } else { 'horizontal' }
        $run = 'p-{0:d2}' -f $i++
        Pg @{ Layouts = $layout; Modes = 'all'; Scenarios = 'single'; Settings = $s; Run = $run }
        $cases[$label] = "shots\pg\$run\$layout-all-$c"
    }
}
# back to defaults
Pg @{ Layouts = 'vertical'; Modes = 'all'; Scenarios = 'single'; Settings = $defaults; Run = "$(if ($Set -eq 'parts') { 'p' } else { 'm' })-reset" }

if ($Set -eq 'modes') {
    foreach ($t in 'light', 'dark') {
        & "$PSScriptRoot\gallery.ps1" -Out "shots\modes-$t.png" -Cases ([ordered]@{
                'Vertical'                = "shots\pg\m-$t-full\vertical-all-$c"
                'Horizontal'              = "shots\pg\m-$t-full\horizontal-all-$c"
                'Compact vertical'        = "shots\pg\m-$t-codes\vertical-all-$c"
                'Compact horizontal'      = "shots\pg\m-$t-codes\horizontal-all-$c"
                'Mac (only new language)' = "shots\pg\m-$t-mac\vertical-only-$c"
                'Mac compact'             = "shots\pg\m-$t-maccodes\vertical-only-$c"
            })
    }
}
else { & "$PSScriptRoot\gallery.ps1" -Out 'shots\parts.png' -Cases $cases }
