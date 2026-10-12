# Runs inside the VM (user session, started from Startup folder).
# Executes every inbox\*.ps1 dropped by the host, writes outbox\<name>.txt.
# ponytail: 300ms file polling over the VBox shared folder; good enough for a dev loop.
$root = '\\VBoxSvr\project\vm-share'
$inbox = "$root\inbox"; $outbox = "$root\outbox"
New-Item -ItemType Directory -Force $inbox, $outbox | Out-Null
while ($true) {
    # -Filter returns nothing on VBox shared folders; filter in PowerShell instead
    foreach ($f in Get-ChildItem $inbox -ErrorAction SilentlyContinue | Where-Object Extension -eq '.ps1') {
        # Each job runs in a fresh process (no shared Add-Type types / variables) and
        # writes its output to a file. Waiting on a pipe instead would hang whenever the
        # job starts a long-lived process that inherits the pipe (windhawk-cli does).
        $job = "$env:TEMP\$($f.Name)"
        $log = "$env:TEMP\$($f.BaseName).out"
        Move-Item $f.FullName $job -Force
        $p = Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList `
            "-NoProfile -ExecutionPolicy Bypass -Command & { & '$job' *>&1 | Out-String -Width 300 | Set-Content -Encoding UTF8 '$log' }"
        if (-not $p.WaitForExit(600000)) { taskkill /PID $p.Id /T /F | Out-Null; Add-Content $log 'AGENT: job killed after 600 s' }
        $out = if (Test-Path $log) { Get-Content -Raw $log } else { '' }
        Remove-Item $job, $log -Force -ErrorAction SilentlyContinue
        # write .tmp then rename so the host never reads a half-written file
        Set-Content -Path "$outbox\$($f.BaseName).tmp" -Value $out -Encoding UTF8
        Move-Item "$outbox\$($f.BaseName).tmp" "$outbox\$($f.BaseName).txt" -Force
    }
    Start-Sleep -Milliseconds 300
}
