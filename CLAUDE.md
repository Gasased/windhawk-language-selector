# windhawk-language-selector

Windhawk mod that makes Windows 11 language switching behave like macOS: the
language panel appears next to the text cursor instead of the taskbar corner.
Everything about it (placement, size, timing, animation, colors, list
direction) is a mod setting.

## Layout

- `mods/mac-style-language-popup.wh.cpp` — the mod. Single file, Windhawk format
  (metadata / readme / settings blocks at the top). Settings block must stay
  Windhawk **1.7.3-compatible**: only `$name`, `$description`, `$options`
  (no 2.0 `$format`/`$min`/`$showIf`) — the user's PC runs 1.7.3. YAML gotcha:
  a value can't start with `"` unless the whole value is quoted.
- `tools/vm.ps1` — host-side VM control: `run`, `gc`, `deploy`, `shot`, `keys`, `type`, `click`, `scroll`
  (mouse goes through the agent; screenshot pixels = click pixels).
- `tools/vm-agent.ps1` — runs inside the VM at logon, executes scripts the host drops in `vm-share/inbox`.
- `tools/vm-dbgview.ps1` — in-VM OutputDebugString listener → `vm-share/debug.log` (mod `Wh_Log` output).
- `tools/vm-bootstrap.ps1` — one-time guest setup (UAC off, autologon, agent at logon).
- `tools/playground.ps1` — test matrix in the VM: layouts × only-new-language × scenarios (single, repeat,
  rapid, hold, althold, spoiled, winspace) × 2/3/5 languages × 100/150% DPI. Per case: frame burst, contact
  sheet, `report.txt` (key timeline, panels found per frame by diff, mod log with guest ms timestamps)
  → `shots/pg/<run>/`. Measure latency from the log (`chord up` → `PlacePanel`), not from frames (~3/s).
- `tools/modes.ps1` — screenshots of every display mode (light + dark) and the no-cursor options → `shots/modes-*.png`;
  `-Set parts` → `shots/parts.png` (parts on/off, custom spacing).
  Runs the playground sequentially: never run two playground jobs at once (one VM; results get mixed).
- `tools/vm-recover.ps1` — one command back to "agent answers": starts the VM, restarts the agent, or hard-resets a
  frozen guest (it freezes now and then under VirtualBox-on-Hyper-V: clock stops, guestcontrol can't start).
- `tools/gallery.ps1` — crops the panel out of playground cases into one labelled image.
- `tools/check.ps1` — compile-only check with the host's Windhawk clang (same flags as Windhawk; it is the
  1.7.3 install, so passing = builds on the user's PC). Also flags unquoted `: ` in settings values, which
  makes Windhawk reject the whole settings block.
- `README.md` + `assets/` — user-facing README (reviewed by a critic agent for accuracy against the code: keep
  settings tables in sync with the settings block) and its marketing kit: `demo.gif` (hero), `demo-mac.gif`,
  `styles.png` (drawn by `tools/make-setting-gifs.py styles`). `tools/gif-poster.py`
  puts a panel-visible frame first (static previews show frame 0).
  `assets/settings/*.gif` (one per settings group, embedded in the README's Settings section) are not recordings:
  `tools/make-setting-gifs.py` redraws the panel with Pillow from the mod's geometry (row height, paddings, accent
  mark, PlaceNear, Render's slide/zoom/fade) and tweens setting values. Keep it in sync when panel drawing changes.
  `python tools/make-setting-gifs.py [scene]`; `STILL=<sec>` writes one PNG per scene for checking.
  GIFs come from VirtualBox screen recording: `VBoxManage controlvm win11-langbar recording videores 1600 900`
  (two numbers; at the default 1024x768 the video is garbled), `recording filename <path>.webm`, `recording start`
  / `stop` (`on` only enables). The webm is TV-range: stretch levels when converting
  (`colorlevels=rimin=0.0627:...:rimax=0.9216`) or it comes out grey. The VM has `nativeNames=1` set. VM languages: English, Ukrainian (Enhanced), Japanese (Microsoft IME; Russian was replaced).
- `vm-share/` — scratch exchange dir (inbox/outbox, installers, test pages). Not source.
- `shots/` — VM screenshots.

## How the mod works

Injected into `explorer.exe` (one instance via a named mutex). Two parts, plus a tiny third in apps that
draw their own caret (`@include Zed.exe`): there it only hooks `ImmSetCompositionWindow`/`ImmSetCandidateWindow`
and stores the IME point as a window property (`MacStyleLanguagePopup.Caret`, read cross-process by explorer).

**A. Windows' own panel (Win+Space, holding Alt+Shift).** `InputSwitch.dll` (in
explorer) shows it as a top-level `Shell_InputSwitchTopLevelWindow` on its own
thread and only moves it via `SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0,
SWP_NOSIZE|SWP_NOACTIVATE)` before drawing. The mod hooks `SetWindowPos` and
rewrites x/y (setting `windowsSwitcher`: `hide` (default) = -32000, `caret` = next to the cursor,
`default` = untouched). In `hide` mode the mod's panel mirrors the switcher's selection, read via UIA
(`InputSwitcherItem_<n>` IsSelected, n = Settings language order) on a separate **MTA thread with UIA
timeouts** — querying it from the popup thread deadlocked explorer's switcher. The hook also posts
`WM_APP_FLYOUT_SHOWN` so the panel shows the predicted next language immediately. The caret lookup is delegated to the popup thread
with `SendMessageTimeout` (300 ms), never done on the flyout's thread.

**B. Own popup, for switches without Windows' panel (Alt+Shift / Ctrl+Shift tap,
or when `windowsSwitcher=hide`).** Dedicated thread + layered window:

