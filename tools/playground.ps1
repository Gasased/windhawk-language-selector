# Playground: drives every popup state in the VM and records it.
#   .\tools\playground.ps1                                 # full matrix -> shots\pg\<run>\
#   .\tools\playground.ps1 -Layouts horizontal -Modes only -Scenarios hold
#   .\tools\playground.ps1 -Langs 5 -Dpi 150
# Scenarios: single (one Alt+Shift tap), repeat (4 taps, 0.45 s apart), rapid (6 taps, 0.15 s apart),
# hold (Alt+Shift held 2 s), althold (Alt held, Shift tapped 3x), spoiled (Alt+Shift+Left: no switch),
# winspace (Win held, Space x2).
# Per case: frame burst (shots\pg\<run>\<case>\NN.png), a contact sheet (<case>.png, frames
# cropped around the caret, newest last) and report.txt: panels found on every frame (diff vs. a
# baseline frame), their rectangles, the mod's own log lines, and whether Windows' switcher window
# was on screen. Keys go in as raw scancodes, so the guest sees real hardware input; "hold" resends
# the make code like keyboard typematic (500 ms delay, 30/s).
param(
    [string[]]$Layouts = @('vertical', 'horizontal'),
    [string[]]$Modes = @('all', 'only'),           # all = list of languages, only = only the new one
    [string[]]$Scenarios = @('single', 'repeat', 'rapid', 'hold', 'althold', 'spoiled', 'winspace'),
    [ValidateSet(2, 3, 5)][int]$Langs = 3,
    [ValidateSet(100, 125, 150, 175)][int]$Dpi = 100,
    [hashtable]$Settings = @{},                    # extra mod settings, e.g. @{animation='none'}
    [string]$Run = (Get-Date -Format 'MMdd-HHmmss'),
    [switch]$NoDeploy,
    [switch]$NoCaret                               # focus the empty desktop instead: no text cursor anywhere
)
$ErrorActionPreference = 'Stop'
$proj = Split-Path $PSScriptRoot
$vm = "$PSScriptRoot\vm.ps1"
$vb = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
$out = "$proj\shots\pg\$Run"
New-Item -ItemType Directory -Force $out | Out-Null
$cli = '& "C:\Program Files\Windhawk\windhawk-cli.exe"'
$mod = 'local@mac-style-language-popup'

