# One-time guest setup. Needs a full admin token: guestcontrol's token is filtered, so run it
# from the agent with  Start-Process powershell -Verb RunAs ...  and accept UAC (vm.ps1 keys left, enter).
# Reboot afterwards.
$ErrorActionPreference = 'Continue'
$sys = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System'
# UAC off: the agent (and everything it starts) gets a full admin token
Set-ItemProperty $sys EnableLUA 0
Set-ItemProperty $sys ConsentPromptBehaviorAdmin 0
# autologon
$wl = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon'
Set-ItemProperty $wl AutoAdminLogon '1'
Set-ItemProperty $wl DefaultUserName 'claude'
Set-ItemProperty $wl DefaultPassword 'claude'
Remove-ItemProperty $wl AutoLogonCount -ErrorAction SilentlyContinue
# never sleep / lock
powercfg /change monitor-timeout-ac 0
powercfg /change standby-timeout-ac 0
New-Item 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\Personalization' -Force | Out-Null
Set-ItemProperty 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\Personalization' NoLockScreen 1
# agent at logon, on the interactive desktop
$startup = 'C:\Users\claude\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup'
New-Item -ItemType Directory -Force $startup | Out-Null
Set-Content "$startup\vm-agent.vbs" @'
CreateObject("WScript.Shell").Run "powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \\VBoxSvr\project\tools\vm-agent.ps1", 0, False
'@
'bootstrap done'
