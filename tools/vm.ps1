# Host-side VM control. Examples:
#   .\tools\vm.ps1 run 'Get-WinUserLanguageList'      # run PS on the VM's interactive desktop
#   .\tools\vm.ps1 run -File .\some.ps1
#   .\tools\vm.ps1 gc 'Get-Process'                     # same via guestcontrol (no agent needed, not interactive, not admin)
#   .\tools\vm.ps1 deploy                              # install mods\mac-style-language-popup.wh.cpp, logging -> vm-share\debug.log
#   .\tools\vm.ps1 shot flyout                        # -> shots\flyout.png
#   .\tools\vm.ps1 keys win+space                      # chord, released after -HoldMs
#   .\tools\vm.ps1 keys alt+shift -HoldMs 1500
#   .\tools\vm.ps1 type 'hello'
#   .\tools\vm.ps1 click 800,450                        # left click at guest pixel (same as screenshot pixel)
#   .\tools\vm.ps1 scroll 800,450,-3                    # mouse wheel 3 notches down
param(
    [Parameter(Mandatory)][ValidateSet('run', 'gc', 'shot', 'keys', 'type', 'deploy', 'click', 'scroll')][string]$Cmd,
    [string]$Arg,
    [string]$File,
    [int]$Timeout = 120,
    [int]$HoldMs = 150
)
$ErrorActionPreference = 'Stop'
$vb = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
$vm = 'win11-langbar'
$proj = Split-Path $PSScriptRoot

switch ($Cmd) {
    'run' {
        $code = if ($File) { Get-Content -Raw $File } else { $Arg }
        $id = [guid]::NewGuid().ToString('N')
        $out = "$proj\vm-share\outbox\$id.txt"
        Set-Content "$proj\vm-share\inbox\$id.tmp" $code -Encoding UTF8
        Move-Item "$proj\vm-share\inbox\$id.tmp" "$proj\vm-share\inbox\$id.ps1"
        $deadline = (Get-Date).AddSeconds($Timeout)
        while (-not (Test-Path $out)) {
            if ((Get-Date) -gt $deadline) { throw "timeout: agent did not answer in ${Timeout}s (is the VM logged in?)" }
            Start-Sleep -Milliseconds 200
        }
        Get-Content -Raw $out; Remove-Item $out
    }
    { $_ -in 'click', 'scroll' } {
        # via the agent (real desktop): click X,Y  /  scroll X,Y,NOTCHES (negative = down). Screenshot pixels = guest pixels.
        $n = $Arg -split '[, ]+' | ForEach-Object { [int]$_ }  # unquoted 800,41 arrives as "800 41"
        # mouse_event flags: 2/4 = left down/up, 0x800 = wheel (120 per notch)
        $act = if ($Cmd -eq 'click') { '[W.M]::mouse_event(2,0,0,0,0); [W.M]::mouse_event(4,0,0,0,0)' }
        else { "[W.M]::mouse_event(0x800,0,0,$($n[2] * 120),0)" }
        & $PSCommandPath run -Timeout 60 -Arg @"
Add-Type -Name M -Namespace W -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y); [DllImport("user32.dll")] public static extern void mouse_event(int f, int x, int y, int d, int e);'
[W.M]::SetCursorPos($($n[0]), $($n[1])) | Out-Null; Start-Sleep -Milliseconds 100; $act
"@
    }
    'deploy' {
        # (re)install the mod from the share with logging on; Wh_Log lines land in vm-share\debug.log
        $id = 'mac-style-language-popup'
        & $PSCommandPath run -Timeout 300 -Arg @"
`$c = 'C:\Program Files\Windhawk\windhawk-cli.exe'
& `$c mod install --file \\VBoxSvr\project\mods\$id.wh.cpp 2>&1 | % { "`$_" }
& `$c mod config set local@$id loggingEnabled true 2>&1 | % { "`$_" }
# fresh listener every deploy: an old one can be left stuck on the DBWIN buffer
Get-CimInstance Win32_Process -Filter "name='powershell.exe'" | ? CommandLine -match 'vm-dbgview' | % { Stop-Process -Id `$_.ProcessId -Force }
Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File \\VBoxSvr\project\tools\vm-dbgview.ps1'
"@
    }
    'gc' {
        # via guestcontrol: works without the agent, but non-interactive session + filtered (non-admin) token
        $code = if ($File) { Get-Content -Raw $File } else { $Arg }
        $name = "gc-$([guid]::NewGuid().ToString('N')).ps1"
        Set-Content "$proj\vm-share\$name" $code -Encoding UTF8
        & $vb guestcontrol $vm run --username claude --password claude --timeout ($Timeout * 1000) `
            --exe 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -- powershell -NoProfile -ExecutionPolicy Bypass -File "\\VBoxSvr\project\vm-share\$name" 2>&1
        Remove-Item "$proj\vm-share\$name"
    }
    'shot' {
        $name = if ($Arg) { $Arg } else { 'screen' }
        $png = "$proj\shots\$name.png"
        & $vb controlvm $vm screenshotpng $png | Out-Null
        $png
    }
    'keys' {
        # make codes; break code = make | 0x80. e0-prefixed keys keep the prefix.
        $map = @{ win = 'e0 5b'; ctrl = '1d'; alt = '38'; shift = '2a'; space = '39'; enter = '1c'; esc = '01'; tab = '0f'
            left = 'e0 4b'; right = 'e0 4d'; up = 'e0 48'; down = 'e0 50'; bksp = '0e'; del = 'e0 53'; home = 'e0 47'; end = 'e0 4f'
            pgup = 'e0 49'; pgdn = 'e0 51'; apps = 'e0 5d' }
        $i = 0x10; foreach ($c in 'qwertyuiop'.ToCharArray()) { $map["$c"] = '{0:x2}' -f $i++ }
        $i = 0x1e; foreach ($c in 'asdfghjkl'.ToCharArray()) { $map["$c"] = '{0:x2}' -f $i++ }
        $i = 0x2c; foreach ($c in 'zxcvbnm'.ToCharArray()) { $map["$c"] = '{0:x2}' -f $i++ }
        $i = 0x02; foreach ($c in '1234567890'.ToCharArray()) { $map["$c"] = '{0:x2}' -f $i++ }
        for ($f = 1; $f -le 10; $f++) { $map["f$f"] = '{0:x2}' -f (0x3a + $f) }
        $keys = $Arg.Split('+') | ForEach-Object { $map[$_.ToLower()] ?? $(throw "unknown key $_") }
        $break = { param($k) ($k.Split(' ') | ForEach-Object { if ($_ -eq 'e0') { 'e0' } else { '{0:x2}' -f ([Convert]::ToInt32($_, 16) -bor 0x80) } }) -join ' ' }
        foreach ($k in $keys) { & $vb controlvm $vm keyboardputscancode $k.Split(' ') }
        Start-Sleep -Milliseconds $HoldMs
        [array]::Reverse($keys)
        foreach ($k in $keys) { & $vb controlvm $vm keyboardputscancode (& $break $k).Split(' ') }
    }
    'type' { & $vb controlvm $vm keyboardputstring $Arg }
}