Add-Type -ReferencedAssemblies System.Drawing, System.Drawing.Primitives, System.Drawing.Common, System.Collections, System.Runtime.InteropServices, System.Runtime, System.Private.Windows.GdiPlus, System.Private.Windows.Core -TypeDefinition @'
using System; using System.Collections.Generic; using System.Drawing; using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class Pg {
    static int[] Pixels(Bitmap b) {
        var d = b.LockBits(new Rectangle(0, 0, b.Width, b.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        var px = new int[b.Width * b.Height]; Marshal.Copy(d.Scan0, px, 0, px.Length); b.UnlockBits(d); return px;
    }
    // Rectangles of regions that differ from the baseline (4x4 cells, 8-connected), ignoring
    // anything smaller than minSide (caret blink) and the taskbar.
    public static List<Rectangle> Diff(string basePath, string framePath, int minSide, int ignoreBelowY) {
        using (var a = new Bitmap(basePath)) using (var f = new Bitmap(framePath)) {
            int w = a.Width, h = a.Height, C = 4, gw = w / C, gh = h / C;
            var pa = Pixels(a); var pf = Pixels(f);
            var cell = new bool[gw, gh];
            for (int y = 0; y < Math.Min(h, ignoreBelowY); y++) for (int x = 0; x < w; x++) {
                int i = y * w + x, p = pa[i], q = pf[i];
                int d = Math.Abs(((p >> 16) & 255) - ((q >> 16) & 255)) + Math.Abs(((p >> 8) & 255) - ((q >> 8) & 255)) + Math.Abs((p & 255) - (q & 255));
                if (d > 24) cell[Math.Min(x / C, gw - 1), Math.Min(y / C, gh - 1)] = true;
            }
            var seen = new bool[gw, gh]; var res = new List<Rectangle>(); var st = new Stack<Point>();
            for (int y = 0; y < gh; y++) for (int x = 0; x < gw; x++) {
                if (!cell[x, y] || seen[x, y]) continue;
                int x0 = x, x1 = x, y0 = y, y1 = y; st.Push(new Point(x, y)); seen[x, y] = true;
                while (st.Count > 0) {
                    var p = st.Pop();
                    x0 = Math.Min(x0, p.X); x1 = Math.Max(x1, p.X); y0 = Math.Min(y0, p.Y); y1 = Math.Max(y1, p.Y);
                    for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
                        int nx = p.X + dx, ny = p.Y + dy;
                        if (nx < 0 || ny < 0 || nx >= gw || ny >= gh || seen[nx, ny] || !cell[nx, ny]) continue;
                        seen[nx, ny] = true; st.Push(new Point(nx, ny));
                    }
                }
                var r = new Rectangle(x0 * C, y0 * C, (x1 - x0 + 1) * C, (y1 - y0 + 1) * C);
                if (r.Width >= minSide && r.Height >= minSide) res.Add(r);
            }
            return res;
        }
    }
    // Grid of frames cropped to `crop`, labelled with their index and time.
    public static void Sheet(string[] frames, string[] labels, Rectangle crop, int cols, string outPath) {
        int rows = (frames.Length + cols - 1) / cols, lh = 18;
        using (var sheet = new Bitmap(cols * crop.Width, rows * (crop.Height + lh)))
        using (var g = Graphics.FromImage(sheet))
        using (var font = new Font("Segoe UI", 10)) {
            g.Clear(Color.Black);
            for (int i = 0; i < frames.Length; i++) using (var f = new Bitmap(frames[i])) {
                int cx = (i % cols) * crop.Width, cy = (i / cols) * (crop.Height + lh);
                g.DrawString(labels[i], font, Brushes.Yellow, cx + 4, cy + 1);
                g.DrawImage(f, new Rectangle(cx, cy + lh, crop.Width, crop.Height), crop, GraphicsUnit.Pixel);
                g.DrawRectangle(Pens.DimGray, cx, cy + lh, crop.Width - 1, crop.Height - 1);
            }
            sheet.Save(outPath, ImageFormat.Png);
        }
    }
}
'@

function Guest([string]$code, [int]$t = 120) { & $vm run $code -Timeout $t }
# shared clock for frames and keys: ms since $script:t0 (Stopwatch ticks)
$script:t0 = 0; $script:keyLog = @()
function Ms { [int](([Diagnostics.Stopwatch]::GetTimestamp() - $script:t0) * 1000 / [Diagnostics.Stopwatch]::Frequency) }
function Scan([string]$codes) {
    & $vb controlvm win11-langbar keyboardputscancode $codes.Split(' ')
    if ($script:t0) { $script:keyLog += '{0,6} ms  sent {1}' -f (Ms), $codes }   # logged once the key is in
}

# --- one-time guest state for this run: languages, DPI, ENG active, Notepad with a caret
$langTags = @{ 2 = 'en-US', 'uk'; 3 = 'en-US', 'uk', 'ru'; 5 = 'en-US', 'uk', 'ru', 'de-DE', 'fr-FR' }[$Langs]
Guest @"
`$want = '$($langTags -join ',')'
if (((Get-WinUserLanguageList).LanguageTag -join ',') -ne `$want) {
    `$l = New-WinUserLanguageList '$($langTags[0])'
    foreach (`$t in '$($langTags[1..9] -join "','")'.Split(',')) { `$l.Add(`$t.Trim("'")) }
    `$l | % { if (`$_.LanguageTag -eq 'uk') { `$_.InputMethodTips.Clear(); `$_.InputMethodTips.Add('0422:00020422') } }
    Set-WinUserLanguageList `$l -Force
}
# live DPI change of the primary monitor (SPI_SETLOGICALDPIOVERRIDE takes a step index relative to the recommended 100%)
Add-Type -Name S -Namespace P -MemberDefinition '[DllImport("user32.dll")] public static extern bool SystemParametersInfo(uint a, uint b, IntPtr c, uint d);'
# caret blink off: a blinking caret next to the panel would show up as a change in every frame diff
Add-Type -Name C -Namespace P -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetCaretBlinkTime(uint ms);'
[P.C]::SetCaretBlinkTime([uint32]::MaxValue) | Out-Null
[P.S]::SystemParametersInfo(0x9F, [uint32]([int]($(@{100 = 0; 125 = 1; 150 = 2; 175 = 3 }[$Dpi]))), [IntPtr]::Zero, 1) | Out-Null
# mod log listener (deploy starts it too; after a VM reboot only this does)
if (-not (Get-CimInstance Win32_Process -Filter "name='powershell.exe'" | ? CommandLine -match 'vm-dbgview')) {
    Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File \\VBoxSvr\project\tools\vm-dbgview.ps1'
}
'langs: ' + ((Get-WinUserLanguageList).LanguageTag -join ',')
"@ | Out-Host

if (-not $NoDeploy) { $d = & $vm deploy | Out-String; $d | Out-Host; if ($d -match '(?m)^error:') { throw "deploy failed (see above)" } }

function Prepare-Notepad {
    # fresh Notepad at a fixed spot, caret after "Hello world" on line 2, ENG active
    Guest @'
Get-Process notepad, msedge, windhawk-ui -ErrorAction SilentlyContinue | Stop-Process -Force
(New-Object -ComObject Shell.Application).MinimizeAll()
Start-Sleep -Milliseconds 800
Start-Process notepad
$i = 0; while ($i++ -lt 40 -and -not ($p = @(Get-Process notepad -ErrorAction SilentlyContinue | ? MainWindowHandle -ne 0)[0])) { Start-Sleep -Milliseconds 250 }
Add-Type -Name W -Namespace U -MemberDefinition '[DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h,int x,int y,int w,int ht,bool r); [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y); [DllImport("user32.dll")] public static extern void mouse_event(int f, int x, int y, int d, int e); [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l); [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow(); [DllImport("user32.dll")] public static extern IntPtr LoadKeyboardLayout(string id, uint f);'
[U.W]::MoveWindow($p.MainWindowHandle, 160, 90, 1100, 600, $true) | Out-Null
Start-Sleep -Milliseconds 600
[U.W]::SetCursorPos(700, 400) | Out-Null; [U.W]::mouse_event(2,0,0,0,0); [U.W]::mouse_event(4,0,0,0,0)
Start-Sleep -Milliseconds 300
[U.W]::SetCursorPos(1500, 820) | Out-Null   # pointer out of the way
# WM_INPUTLANGCHANGEREQUEST -> English
[U.W]::PostMessage([U.W]::GetForegroundWindow(), 0x50, [IntPtr]::Zero, [U.W]::LoadKeyboardLayout('00000409', 1)) | Out-Null
'notepad ' + $p.Id
'@ | Out-Host
    Start-Sleep -Milliseconds 500
    Scan '1d 1e 9e 9d'   # Ctrl+A: replace whatever Notepad restored
    & $vb controlvm win11-langbar keyboardputstring "The quick brown fox`nHello world" | Out-Null
    if ($NoCaret) {
        Guest @'
(New-Object -ComObject Shell.Application).MinimizeAll(); Start-Sleep -Milliseconds 700
Add-Type -Name W -Namespace U -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y); [DllImport("user32.dll")] public static extern void mouse_event(int f, int x, int y, int d, int e);'
[U.W]::SetCursorPos(900, 480) | Out-Null; [U.W]::mouse_event(2,0,0,0,0); [U.W]::mouse_event(4,0,0,0,0)   # click empty desktop, pointer stays there
'@ | Out-Null
    }
    Start-Sleep -Milliseconds 1500   # let any popup from setup fade out
}

