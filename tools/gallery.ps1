# Builds one labelled image from playground cases: per case, the panel cropped out of a frame where
# it is fully faded in (the 2nd frame that shows it).
#   .\tools\gallery.ps1 -Out shots\modes-light.png -Cases @{ 'Vertical' = 'shots\pg\m-light\vertical-all-single-3l-100dpi'; ... }
# -Cases is an ordered hashtable: label -> case folder.
param([Parameter(Mandatory)][System.Collections.Specialized.OrderedDictionary]$Cases, [Parameter(Mandatory)][string]$Out, [int]$Cols = 2)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$tiles = foreach ($label in $Cases.Keys) {
    $dir = $Cases[$label]
    $hits = Get-Content "$dir\report.txt" | ? { $_ -match '^(\d\d_\S+?)\s+1\s+(\d+),(\d+) (\d+)x(\d+)' } | % {
        [pscustomobject]@{ Frame = $Matches[1]; X = [int]$Matches[2]; Y = [int]$Matches[3]; W = [int]$Matches[4]; H = [int]$Matches[5] } }
    if (-not $hits) { Write-Warning "no panel in $dir"; continue }
    $h = @($hits)[[Math]::Min(1, @($hits).Count - 1)]
    # the diff box can include the caret line above the panel; 16 px margin keeps the shadow
    $r = [Drawing.Rectangle]::FromLTRB([Math]::Max(0, $h.X - 16), [Math]::Max(0, $h.Y - 16), [Math]::Min(1600, $h.X + $h.W + 16), [Math]::Min(900, $h.Y + $h.H + 16))
    [pscustomobject]@{ Label = $label; Img = [Drawing.Bitmap]::new("$dir\$($h.Frame).png"); Rect = $r }
}
$tiles = @($tiles)
$lh = 26; $gap = 12
$cw = ($tiles | % { $_.Rect.Width } | Measure -Max).Maximum
$rows = [Math]::Ceiling($tiles.Count / $Cols)
$rowH = @(for ($i = 0; $i -lt $rows; $i++) { ($tiles[($i * $Cols)..([Math]::Min($tiles.Count, ($i + 1) * $Cols) - 1)] | % { $_.Rect.Height } | Measure -Max).Maximum + $lh })
$sheet = [Drawing.Bitmap]::new([int]($Cols * ($cw + $gap) + $gap), [int](($rowH | Measure -Sum).Sum + ($rows + 1) * $gap))  # ints: doubles pick the (string, bool) overload
$g = [Drawing.Graphics]::FromImage($sheet)
$g.Clear([Drawing.Color]::FromArgb(40, 40, 40))
$font = [Drawing.Font]::new('Segoe UI', 11, [Drawing.FontStyle]::Bold)
$y = $gap
for ($i = 0; $i -lt $tiles.Count; $i++) {
    $t = $tiles[$i]; $c = $i % $Cols
    if ($c -eq 0 -and $i -gt 0) { $y += $rowH[[int][Math]::Floor($i / $Cols) - 1] + $gap }
    $x = $gap + $c * ($cw + $gap)
    $g.DrawString($t.Label, $font, [Drawing.Brushes]::White, $x, $y + 2)
    $g.DrawImage($t.Img, [Drawing.Rectangle]::new($x, $y + $lh, $t.Rect.Width, $t.Rect.Height), $t.Rect, [Drawing.GraphicsUnit]::Pixel)
    $t.Img.Dispose()
}
$sheet.Save((Join-Path (Get-Location) $Out), [Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $sheet.Dispose()
$Out