1. **Detect a switch**: 50 ms timer reads `GetKeyboardLayout` of the focused
   thread (UWP: the `Windows.UI.Core.CoreWindow` thread). A changed HKL counts as a
   user switch only if the foreground window did not change in the last 300 ms
   (else it's per-app layout / activation sync), and not if Windows' panel was
   visible in the last 500 ms (it already showed this switch).
   **Chord watch** (`WatchChord`, `GetAsyncKeyState`, 10 ms while held): the language hotkey
   (`HKCU\Keyboard Layout\Toggle`) released with no other key → show the predicted next layout at once;
   held ≥250 ms → preview. Another key during the chord (Shift+Alt+F) cancels; a prediction with no real
   switch within 400 ms of release fades out. Hides `hideDelay` (300 ms) after the last show / key release;
   `hideOnTyping` (`WatchTyping`, 50 ms poll): any new non-modifier key press while visible hides it. The hide timer also waits while a hotkey key (Shift / Alt or Ctrl) is held: Shift held + Alt taps, or Alt held while Windows' switcher opens ~600 ms later, would otherwise fade and re-show. While visible the panel keeps its anchor (no re-lookup, no drift);
   only-new-language mode is sized for the widest language.
2. **Find the caret** (physical px): Win32 caret via `GetGUIThreadInfo` (DPI-mapped)
   (a tiny caret, Qt's invisible 2x2, is stretched to a 20 DIP line) → IME point property (Zed: no caret, no UIA,
   but it sets the IME position on every caret move) → (setting `rememberCaret`, off by default) last Win32 caret Poll saw in this top-level window, if the
   window hasn't moved (Qt/Telegram destroy the caret on focus loss and recreate it only on click/key) → MSAA `OBJID_CARET` (Chromium/Electron/Firefox) → UIA `TextPattern2::GetCaretRange`
   / `TextPattern::GetSelection` (WinUI/WPF/UWP). No caret: `noCursorMouse` → panel at the pointer; else no
   anchor, and with `noCursorClassic` the hook leaves Windows' switcher in place (asks the popup thread via
   `WM_APP_HAS_ANCHOR`, 300 ms; timeout = hide). Games/full-screen get no special handling.
   `FindAnchor` + `PlaceNear` are shared by A and B.