$keys = @{
    single   = { Scan '38'; Scan '2a'; Start-Sleep -Milliseconds 60; Scan 'aa'; Scan 'b8' }
    # 4 separate Alt+Shift taps, ~0.45 s apart (each while the previous panel is still up)
    repeat   = { 1..4 | % { Scan '38'; Scan '2a'; Start-Sleep -Milliseconds 60; Scan 'aa'; Scan 'b8'; Start-Sleep -Milliseconds 380 } }
    # 6 taps as fast as a person can (~0.15 s apart)
    rapid    = { 1..6 | % { Scan '38'; Scan '2a'; Scan 'aa'; Scan 'b8'; Start-Sleep -Milliseconds 70 } }
    # Alt held, Shift held 2 s with typematic repeats, then both released
    hold     = { Scan '38'; Scan '2a'; Start-Sleep -Milliseconds 500; $t = [Diagnostics.Stopwatch]::StartNew(); while ($t.ElapsedMilliseconds -lt 1500) { Scan '2a'; Start-Sleep -Milliseconds 25 }; Scan 'aa'; Scan 'b8' }
    # Alt held, Shift tapped 3 times, Alt released (Windows shows its own switcher from the 2nd tap)
    althold  = { Scan '38'; 1..3 | % { Start-Sleep -Milliseconds 250; Scan '2a'; Start-Sleep -Milliseconds 80; Scan 'aa' }; Start-Sleep -Milliseconds 700; Scan 'b8' }
    # Alt+Shift held, then an arrow key: a shortcut, not a language switch (no panel should stay)
    spoiled  = { Scan '38'; Scan '2a'; Start-Sleep -Milliseconds 120; Scan 'e0 4b'; Scan 'e0 cb'; Start-Sleep -Milliseconds 400; Scan 'aa'; Scan 'b8' }
    # Win held, Space pressed twice, Win released
    winspace = { Scan 'e0 5b'; Start-Sleep -Milliseconds 150; Scan '39 b9'; Start-Sleep -Milliseconds 700; Scan '39 b9'; Start-Sleep -Milliseconds 900; Scan 'e0 db' }
}
# how long to keep shooting after the keys start
$burst = @{ single = 2.0; repeat = 3.4; rapid = 2.6; hold = 3.6; althold = 3.6; spoiled = 2.4; winspace = 3.6 }

