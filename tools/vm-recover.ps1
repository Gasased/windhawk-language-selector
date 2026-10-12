# Gets the VM to "agent answers": starts it if off; if running but the agent is silent, restarts the agent
# (guest alive) or hard-resets the VM (guest frozen: guestcontrol can't even start a session).
#   .\tools\vm-recover.ps1
$ErrorActionPreference = 'Stop'
$vb = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
$vmName = 'win11-langbar'
$proj = Split-Path $PSScriptRoot
$vm = "$PSScriptRoot\vm.ps1"
function Ping([int]$t) {
    Remove-Item "$proj\vm-share\inbox\*.ps1", "$proj\vm-share\outbox\*.txt" -ErrorAction SilentlyContinue  # stale jobs
    try { return [bool](& $vm run '"pong"' -Timeout $t | Select-String pong) } catch { return $false }
}
$state = (& $vb showvminfo $vmName --machinereadable | Select-String '^VMState=').ToString()
"VM: $state"
if ($state -notmatch '"running"') {
    & $vb startvm $vmName | Out-Host
} elseif (-not (Ping 45)) {
    $gc = & $vm gc '"alive"' -Timeout 60 2>&1 | Out-String
    if ($gc -match 'alive') {
        'guest alive, agent down: restarting the agent'
        & $vm gc 'Get-CimInstance Win32_Process -Filter "name=''powershell.exe''" | ? CommandLine -match ''vm-agent'' | % { "old agent still there: $($_.ProcessId)" }; Start-ScheduledTask -TaskName vmagent' -Timeout 60 2>&1 | Out-Host
    } else {
        "guest not answering guestcontrol either ($($gc.Trim())): hard reset"
        & $vb controlvm $vmName reset
    }
}
for ($i = 0; $i -lt 8; $i++) {
    if (Ping 45) { 'agent ok'; exit 0 }
    "waiting for the agent ($($i + 1)/8)"
}
'agent still down after ~6 min'
exit 1