3. **Display**: `orientation` × `onlyNewLanguage` (one entry, sized for the widest) × parts`showAbbreviation` /
   `showLanguageName` / `showLayoutName` (code only = compact equal cells, `g_s.codesOnly` is derived; a layout
   without the name takes the name's line). Spacing settings (DIPs): `panelPadding`, `itemPaddingX/Y`, `columnGap`,
   `itemSpacing`; row height = 2·paddingY + text lines (20, +2+16 for the layout line).
   **Render**: Direct2D DC render target → 32-bit DIB → `UpdateLayeredWindow`
   (per-pixel alpha). Animation = 8 ms timer re-rendering with an eased progress
   value (fade alpha, slide offset, zoom transform).
4. Names come from the user's language list
   (`HKCU\Control Panel\International\User Profile\Languages`, also the display
   order); abbreviation = first 3 letters of the native name uppercased
   (ENG/УКР/РУС, same as Windows); layout name from
   `HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layouts\<KLID>`.

Verified in the VM: Notepad (Win32 caret), Edge page + address bar (MSAA),
Settings search (UIA), no-caret fallback, Win+Space native panel at caret,
`hide` + compact mode, live settings changes.

## Test VM

- VirtualBox VM `win11-langbar` (F:\VMs), Windows 11 Pro 26H2 (26300) en-US, user `claude` / `claude`,
  UAC off, autologon. Languages: English (US), Ukrainian (Enhanced layout), Japanese (Microsoft IME, Language.Basic FOD).
  Windhawk 2.0.0-alpha.7 (has `windhawk-cli.exe`).
- **2 vCPUs** — with 4 the VM livelocked under VirtualBox-on-Hyper-V. Display 1600x900 via `setvideomodehint`.
- Snapshots: `clean-windhawk` (no mods) and `mod-v0.1` (mod installed). Restore:
  `VBoxManage controlvm win11-langbar poweroff; VBoxManage snapshot win11-langbar restore clean-windhawk; VBoxManage startvm win11-langbar`
- Project dir is shared into the guest as `\\VBoxSvr\project`.
- GUI-level VM interaction (clicking through installers, eyeballing screens) is
  delegated to a **Haiku subagent** (`model: haiku`) that drives `tools/vm.ps1`.
  CLI work (`vm.ps1 run`) is done directly.

## Dev loop

```powershell
.\tools\check.ps1                     # compile check on host (fast)
.\tools\vm.ps1 deploy                 # install the mod in the VM, logging on, starts dbgview
.\tools\vm.ps1 run '(New-Object -ComObject WScript.Shell).AppActivate((Get-Process notepad | ? MainWindowTitle)[0].Id)'
.\tools\vm.ps1 keys alt+shift         # switch language (tap) -> own popup
& VBoxManage controlvm win11-langbar keyboardputscancode e0 5b 39 b9   # Win down + Space; release: e0 db
.\tools\vm.ps1 shot test              # -> shots\test.png, then Read it
Get-Content vm-share\debug.log -Tail 20
.\tools\vm.ps1 run '& "C:\Program Files\Windhawk\windhawk-cli.exe" mod settings set local@mac-style-language-popup mode=compact'
```

The popup is visible ~1.1 s and the VM is slow: take a burst of screenshots
(every 200-300 ms) right after the key press instead of one.

## Gotchas

- mingw: no `min`/`max` macros in C++ — the mod uses `std::min/max`.
- Locally installed mods get the id `local@<id>` in `windhawk-cli`.
- Windhawk 2.0 offline installer with `/S` skips the compiler. It was extracted
  with 7-Zip (`Compiler\*`) into `C:\Program Files\Windhawk\Compiler` and
  `CompilerPath=Compiler` set in `windhawk.ini`.
- `VBoxManage guestcontrol` (`vm.ps1 gc`) runs with a filtered token and can't
  touch HKLM or kill the agent's processes; use it only when the agent is down.
- The agent runs every job in a fresh `powershell.exe` writing to a file (no pipes:
  a job that spawns long-lived processes would otherwise hang the agent) and kills
  jobs after 600 s. Guest is Windows PowerShell 5.1 (no `??`, `?.`).
- `Get-ChildItem -Filter` returns nothing on VBox shared folders.
- Injected keystrokes (`vm.ps1 type`) follow the guest's active layout — switch back
  to ENG before typing commands.
- `AppActivate` from the agent can fail (foreground lock); launching via Win+R gets focus.
- If the agent is wedged: Win+R → `cmd /c taskkill /f /im powershell.exe & wscript "%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\vm-agent.vbs"`.
- Chromium can report a stale `OBJID_CARET` position after navigation (popup lands at
  the old caret spot) — known, not fixed.