$summary = @()
foreach ($layout in $Layouts) {
    foreach ($m in $Modes) {
        $set = @{ orientation = $layout; onlyNewLanguage = [int]($m -eq 'only') } + $Settings
        $pairs = ($set.GetEnumerator() | % { "$($_.Key)=$($_.Value)" }) -join ' '
        $r = Guest "$cli mod settings set $mod $pairs 2>&1 | Out-String"; if ($r -match '(?m)^error') { throw "settings not applied: $r" }
        foreach ($sc in $Scenarios) {
            $case = "$layout-$m-$sc-${Langs}l-$($Dpi)dpi" + $(if ($NoCaret) { "-nocaret" })
            $dir = "$out\$case"; New-Item -ItemType Directory -Force $dir | Out-Null
            Prepare-Notepad
            $logPos = (Get-Item "$proj\vm-share\debug.log").Length
            & $vb controlvm win11-langbar screenshotpng "$dir\base.png" | Out-Null
            # frames are shot on a thread while the keys go in
            $script:t0 = [Diagnostics.Stopwatch]::GetTimestamp(); $script:keyLog = @()
            $shooter = Start-ThreadJob -ArgumentList $vb, $dir, $burst[$sc], $script:t0 {
                param($vb, $dir, $secs, $t0)
                $t = [Diagnostics.Stopwatch]::StartNew(); $i = 0
                $skew = [int](([Diagnostics.Stopwatch]::GetTimestamp() - $t0) * 1000 / [Diagnostics.Stopwatch]::Frequency) - $t.ElapsedMilliseconds
                while ($t.Elapsed.TotalSeconds -lt $secs) {
                    $ms = $t.ElapsedMilliseconds + $skew   # frame name = ms on the shared clock, taken at shot start
                    & $vb controlvm win11-langbar screenshotpng ("$dir\{0:d2}_{1}ms.png" -f $i++, $ms) | Out-Null
                }
            }
            Start-Sleep -Milliseconds 150
            & $keys[$sc]
            $shooter | Wait-Job | Remove-Job
            Start-Sleep -Milliseconds 300
            # guest-side facts: active layout (Windows' switcher: see 'native panel' lines in the mod log)
            $facts = Guest @'
Add-Type -Name F -Namespace Q -MemberDefinition '[DllImport("user32.dll")] public static extern IntPtr FindWindow(string c, string n); [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r); [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow(); [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr p); [DllImport("user32.dll")] public static extern IntPtr GetKeyboardLayout(uint t); public struct RECT { public int l, t, r, b; }'
'active layout: {0:X}' -f [Q.F]::GetKeyboardLayout([Q.F]::GetWindowThreadProcessId([Q.F]::GetForegroundWindow(), [IntPtr]::Zero)).ToInt64()
'@
            $log = [IO.File]::ReadAllText("$proj\vm-share\debug.log").Substring($logPos)
            $frames = Get-ChildItem $dir -File -Filter *.png | ? Name -ne 'base.png' | Sort-Object Name
            $script:t0 = 0
            $rep = @("case $case", "settings: $pairs", $facts.Trim(), '', 'keys (ms on the same clock as frame names; 2a/aa = Shift down/up, 38/b8 = Alt, e0 5b/e0 db = Win, 39 b9 = Space, e0 4b e0 cb = Left):') + $script:keyLog + @('', 'frame  panels (x,y wxh) - diff vs base.png')
            $maxPanels = 0; $boxes = @()
            foreach ($f in $frames) {
                try { $rs = [Pg]::Diff("$dir\base.png", $f.FullName, 40, 850) }
                catch { $rep += "{0,-14} unreadable frame (screenshot cut short), skipped" -f $f.BaseName; Remove-Item $f.FullName; continue }
                $maxPanels = [Math]::Max($maxPanels, $rs.Count)
                $boxes += $rs
                $rep += "{0,-14} {1}  {2}" -f $f.BaseName, $rs.Count, (($rs | % { "$($_.X),$($_.Y) $($_.Width)x$($_.Height)" }) -join ' | ')
            }
            $rep += '', 'mod log (guest clock, ms precision; the panel appears at a PlacePanel line and is fully opaque ~140 ms later):', ($log -split "`r?`n" | ? { $_ } | % { ($_ -replace '^(\S+) \d+ \[WH\] \[[^\]]+\] \[\d+:(\w+)\]: ', '$1 [$2] ') })
            $rep | Set-Content "$dir\report.txt" -Encoding UTF8
            # contact sheet: crop to everything that changed (plus margin), else the caret area
            $crop = if ($boxes) {
                $x0 = ($boxes | % X | Measure -Min).Minimum; $y0 = ($boxes | % Y | Measure -Min).Minimum
                $x1 = ($boxes | % { $_.Right } | Measure -Max).Maximum; $y1 = ($boxes | % { $_.Bottom } | Measure -Max).Maximum
                [Drawing.Rectangle]::FromLTRB([Math]::Max(0, $x0 - 30), [Math]::Max(0, $y0 - 30), [Math]::Min(1600, $x1 + 30), [Math]::Min(900, $y1 + 30))
            } else { [Drawing.Rectangle]::new(160, 90, 700, 400) }
            $pick = $frames | ? { Test-Path $_.FullName } | select -First 24
            [Pg]::Sheet(@($pick.FullName), @($pick | % { $_.BaseName }), $crop, [Math]::Max(2, [Math]::Min(6, [int](1500 / $crop.Width))), "$out\$case.png")
            $summary += "{0,-40} max panels on one frame: {1}   sheet: {2}" -f $case, $maxPanels, "$case.png"
        }
    }
}
$summary | Set-Content "$out\summary.txt" -Encoding UTF8
$summary
"-> $out"
